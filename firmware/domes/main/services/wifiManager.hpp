#pragma once

/**
 * @file wifiManager.hpp
 * @brief WiFi connection manager implementation
 *
 * Provides WiFi station mode with:
 * - Credential storage in NVS
 * - Automatic reconnection with exponential backoff
 * - SmartConfig provisioning support
 */

#include "interfaces/iWifiManager.hpp"
#include "interfaces/iConfigStorage.hpp"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include <atomic>

namespace domes {

/**
 * @brief NVS namespace and keys for WiFi credentials
 */
namespace wifi_nvs {
    constexpr const char* kNamespace = "wifi";
    constexpr const char* kSsid      = "ssid";
    constexpr const char* kPassword  = "pass";
}

/**
 * @brief WiFi connection manager implementation
 *
 * Manages WiFi station mode with automatic reconnection.
 * Uses exponential backoff: 1s, 2s, 4s, 8s, max 30s.
 *
 * @note Must be initialized after NVS flash is initialized.
 *
 * @code
 * WifiManager wifi(configStorage);
 * wifi.init();
 *
 * if (wifi.hasStoredCredentials()) {
 *     wifi.connect();
 * } else {
 *     wifi.startSmartConfig();
 * }
 * @endcode
 */
class WifiManager : public IWifiManager {
public:
    /**
     * @brief Construct WiFi manager
     *
     * @param config NVS configuration storage for credentials
     */
    explicit WifiManager(IConfigStorage& config);

    ~WifiManager() override;

    // Non-copyable
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    // IWifiManager implementation
    esp_err_t init() override;
    esp_err_t deinit() override;
    esp_err_t connect() override;
    esp_err_t connect(const char* ssid, const char* password,
                      bool saveCredentials = true) override;
    esp_err_t disconnect() override;
    bool isConnected() const override;
    WifiState getState() const override;
    esp_err_t getIpAddress(char* ipOut, size_t len) const override;
    int8_t getRssi() const override;
    bool hasStoredCredentials() const override;
    esp_err_t clearCredentials() override;
    void onEvent(WifiEventCallback callback) override;
    esp_err_t startSmartConfig(uint32_t timeoutMs = 60000) override;
    void stopSmartConfig() override;
    bool isSmartConfigActive() const override;
    esp_err_t getConnectedSsid(char* ssidOut, size_t len) const override;

private:
    /**
     * @brief Load credentials from NVS
     *
     * @param ssid Output buffer for SSID (min 33 bytes)
     * @param password Output buffer for password (min 65 bytes)
     * @return ESP_OK if credentials found
     */
    esp_err_t loadCredentials(char* ssid, char* password) const;

    /**
     * @brief Save credentials to NVS
     *
     * @param ssid SSID to save
     * @param password Password to save
     * @return ESP_OK on success
     */
    esp_err_t saveCredentials(const char* ssid, const char* password);

    /**
     * @brief WiFi event handler
     */
    static void wifiEventHandler(void* arg, esp_event_base_t eventBase,
                                  int32_t eventId, void* eventData);

    /**
     * @brief IP event handler
     */
    static void ipEventHandler(void* arg, esp_event_base_t eventBase,
                                int32_t eventId, void* eventData);

    /**
     * @brief SmartConfig event handler
     */
    static void smartconfigEventHandler(void* arg, esp_event_base_t eventBase,
                                         int32_t eventId, void* eventData);

    /**
     * @brief Handle WiFi disconnection with retry logic
     */
    void handleDisconnect();

    /**
     * @brief Reset reconnection backoff
     */
    void resetBackoff();

    /**
     * @brief Calculate next backoff delay
     *
     * @return Delay in milliseconds
     */
    uint32_t getNextBackoffMs();

    IConfigStorage& config_;
    esp_netif_t* staNetif_;

    std::atomic<WifiState> state_;
    std::atomic<bool> smartConfigActive_;
    std::atomic<bool> initialized_;

    WifiEventCallback eventCallback_;

    esp_event_handler_instance_t wifiEventInstance_;
    esp_event_handler_instance_t ipEventInstance_;
    esp_event_handler_instance_t scEventInstance_;

    uint8_t retryCount_;
    uint32_t currentBackoffMs_;

    esp_ip4_addr_t ipAddress_;

    static constexpr uint32_t kInitialBackoffMs = 1000;
    static constexpr uint32_t kMaxBackoffMs     = 30000;
    static constexpr uint8_t  kMaxRetries       = 10;
};

}  // namespace domes
