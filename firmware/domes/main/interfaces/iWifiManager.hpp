#pragma once

/**
 * @file iWifiManager.hpp
 * @brief Abstract interface for WiFi connection management
 *
 * Defines the contract for WiFi management implementations.
 * Allows mocking for unit tests without hardware.
 */

#include "esp_err.h"

#include <cstdint>
#include <functional>

namespace domes {

/**
 * @brief WiFi connection state
 */
enum class WifiState : uint8_t {
    kDisconnected,  ///< Not connected to any network
    kConnecting,    ///< Connection attempt in progress
    kConnected,     ///< Connected but no IP yet
    kGotIp,         ///< Connected with valid IP address
    kError,         ///< Connection error occurred
};

/**
 * @brief WiFi event types for callbacks
 */
enum class WifiEvent : uint8_t {
    kStarted,           ///< WiFi station started
    kConnected,         ///< Connected to AP
    kDisconnected,      ///< Disconnected from AP
    kGotIp,             ///< IP address acquired
    kLostIp,            ///< IP address lost
    kConnectionFailed,  ///< Connection attempt failed
};

/**
 * @brief WiFi event callback type
 */
using WifiEventCallback = std::function<void(WifiEvent event)>;

/**
 * @brief Abstract interface for WiFi management
 *
 * Provides WiFi connection management with credential storage
 * and reconnection logic. Mockable for unit testing.
 */
class IWifiManager {
public:
    virtual ~IWifiManager() = default;

    /**
     * @brief Initialize WiFi subsystem
     *
     * Initializes the WiFi driver and event handlers.
     * Must be called before any other operations.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitialize WiFi subsystem
     *
     * Disconnects and releases WiFi resources.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Connect using stored credentials
     *
     * Attempts connection using credentials previously saved to NVS.
     *
     * @return ESP_OK if connection attempt started
     * @return ESP_ERR_NVS_NOT_FOUND if no stored credentials
     */
    virtual esp_err_t connect() = 0;

    /**
     * @brief Connect with specific credentials
     *
     * @param ssid WiFi network name (max 32 chars)
     * @param password WiFi password (max 64 chars)
     * @param saveCredentials If true, save credentials to NVS for future use
     * @return ESP_OK if connection attempt started
     */
    virtual esp_err_t connect(const char* ssid, const char* password,
                              bool saveCredentials = true) = 0;

    /**
     * @brief Disconnect from WiFi
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t disconnect() = 0;

    /**
     * @brief Check if WiFi is connected with IP
     *
     * @return true if connected and IP acquired
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get current WiFi state
     *
     * @return Current connection state
     */
    virtual WifiState getState() const = 0;

    /**
     * @brief Get current IP address as string
     *
     * @param ipOut Output buffer for IP string (min 16 bytes recommended)
     * @param len Buffer length
     * @return ESP_OK if connected, ESP_ERR_WIFI_NOT_CONNECT if not connected
     */
    virtual esp_err_t getIpAddress(char* ipOut, size_t len) const = 0;

    /**
     * @brief Get current signal strength
     *
     * @return RSSI in dBm, or 0 if not connected
     */
    virtual int8_t getRssi() const = 0;

    /**
     * @brief Check if credentials are stored in NVS
     *
     * @return true if valid credentials exist
     */
    virtual bool hasStoredCredentials() const = 0;

    /**
     * @brief Clear stored credentials from NVS
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t clearCredentials() = 0;

    /**
     * @brief Register event callback
     *
     * Only one callback can be registered at a time.
     * Pass nullptr to unregister.
     *
     * @param callback Function to call on WiFi events
     */
    virtual void onEvent(WifiEventCallback callback) = 0;

    /**
     * @brief Start SmartConfig provisioning
     *
     * Enables SmartConfig to receive WiFi credentials from
     * a smartphone app (e.g., ESP-Touch).
     *
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return ESP_OK if provisioning started
     */
    virtual esp_err_t startSmartConfig(uint32_t timeoutMs = 60000) = 0;

    /**
     * @brief Stop SmartConfig provisioning
     */
    virtual void stopSmartConfig() = 0;

    /**
     * @brief Check if SmartConfig is active
     *
     * @return true if SmartConfig is running
     */
    virtual bool isSmartConfigActive() const = 0;

    /**
     * @brief Get the SSID of the connected network
     *
     * @param ssidOut Output buffer (min 33 bytes for 32 chars + null)
     * @param len Buffer length
     * @return ESP_OK if connected, error otherwise
     */
    virtual esp_err_t getConnectedSsid(char* ssidOut, size_t len) const = 0;
};

}  // namespace domes
