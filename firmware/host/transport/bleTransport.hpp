#pragma once

/**
 * @file bleTransport.hpp
 * @brief BLE transport implementation using BlueZ D-Bus API
 *
 * Implements ITransport for BLE connections to ESP32 using BlueZ
 * via the GLib D-Bus interface.
 *
 * OTA Service UUIDs:
 * - Service:     12345678-1234-5678-1234-56789abcdef0
 * - Data Char:   12345678-1234-5678-1234-56789abcdef1 (Write)
 * - Status Char: 12345678-1234-5678-1234-56789abcdef2 (Notify)
 */

#include "interfaces/iTransport.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace domes::host {

/// OTA Service UUID
constexpr const char* kOtaServiceUuid = "12345678-1234-5678-1234-56789abcdef0";

/// OTA Data Characteristic UUID (write)
constexpr const char* kOtaDataCharUuid = "12345678-1234-5678-1234-56789abcdef1";

/// OTA Status Characteristic UUID (notify/read)
constexpr const char* kOtaStatusCharUuid = "12345678-1234-5678-1234-56789abcdef2";

/**
 * @brief BLE transport for Linux using BlueZ
 *
 * Connects to an ESP32 BLE peripheral running the OTA service
 * and provides ITransport interface for firmware updates.
 *
 * Usage:
 * 1. Create BleTransport with target device name or address
 * 2. Call init() to scan, connect, and discover services
 * 3. Use send()/receive() to communicate via GATT characteristics
 */
class BleTransport : public ITransport {
public:
    /**
     * @brief Construct BLE transport
     *
     * @param targetName Device name to scan for (e.g., "DOMES-Pod")
     * @param targetAddress Optional MAC address (if known, skips scan)
     */
    explicit BleTransport(const std::string& targetName,
                          const std::string& targetAddress = "");

    ~BleTransport() override;

    // Non-copyable, non-movable
    BleTransport(const BleTransport&) = delete;
    BleTransport& operator=(const BleTransport&) = delete;

    // =========================================================================
    // ITransport implementation
    // =========================================================================

    /**
     * @brief Initialize BLE: scan, connect, discover OTA service
     */
    TransportError init() override;

    /**
     * @brief Send data via Data characteristic (write without response)
     */
    TransportError send(const uint8_t* data, size_t len) override;

    /**
     * @brief Receive data from Status characteristic notifications
     */
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;

    /**
     * @brief Check if BLE connection is active
     */
    bool isConnected() const override;

    /**
     * @brief Disconnect from BLE device
     */
    void disconnect() override;

    // =========================================================================
    // BLE-specific methods
    // =========================================================================

    /**
     * @brief Scan for devices with OTA service (timeout in seconds)
     * @return Device address if found, empty string if not
     */
    std::string scanForDevice(int timeoutSec = 10);

    /**
     * @brief Get negotiated MTU
     */
    uint16_t getMtu() const { return mtu_; }

    /**
     * @brief Get connected device address
     */
    const std::string& getDeviceAddress() const { return deviceAddress_; }

    // =========================================================================
    // Internal callbacks (called from D-Bus signal handlers)
    // =========================================================================

    /// Called when notification received on Status characteristic
    void onNotification(const uint8_t* data, size_t len);

    /// Called when connection state changes
    void onConnectionChanged(bool connected);

private:
    // D-Bus helper methods
    bool initDBus();
    void cleanupDBus();
    bool enableAdapter();
    bool startScan();
    bool stopScan();
    bool connectToDevice();
    bool discoverServices();
    bool setupCharacteristics();
    bool enableNotifications();

    // Configuration
    std::string targetName_;
    std::string targetAddress_;
    std::string deviceAddress_;  // Actual connected device address

    // D-Bus state (opaque pointers to avoid GLib header in public API)
    struct DBusState;
    std::unique_ptr<DBusState> dbus_;

    // GATT object paths
    std::string devicePath_;
    std::string dataCharPath_;
    std::string statusCharPath_;

    // Connection state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    uint16_t mtu_ = 23;  // Default BLE MTU

    // Receive buffer (for notifications)
    std::queue<std::vector<uint8_t>> rxQueue_;
    mutable std::mutex rxMutex_;
    std::condition_variable rxCv_;
};

}  // namespace domes::host
