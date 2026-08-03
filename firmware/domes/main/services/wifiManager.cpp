/**
 * @file wifiManager.cpp
 * @brief WiFi connection manager implementation
 */

#include "wifiManager.hpp"

#include "esp_log.h"
#include "infra/logging.hpp"
#include "nvs_flash.h"

#include <algorithm>
#include <cstring>

namespace domes {

namespace {
constexpr const char* kTag = "wifi";
}

WifiManager::WifiManager(IConfigStorage& config)
    : config_(config),
      staNetif_(nullptr),
      state_(WifiState::kDisconnected),
      initialized_(false),
      clientEnabled_(false),
      wifiEventInstance_(nullptr),
      ipEventInstance_(nullptr),
      retryCount_(0),
      currentBackoffMs_(kInitialBackoffMs),
      connectionGeneration_(0),
      ipAddress_{} {}

WifiManager::~WifiManager() {
    if (initialized_) {
        deinit();
    }
}

esp_err_t WifiManager::init() {
    if (initialized_) {
        ESP_LOGW(kTag, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Initializing WiFi manager");

    // Initialize TCP/IP stack
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create default event loop if not exists
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create default WiFi station
    staNetif_ = esp_netif_create_default_wifi_sta();
    if (!staNetif_) {
        ESP_LOGE(kTag, "Failed to create default WiFi STA");
        return ESP_FAIL;
    }

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Register event handlers
    err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::wifiEventHandler, this, &wifiEventInstance_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::ipEventHandler, this, &ipEventInstance_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register IP event handler: %s", esp_err_to_name(err));
        return err;
    }

    // Set WiFi mode to station
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    // Start WiFi
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    initialized_ = true;
    ESP_LOGI(kTag, "WiFi manager initialized");

    return ESP_OK;
}

esp_err_t WifiManager::deinit() {
    if (!initialized_) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Deinitializing WiFi manager");

    disconnect();

    esp_wifi_stop();
    esp_wifi_deinit();

    if (wifiEventInstance_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventInstance_);
        wifiEventInstance_ = nullptr;
    }

    if (ipEventInstance_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ipEventInstance_);
        ipEventInstance_ = nullptr;
    }

    if (staNetif_) {
        esp_netif_destroy_default_wifi(staNetif_);
        staNetif_ = nullptr;
    }

    initialized_ = false;
    state_ = WifiState::kDisconnected;

    ESP_LOGI(kTag, "WiFi manager deinitialized");
    return ESP_OK;
}

esp_err_t WifiManager::connect() {
    utils::MutexGuard guard(connectionMutex_);
    if (!initialized_) {
        ESP_LOGE(kTag, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    clientEnabled_.store(true, std::memory_order_release);
    ++connectionGeneration_;

    char ssid[33] = {0};
    char password[65] = {0};

    esp_err_t err = loadCredentials(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "No stored credentials");
        return err;
    }

    return connectLocked(ssid, password, false);
}

esp_err_t WifiManager::connect(const char* ssid, const char* password, bool shouldSave) {
    utils::MutexGuard guard(connectionMutex_);
    if (!initialized_) {
        ESP_LOGE(kTag, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(kTag, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }

    clientEnabled_.store(true, std::memory_order_release);
    ++connectionGeneration_;
    return connectLocked(ssid, password, shouldSave);
}

esp_err_t WifiManager::connectLocked(const char* ssid, const char* password, bool shouldSave) {
    ESP_LOGI(kTag, "Connecting to: %s", ssid);

    // Configure WiFi
    wifi_config_t wifiConfig = {};
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), ssid, sizeof(wifiConfig.sta.ssid) - 1);

    if (password && strlen(password) > 0) {
        strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), password,
                sizeof(wifiConfig.sta.password) - 1);
    }

    wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifiConfig.sta.pmf_cfg.capable = true;
    wifiConfig.sta.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        return err;
    }

    state_ = WifiState::kConnecting;
    resetBackoff();

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        state_ = WifiState::kError;
        return err;
    }

    if (shouldSave) {
        saveCredentials(ssid, password);
    }

    return ESP_OK;
}

esp_err_t WifiManager::disconnect() {
    esp_err_t err = ESP_OK;
    {
        utils::MutexGuard guard(connectionMutex_);
        clientEnabled_.store(false, std::memory_order_release);
        ++connectionGeneration_;

        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(kTag, "Disconnecting");
        err = esp_wifi_disconnect();
        state_ = WifiState::kDisconnected;
    }

    return err == ESP_ERR_WIFI_NOT_CONNECT ? ESP_OK : err;
}

bool WifiManager::isConnected() const {
    return state_ == WifiState::kGotIp;
}

esp_err_t WifiManager::getIpAddress(char* ipOut, size_t len) const {
    if (!isConnected()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    if (!ipOut || len < 16) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(ipOut, len, IPSTR, IP2STR(&ipAddress_));
    return ESP_OK;
}

int8_t WifiManager::getRssi() const {
    if (!isConnected()) {
        return 0;
    }

    wifi_ap_record_t apInfo;
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
        return apInfo.rssi;
    }

    return 0;
}

