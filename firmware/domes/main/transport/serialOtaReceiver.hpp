#pragma once

/**
 * @file serialOtaReceiver.hpp
 * @brief OTA receiver task for serial transport
 *
 * Listens for OTA protocol messages on a transport (USB-CDC or UART)
 * and handles firmware updates using the ESP32 OTA APIs. Also handles
 * trace protocol commands for performance profiling.
 */

#include "config/configCommandHandler.hpp"
#include "config/featureManager.hpp"
#include "config/modeManager.hpp"
#include "esp_ota_ops.h"
#include "interfaces/iTaskRunner.hpp"
#include "interfaces/iTransport.hpp"
#include "trace/traceCommandHandler.hpp"

#include <atomic>
#include <memory>

namespace domes {

class LedService;  // Forward declaration
class ImuService;  // Forward declaration

/**
 * @brief FreeRTOS task that receives OTA updates via serial transport
 *
 * Implements the device side of the OTA protocol:
 * - Receives OTA_BEGIN → prepares OTA partition
 * - Receives OTA_DATA → writes chunks to flash
 * - Receives OTA_END → verifies and sets boot partition
 * - Sends OTA_ACK/ABORT responses
 *
 * Usage:
 * @code
 * UsbCdcTransport transport;
 * transport.init();
 *
 * SerialOtaReceiver receiver(transport);
 * TaskManager::createTask(receiver, "ota_rx", 4096, 5);
 * @endcode
 */
class SerialOtaReceiver : public ITaskRunner {
public:
    /**
     * @brief Construct OTA receiver
     *
     * @param transport Transport to receive OTA data on
     * @param features Feature manager for runtime config (optional)
     */
    explicit SerialOtaReceiver(ITransport& transport,
                                config::FeatureManager* features = nullptr);

    ~SerialOtaReceiver() override = default;

    // Non-copyable
    SerialOtaReceiver(const SerialOtaReceiver&) = delete;
    SerialOtaReceiver& operator=(const SerialOtaReceiver&) = delete;

    // ITaskRunner implementation
    void run() override;
    esp_err_t requestStop() override;
    bool shouldRun() const override;

    /**
     * @brief Check if OTA is currently in progress
     */
    bool isOtaInProgress() const { return otaInProgress_.load(); }

    /**
     * @brief Get bytes received so far
     */
    size_t getBytesReceived() const { return bytesReceived_; }

    /**
     * @brief Get expected firmware size
     */
    size_t getFirmwareSize() const { return firmwareSize_; }

    /**
     * @brief Set LED service for pattern commands
     *
     * @param ledService LED service instance
     */
    void setLedService(LedService* ledService) {
        if (configHandler_) {
            configHandler_->setLedService(ledService);
        }
    }

    /**
     * @brief Set IMU service for triage commands
     *
     * @param imuService IMU service instance
     */
    void setImuService(ImuService* imuService) {
        if (configHandler_) {
            configHandler_->setImuService(imuService);
        }
    }

    /**
     * @brief Set mode manager for system mode commands
     *
     * @param modeManager Mode manager instance
     */
    void setModeManager(config::ModeManager* modeManager) {
        if (configHandler_) {
            configHandler_->setModeManager(modeManager);
        }
    }

private:
    /**
     * @brief Handle OTA_BEGIN message
     */
    void handleOtaBegin(const uint8_t* payload, size_t len);

    /**
     * @brief Handle OTA_DATA message
     */
    void handleOtaData(const uint8_t* payload, size_t len);

    /**
     * @brief Handle OTA_END message
     */
    void handleOtaEnd();

    /**
     * @brief Send ACK response
     */
    void sendAck(OtaStatus status, uint32_t nextOffset);

    /**
     * @brief Send ABORT response and cleanup
     */
    void sendAbortAndCleanup(OtaStatus reason);

    /**
     * @brief Cleanup OTA state
     */
    void cleanupOta();

    ITransport& transport_;
    std::atomic<bool> stopRequested_;
    std::atomic<bool> otaInProgress_;

    // Trace command handler
    std::unique_ptr<trace::CommandHandler> traceHandler_;

    // Config command handler (nullptr if feature manager not provided)
    std::unique_ptr<config::ConfigCommandHandler> configHandler_;

    // OTA state
    esp_ota_handle_t otaHandle_;
    const esp_partition_t* updatePartition_;
    size_t firmwareSize_;
    size_t bytesReceived_;
    uint32_t expectedOffset_;
    uint8_t expectedSha256_[32];
};

}  // namespace domes
