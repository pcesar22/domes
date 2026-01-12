/**
 * @file wifiManager.cpp
 * @brief WiFi connection manager implementation
 */

#include "wifiManager.hpp"

#include "esp_log.h"
#include "esp_smartconfig.h"
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
      smartConfigActive_(false),
      initialized_(false),
      eventCallback_(nullptr),
      wifiEventInstance_(nullptr),
      ipEventInstance_(nullptr),
      scEventInstance_(nullptr),
      retryCount_(0),
      currentBackoffMs_(kInitialBackoffMs),
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

    err = esp_event_handler_instance_register(
        SC_EVENT, ESP_EVENT_ANY_ID, &WifiManager::smartconfigEventHandler, this, &scEventInstance_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register SmartConfig event handler: %s", esp_err_to_name(err));
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

    stopSmartConfig();
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

    if (scEventInstance_) {
        esp_event_handler_instance_unregister(SC_EVENT, ESP_EVENT_ANY_ID, scEventInstance_);
        scEventInstance_ = nullptr;
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
    if (!initialized_) {
        ESP_LOGE(kTag, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    char ssid[33] = {0};
    char password[65] = {0};

    esp_err_t err = loadCredentials(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "No stored credentials");
        return err;
    }

    return connect(ssid, password, false);
}

esp_err_t WifiManager::connect(const char* ssid, const char* password, bool shouldSave) {
    if (!initialized_) {
        ESP_LOGE(kTag, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(kTag, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }

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
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(kTag, "Disconnecting");

    esp_err_t err = esp_wifi_disconnect();
    state_ = WifiState::kDisconnected;

    return err;
}

bool WifiManager::isConnected() const {
    return state_ == WifiState::kGotIp;
}

WifiState WifiManager::getState() const {
    return state_;
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

esp_err_t WifiManager::clearCredentials() {
    ESP_LOGI(kTag, "Clearing stored credentials");

    esp_err_t err = config_.open(wifi_nvs::kNamespace);
    if (err != ESP_OK) {
        return err;
    }

    err = config_.eraseAll();
    if (err == ESP_OK) {
        err = config_.commit();
    }

    config_.close();
    return err;
}

void WifiManager::onEvent(WifiEventCallback callback) {
    eventCallback_ = callback;
}

esp_err_t WifiManager::startSmartConfig(uint32_t timeoutMs) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (smartConfigActive_) {
        ESP_LOGW(kTag, "SmartConfig already active");
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Starting SmartConfig");

    esp_err_t err = esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_smartconfig_set_type failed: %s", esp_err_to_name(err));
        return err;
    }

    smartconfig_start_config_t scConfig = SMARTCONFIG_START_CONFIG_DEFAULT();
    err = esp_smartconfig_start(&scConfig);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_smartconfig_start failed: %s", esp_err_to_name(err));
        return err;
    }

    smartConfigActive_ = true;
    return ESP_OK;
}

void WifiManager::stopSmartConfig() {
    if (smartConfigActive_) {
        ESP_LOGI(kTag, "Stopping SmartConfig");
        esp_smartconfig_stop();
        smartConfigActive_ = false;
    }
}

bool WifiManager::isSmartConfigActive() const {
    return smartConfigActive_;
}

esp_err_t WifiManager::getConnectedSsid(char* ssidOut, size_t len) const {
    if (!isConnected()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    if (!ssidOut || len < 33) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_record_t apInfo;
    esp_err_t err = esp_wifi_sta_get_ap_info(&apInfo);
    if (err != ESP_OK) {
        return err;
    }

    strncpy(ssidOut, reinterpret_cast<const char*>(apInfo.ssid), len - 1);
    ssidOut[len - 1] = '\0';

    return ESP_OK;
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
            if (self->eventCallback_) {
                self->eventCallback_(WifiEvent::kStarted);
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(kTag, "Connected to AP");
            self->state_ = WifiState::kConnected;
            self->resetBackoff();
            if (self->eventCallback_) {
                self->eventCallback_(WifiEvent::kConnected);
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* event = static_cast<wifi_event_sta_disconnected_t*>(eventData);
            ESP_LOGW(kTag, "Disconnected from AP, reason: %d", event->reason);
            self->state_ = WifiState::kDisconnected;
            self->ipAddress_ = {};
            if (self->eventCallback_) {
                self->eventCallback_(WifiEvent::kDisconnected);
            }
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
        self->ipAddress_ = event->ip_info.ip;
        self->state_ = WifiState::kGotIp;
        self->resetBackoff();

        ESP_LOGI(kTag, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (self->eventCallback_) {
            self->eventCallback_(WifiEvent::kGotIp);
        }
    }
}

void WifiManager::smartconfigEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId,
                                          void* eventData) {
    auto* self = static_cast<WifiManager*>(arg);

    switch (eventId) {
        case SC_EVENT_SCAN_DONE:
            ESP_LOGI(kTag, "SmartConfig scan done");
            break;

        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGI(kTag, "SmartConfig found channel");
            break;

        case SC_EVENT_GOT_SSID_PSWD: {
            ESP_LOGI(kTag, "SmartConfig got SSID and password");

            auto* evt = static_cast<smartconfig_event_got_ssid_pswd_t*>(eventData);
            char ssid[33] = {0};
            char password[65] = {0};

            memcpy(ssid, evt->ssid, sizeof(evt->ssid));
            memcpy(password, evt->password, sizeof(evt->password));

            ESP_LOGI(kTag, "SmartConfig SSID: %s", ssid);

            // Save credentials and connect
            self->saveCredentials(ssid, password);
            self->connect(ssid, password, false);
            break;
        }

        case SC_EVENT_SEND_ACK_DONE:
            ESP_LOGI(kTag, "SmartConfig ACK sent");
            self->stopSmartConfig();
            break;

        default:
            break;
    }
}

void WifiManager::handleDisconnect() {
    if (smartConfigActive_) {
        // Don't retry during SmartConfig
        return;
    }

    retryCount_++;
    if (retryCount_ > kMaxRetries) {
        ESP_LOGE(kTag, "Max retries exceeded");
        state_ = WifiState::kError;
        if (eventCallback_) {
            eventCallback_(WifiEvent::kConnectionFailed);
        }
        return;
    }

    uint32_t delayMs = getNextBackoffMs();
    ESP_LOGI(kTag, "Retry %d/%d in %lu ms", retryCount_, kMaxRetries, delayMs);

    // Schedule reconnection
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    esp_wifi_connect();
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
