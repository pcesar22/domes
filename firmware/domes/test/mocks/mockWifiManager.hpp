#pragma once

/**
 * @file mockWifiManager.hpp
 * @brief Mock WiFi manager for unit testing
 *
 * Provides controllable WiFi behavior for testing services
 * that depend on WiFi connectivity.
 */

#include "interfaces/iWifiManager.hpp"

#include <cstring>

namespace domes {

/**
 * @brief Mock WiFi manager for unit testing
 *
 * Allows tests to control WiFi state and verify method calls.
 *
 * @code
 * MockWifiManager mockWifi;
 * mockWifi.connectSuccess = false;  // Simulate connection failure
 *
 * SomeService service(mockWifi);
 * service.doSomething();
 *
 * TEST_ASSERT_TRUE(mockWifi.connectCalled);
 * @endcode
 */
class MockWifiManager : public IWifiManager {
public:
    MockWifiManager() {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        initCalled = false;
        deinitCalled = false;
        connectCalled = false;
        disconnectCalled = false;
        clearCredentialsCalled = false;
        smartConfigStarted = false;

        initReturnValue = ESP_OK;
        connectReturnValue = ESP_OK;
        connectSuccess = true;
        hasCredentials = true;

        state_ = WifiState::kDisconnected;
        mockRssi = -50;
        std::strcpy(mockIpAddress, "192.168.1.100");
        std::strcpy(mockSsid, "TestNetwork");
        lastSsid[0] = '\0';
        lastPassword[0] = '\0';
    }

    // IWifiManager implementation
    esp_err_t init() override {
        initCalled = true;
        if (initReturnValue == ESP_OK) {
            initialized_ = true;
        }
        return initReturnValue;
    }

    esp_err_t deinit() override {
        deinitCalled = true;
        initialized_ = false;
        state_ = WifiState::kDisconnected;
        return ESP_OK;
    }

    esp_err_t connect() override {
        connectCalled = true;
        if (!hasCredentials) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        return simulateConnect();
    }

    esp_err_t connect(const char* ssid, const char* password,
                      bool saveCredentials) override {
        connectCalled = true;
        if (ssid) {
            std::strncpy(lastSsid, ssid, sizeof(lastSsid) - 1);
        }
        if (password) {
            std::strncpy(lastPassword, password, sizeof(lastPassword) - 1);
        }
        if (saveCredentials) {
            hasCredentials = true;
        }
        return simulateConnect();
    }

    esp_err_t disconnect() override {
        disconnectCalled = true;
        state_ = WifiState::kDisconnected;
        if (eventCallback_) {
            eventCallback_(WifiEvent::kDisconnected);
        }
        return ESP_OK;
    }

    bool isConnected() const override {
        return state_ == WifiState::kGotIp;
    }

    WifiState getState() const override {
        return state_;
    }

    esp_err_t getIpAddress(char* ipOut, size_t len) const override {
        if (!isConnected()) {
            return ESP_ERR_WIFI_NOT_CONNECT;
        }
        if (!ipOut || len < 16) {
            return ESP_ERR_INVALID_ARG;
        }
        std::strncpy(ipOut, mockIpAddress, len - 1);
        ipOut[len - 1] = '\0';
        return ESP_OK;
    }

    int8_t getRssi() const override {
        return isConnected() ? mockRssi : 0;
    }

    bool hasStoredCredentials() const override {
        return hasCredentials;
    }

    esp_err_t clearCredentials() override {
        clearCredentialsCalled = true;
        hasCredentials = false;
        return ESP_OK;
    }

    void onEvent(WifiEventCallback callback) override {
        eventCallback_ = callback;
    }

    esp_err_t startSmartConfig(uint32_t timeoutMs) override {
        smartConfigStarted = true;
        smartConfigTimeout_ = timeoutMs;
        return ESP_OK;
    }

    void stopSmartConfig() override {
        smartConfigStarted = false;
    }

    bool isSmartConfigActive() const override {
        return smartConfigStarted;
    }

    esp_err_t getConnectedSsid(char* ssidOut, size_t len) const override {
        if (!isConnected()) {
            return ESP_ERR_WIFI_NOT_CONNECT;
        }
        if (!ssidOut || len < 33) {
            return ESP_ERR_INVALID_ARG;
        }
        std::strncpy(ssidOut, mockSsid, len - 1);
        ssidOut[len - 1] = '\0';
        return ESP_OK;
    }

    // Test control methods

    /**
     * @brief Simulate connection success/failure
     */
    esp_err_t simulateConnect() {
        if (connectReturnValue != ESP_OK) {
            state_ = WifiState::kError;
            if (eventCallback_) {
                eventCallback_(WifiEvent::kConnectionFailed);
            }
            return connectReturnValue;
        }

        state_ = WifiState::kConnecting;
        if (eventCallback_) {
            eventCallback_(WifiEvent::kStarted);
        }

        if (connectSuccess) {
            state_ = WifiState::kConnected;
            if (eventCallback_) {
                eventCallback_(WifiEvent::kConnected);
            }
            state_ = WifiState::kGotIp;
            if (eventCallback_) {
                eventCallback_(WifiEvent::kGotIp);
            }
        } else {
            state_ = WifiState::kError;
            if (eventCallback_) {
                eventCallback_(WifiEvent::kConnectionFailed);
            }
        }

        return ESP_OK;
    }

    /**
     * @brief Manually set WiFi state for testing
     */
    void setState(WifiState state) {
        state_ = state;
    }

    /**
     * @brief Trigger an event callback
     */
    void triggerEvent(WifiEvent event) {
        if (eventCallback_) {
            eventCallback_(event);
        }
    }

    // Test inspection - method calls
    bool initCalled;
    bool deinitCalled;
    bool connectCalled;
    bool disconnectCalled;
    bool clearCredentialsCalled;
    bool smartConfigStarted;

    // Test control - return values
    esp_err_t initReturnValue;
    esp_err_t connectReturnValue;
    bool connectSuccess;
    bool hasCredentials;

    // Test control - mock data
    int8_t mockRssi;
    char mockIpAddress[16];
    char mockSsid[33];

    // Test inspection - captured arguments
    char lastSsid[33];
    char lastPassword[65];

private:
    WifiState state_ = WifiState::kDisconnected;
    bool initialized_ = false;
    WifiEventCallback eventCallback_;
    uint32_t smartConfigTimeout_ = 0;
};

}  // namespace domes
