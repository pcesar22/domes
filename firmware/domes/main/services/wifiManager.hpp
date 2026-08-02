#pragma once

/**
 * @file wifiManager.hpp
 * @brief WiFi connection manager implementation
 *
 * Provides WiFi station mode with:
 * - Credential storage in NVS
 * - Automatic reconnection with exponential backoff
 */

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "interfaces/iConfigStorage.hpp"
#include "utils/mutex.hpp"

#include <atomic>

namespace domes {

enum class WifiState : uint8_t {
    kDisconnected,
    kConnecting,
    kConnected,
    kGotIp,
    kError,
};

/**
 * @brief NVS namespace and keys for WiFi credentials
 */
namespace wifi_nvs {
constexpr const char* kNamespace = "wifi";
constexpr const char* kSsid = "ssid";
constexpr const char* kPassword = "pass";
}  // namespace wifi_nvs

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
 * if (wifi.hasStoredCredentials()) wifi.connect();
 * @endcode
 */
class WifiManager {
public:
    /**
     * @brief Construct WiFi manager
     *
     * @param config NVS configuration storage for credentials
     */
    explicit WifiManager(IConfigStorage& config);

    ~WifiManager();

    // Non-copyable
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    esp_err_t init();
    esp_err_t deinit();
    esp_err_t connect();
    esp_err_t connect(const char* ssid, const char* password, bool saveCredentials = true);
    esp_err_t disconnect();
    bool isConnected() const;
    esp_err_t getIpAddress(char* ipOut, size_t len) const;
    int8_t getRssi() const;
    bool hasStoredCredentials() const;

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
    static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId,
                                 void* eventData);

    /**
     * @brief IP event handler
     */
    static void ipEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId,
                               void* eventData);

    /**
     * @brief Handle WiFi disconnection with retry logic
     */
    void handleDisconnect();

    /** Connect while connectionMutex_ is held. */
    esp_err_t connectLocked(const char* ssid, const char* password, bool saveCredentials);

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
    std::atomic<bool> initialized_;
    std::atomic<bool> clientEnabled_;

    esp_event_handler_instance_t wifiEventInstance_;
    esp_event_handler_instance_t ipEventInstance_;
    uint8_t retryCount_;
    uint32_t currentBackoffMs_;
    uint32_t connectionGeneration_;
    utils::Mutex connectionMutex_;

    esp_ip4_addr_t ipAddress_;

    static constexpr uint32_t kInitialBackoffMs = 1000;
    static constexpr uint32_t kMaxBackoffMs = 30000;
    static constexpr uint8_t kMaxRetries = 10;
};

}  // namespace domes