bool WifiManager::hasStoredCredentials() const {
    char ssid[33] = {0};
    char password[65] = {0};

    return loadCredentials(ssid, password) == ESP_OK;
}

esp_err_t WifiManager::loadCredentials(char* ssid, char* password) const {
    esp_err_t err = const_cast<IConfigStorage&>(config_).open(wifi_nvs::kNamespace);
    if (err != ESP_OK) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t ssidLen = 33;
    err = config_.getBlob(wifi_nvs::kSsid, ssid, ssidLen);
    if (err != ESP_OK) {
        const_cast<IConfigStorage&>(config_).close();
        return err;
    }

    size_t passLen = 65;
    err = config_.getBlob(wifi_nvs::kPassword, password, passLen);
    if (err != ESP_OK) {
        // Password might be empty for open networks
        password[0] = '\0';
    }

    const_cast<IConfigStorage&>(config_).close();

    if (strlen(ssid) == 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t WifiManager::saveCredentials(const char* ssid, const char* password) {
    ESP_LOGI(kTag, "Saving credentials for: %s", ssid);

    esp_err_t err = config_.open(wifi_nvs::kNamespace);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = config_.setBlob(wifi_nvs::kSsid, ssid, strlen(ssid) + 1);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save SSID: %s", esp_err_to_name(err));
        config_.close();
        return err;
    }

    if (password) {
        err = config_.setBlob(wifi_nvs::kPassword, password, strlen(password) + 1);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to save password: %s", esp_err_to_name(err));
            config_.close();
            return err;
        }
    }

    err = config_.commit();
    config_.close();

    return err;
}

void WifiManager::wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId,
                                   void* eventData) {
    auto* self = static_cast<WifiManager*>(arg);

    switch (eventId) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(kTag, "WiFi STA started");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(kTag, "Connected to AP");
            {
                utils::MutexGuard guard(self->connectionMutex_);
                if (!self->clientEnabled_.load(std::memory_order_acquire)) {
                    esp_wifi_disconnect();
                    break;
                }
                self->state_ = WifiState::kConnected;
                self->resetBackoff();
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* event = static_cast<wifi_event_sta_disconnected_t*>(eventData);
            ESP_LOGW(kTag, "Disconnected from AP, reason: %d", event->reason);
            self->state_ = WifiState::kDisconnected;
            self->ipAddress_ = {};
            self->handleDisconnect();
            break;
        }

        default:
            break;
    }
}

void WifiManager::ipEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId,
                                 void* eventData) {
    auto* self = static_cast<WifiManager*>(arg);

    if (eventId == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(eventData);
        {
            utils::MutexGuard guard(self->connectionMutex_);
            if (!self->clientEnabled_.load(std::memory_order_acquire)) {
                esp_wifi_disconnect();
                return;
            }
            self->ipAddress_ = event->ip_info.ip;
            self->state_ = WifiState::kGotIp;
            self->resetBackoff();
        }

        ESP_LOGI(kTag, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void WifiManager::handleDisconnect() {
    uint32_t delayMs = 0;
    uint32_t retryGeneration = 0;
    uint8_t retryNumber = 0;
    bool retriesExhausted = false;
    {
        utils::MutexGuard guard(connectionMutex_);
        if (!clientEnabled_.load(std::memory_order_acquire)) {
            ESP_LOGI(kTag, "WiFi client disabled; automatic reconnect suppressed");
            return;
        }

        retryCount_++;
        retryNumber = retryCount_;
        if (retryCount_ > kMaxRetries) {
            ESP_LOGE(kTag, "Max retries exceeded");
            state_ = WifiState::kError;
            retriesExhausted = true;
        } else {
            delayMs = getNextBackoffMs();
            retryGeneration = connectionGeneration_;
        }
    }

    if (retriesExhausted) {
        return;
    }

    ESP_LOGI(kTag, "Retry %d/%d in %lu ms", retryNumber, kMaxRetries, delayMs);

    vTaskDelay(pdMS_TO_TICKS(delayMs));

    utils::MutexGuard guard(connectionMutex_);
    if (!clientEnabled_.load(std::memory_order_acquire) ||
        retryGeneration != connectionGeneration_) {
        ESP_LOGI(kTag, "WiFi reconnect canceled by a newer client request");
        return;
    }

    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Automatic reconnect failed: %s", esp_err_to_name(err));
        state_ = WifiState::kError;
    }
}

void WifiManager::resetBackoff() {
    retryCount_ = 0;
    currentBackoffMs_ = kInitialBackoffMs;
}

uint32_t WifiManager::getNextBackoffMs() {
    uint32_t delay = currentBackoffMs_;
    currentBackoffMs_ = std::min(currentBackoffMs_ * 2, kMaxBackoffMs);
    return delay;
}

}  // namespace domes
