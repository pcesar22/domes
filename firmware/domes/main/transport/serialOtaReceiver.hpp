#pragma once

/**
 * @file serialOtaReceiver.hpp
 * @brief OTA receiver task for serial transport
 *
 * Listens for OTA protocol messages on a serial byte-stream transport
 * and handles firmware updates using the ESP32 OTA APIs. Also handles
 * trace protocol commands for performance profiling.
 */

#include "config/configCommandHandler.hpp"
#include "config/featureManager.hpp"
#include "config/modeManager.hpp"
#include "esp_ota_ops.h"
#include "interfaces/iTaskRunner.hpp"
#include "interfaces/iTransport.hpp"
#include "mbedtls/sha256.h"
#include "protocol/otaProtocol.hpp"
#include "trace/traceCommandHandler.hpp"

#include <atomic>
#include <memory>

namespace domes {

class LedService;  // Forward declaration
class ImuService;  // Forward declaration
class FeedbackController;

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
 * UartTransport transport(UART_NUM_0, GPIO_NUM_43, GPIO_NUM_44);
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
     * @param podId Pod identity for trace session metadata (0 if unset)
     */
    explicit SerialOtaReceiver(ITransport& transport, config::FeatureManager* features = nullptr,
                               uint8_t podId = 0);

    ~SerialOtaReceiver() override;

    // Non-copyable
    SerialOtaReceiver(const SerialOtaReceiver&) = delete;
    SerialOtaReceiver& operator=(const SerialOtaReceiver&) = delete;

    // ITaskRunner implementation
    void run() override;
    esp_err_t requestStop() override;
    bool shouldRun() const override;

    /** Publish a touch notification on this receiver's transport. */
    bool sendTouchEvent(uint8_t podId, uint8_t padIndex, uint64_t timestampUs) {
        return configHandler_ && configHandler_->sendTouchEvent(podId, padIndex, timestampUs);
    }

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

    /**
     * @brief Set ESP-NOW transport for observability queries
     */
    void setEspNowTransport(EspNowTransport* transport) {
        if (configHandler_) {
            configHandler_->setEspNowTransport(transport);
        }
    }

    /**
     * @brief Set ESP-NOW service for observability queries
     */
    void setEspNowService(EspNowService* service) {
        if (configHandler_) {
            configHandler_->setEspNowService(service);
        }
    }

    /**
     * @brief Set OTA manager for update check commands
     */
    void setOtaManager(IOtaManager* otaManager) {
        if (configHandler_) {
            configHandler_->setOtaManager(otaManager);
        }
    }

    /**
     * @brief Set injectable touch driver for simulated touch commands
     */
    void setInjectableTouchDriver(InjectableTouchDriver* driver) {
        if (configHandler_) {
            configHandler_->setInjectableTouchDriver(driver);
        }
    }

    void setFeedbackController(FeedbackController* controller) {
        if (configHandler_) {
            configHandler_->setFeedbackController(controller);
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
    void handleOtaEnd(const uint8_t* payload, size_t len);

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

    /**
     * @brief Abort an active session after disconnect or prolonged inactivity
     * @return true when the active session was cleaned up
     */
    bool cleanupInterruptedOta();

    ITransport& transport_;
    std::atomic<bool> stopRequested_;
    std::atomic<bool> otaInProgress_;
    bool ownsOtaSession_ = false;

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
    int64_t lastOtaActivityUs_;
    uint8_t expectedSha256_[32];
    char expectedVersion_[kOtaVersionMaxLen];
    mbedtls_sha256_context sha256Context_;
    bool sha256Active_;
};

}  // namespace domes
