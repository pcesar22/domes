#pragma once

/**
 * @file bleOtaService.hpp
 * @brief BLE GATT service for OTA updates
 *
 * Implements a BLE peripheral with a custom OTA service that allows
 * a phone or host (via BlueZ) to send firmware updates over Bluetooth.
 *
 * Service UUIDs:
 * - OTA Service: 12345678-1234-5678-1234-56789abcdef0
 * - Data Characteristic: 12345678-1234-5678-1234-56789abcdef1 (Write)
 * - Status Characteristic: 12345678-1234-5678-1234-56789abcdef2 (Notify)
 *
 * Protocol:
 * - Uses the same frame protocol as serial OTA (frameCodec + otaProtocol)
 * - Central writes OTA frames to Data characteristic
 * - Peripheral sends ACK/ABORT via Status notifications
 */

#include "interfaces/iTransport.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>

namespace domes {

/// OTA Service UUID: 12345678-1234-5678-1234-56789abcdef0
extern const uint8_t kOtaServiceUuid[16];

/// OTA Data Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1
extern const uint8_t kOtaDataCharUuid[16];

/// OTA Status Characteristic UUID: 12345678-1234-5678-1234-56789abcdef2
extern const uint8_t kOtaStatusCharUuid[16];

/**
 * @brief BLE OTA Service - GATT server for firmware updates
 *
 * This class manages the BLE stack and exposes a GATT service for OTA.
 * It implements ITransport so it can be used with SerialOtaReceiver.
 */
class BleOtaService : public ITransport {
public:
    /// Maximum BLE MTU (negotiated, typically 512 for BLE 5.0)
    static constexpr size_t kMaxMtu = 512;

    /// Receive buffer size (must hold at least one full frame)
    static constexpr size_t kRxBufferSize = 2048;

    BleOtaService();
    ~BleOtaService() override;

    // Non-copyable
    BleOtaService(const BleOtaService&) = delete;
    BleOtaService& operator=(const BleOtaService&) = delete;

    // =========================================================================
    // ITransport interface
    // =========================================================================

    /**
     * @brief Initialize NimBLE stack and start advertising
     */
    TransportError init() override;

    /**
     * @brief Send data via Status characteristic notification
     */
    TransportError send(const uint8_t* data, size_t len) override;

    /**
     * @brief Receive data written to Data characteristic
     */
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;

    /**
     * @brief Check if a BLE central is connected
     */
    bool isConnected() const override;

    /**
     * @brief Disconnect current client and stop advertising
     */
    void disconnect() override;

    // =========================================================================
    // BLE-specific methods
    // =========================================================================

    /**
     * @brief Start BLE advertising
     */
    void startAdvertising();

    /**
     * @brief Stop BLE advertising
     */
    void stopAdvertising();

    /**
     * @brief Get current negotiated MTU
     */
    uint16_t getMtu() const { return currentMtu_; }

    /**
     * @brief Set device name for advertising
     */
    void setDeviceName(const char* name);

    // =========================================================================
    // Internal callbacks (called from NimBLE stack)
    // =========================================================================

    /// Called when data is written to Data characteristic
    void onDataReceived(const uint8_t* data, size_t len);

    /// Called when connection state changes
    void onConnectionStateChanged(bool connected, uint16_t connHandle);

    /// Called when MTU is negotiated
    void onMtuChanged(uint16_t mtu);

private:
    /// Initialize GATT services
    void initGattServices();

    /// NimBLE task sync
    void* syncSemaphore_ = nullptr;

    /// Ring buffer for received data
    std::array<uint8_t, kRxBufferSize> rxBuffer_{};
    size_t rxHead_ = 0;
    size_t rxTail_ = 0;
    void* rxMutex_ = nullptr;
    void* rxSemaphore_ = nullptr;

    /// Connection state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    uint16_t connHandle_ = 0;
    uint16_t currentMtu_ = 23;  // Default BLE MTU

    /// Characteristic value handles (for notifications)
    uint16_t statusCharHandle_ = 0;
};

}  // namespace domes
