/**
 * @file serialOtaReceiver.cpp
 * @brief OTA receiver task implementation
 */

#include "serialOtaReceiver.hpp"

#include "infra/diagnostics.hpp"
#include "trace/traceApi.hpp"

#include "config/configProtocol.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocol/frameCodec.hpp"
#include "protocol/otaProtocol.hpp"
#include "trace/traceProtocol.hpp"

#include <array>
#include <cstring>

static const char* TAG = "serial_ota";

namespace domes {

SerialOtaReceiver::SerialOtaReceiver(ITransport& transport,
                                       config::FeatureManager* features)
    : transport_(transport)
    , stopRequested_(false)
    , otaInProgress_(false)
    , traceHandler_(std::make_unique<trace::CommandHandler>(transport))
    , configHandler_(features ? std::make_unique<config::ConfigCommandHandler>(transport, *features) : nullptr)
    , otaHandle_(0)
    , updatePartition_(nullptr)
    , firmwareSize_(0)
    , bytesReceived_(0)
    , expectedOffset_(0) {
    std::memset(expectedSha256_, 0, sizeof(expectedSha256_));
}

void SerialOtaReceiver::run() {
    ESP_LOGI(TAG, "Serial OTA receiver task started");

    FrameDecoder decoder;
    std::array<uint8_t, 256> rxBuf;

    while (shouldRun()) {
        size_t rxLen = rxBuf.size();
        TransportError err = transport_.receive(rxBuf.data(), &rxLen, 100);

        if (err == TransportError::kTimeout) {
            continue;
        }
        if (!isOk(err)) {
            ESP_LOGE(TAG, "Transport receive error: %s", transportErrorToString(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Feed bytes to decoder
        for (size_t i = 0; i < rxLen; ++i) {
            decoder.feedByte(rxBuf[i]);

            if (decoder.isComplete()) {
                TRACE_SCOPE(TRACE_ID("SerialOta.Dispatch"), domes::trace::Category::kTransport);
                uint8_t msgTypeByte = decoder.getType();
                const uint8_t* payload = decoder.getPayload();
                size_t payloadLen = decoder.getPayloadLen();

                // Check if this is a trace command
                if (trace::isTraceMessage(msgTypeByte)) {
                    traceHandler_->handleCommand(msgTypeByte, payload, payloadLen);
                    decoder.reset();
                    continue;
                }

                // Check if this is a config command
                if (config::isConfigMessage(msgTypeByte) && configHandler_) {
                    configHandler_->handleCommand(msgTypeByte, payload, payloadLen);
                    decoder.reset();
                    continue;
                }

                // Handle OTA messages
                auto msgType = static_cast<OtaMsgType>(msgTypeByte);

                switch (msgType) {
                    case OtaMsgType::kBegin:
                        handleOtaBegin(payload, payloadLen);
                        break;

                    case OtaMsgType::kData:
                        handleOtaData(payload, payloadLen);
                        break;

                    case OtaMsgType::kEnd:
                        handleOtaEnd();
                        break;

                    case OtaMsgType::kAbort:
                        ESP_LOGW(TAG, "Received OTA_ABORT from host");
                        cleanupOta();
                        break;

                    default:
                        ESP_LOGW(TAG, "Unknown message type: 0x%02X", msgTypeByte);
                        break;
                }

                decoder.reset();
            } else if (decoder.isError()) {
                TRACE_INSTANT(TRACE_ID("SerialOta.FrameError"), domes::trace::Category::kTransport);
                ESP_LOGW(TAG, "Frame decode error (CRC mismatch or invalid length)");
                domes::infra::Diagnostics::recordCrcError();
                decoder.reset();
            }
        }
    }

    // Cleanup if stopped mid-OTA
    if (otaInProgress_.load()) {
        cleanupOta();
    }

    ESP_LOGI(TAG, "Serial OTA receiver task stopped");
}

esp_err_t SerialOtaReceiver::requestStop() {
    stopRequested_.store(true);
    return ESP_OK;
}

bool SerialOtaReceiver::shouldRun() const {
    return !stopRequested_.load();
}

void SerialOtaReceiver::handleOtaBegin(const uint8_t* payload, size_t len) {
    ESP_LOGI(TAG, "Received OTA_BEGIN");

    if (otaInProgress_.load()) {
        ESP_LOGW(TAG, "OTA already in progress, aborting previous");
        cleanupOta();
    }

    // Deserialize
    char version[32];
    TransportError err =
        deserializeOtaBegin(payload, len, reinterpret_cast<uint32_t*>(&firmwareSize_),
                            expectedSha256_, version, sizeof(version));

    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to deserialize OTA_BEGIN");
        sendAck(OtaStatus::kAborted, 0);
        return;
    }

    ESP_LOGI(TAG, "Firmware size: %zu bytes, version: %s", firmwareSize_, version);

    // Find update partition
    updatePartition_ = esp_ota_get_next_update_partition(nullptr);
    if (updatePartition_ == nullptr) {
        ESP_LOGE(TAG, "No OTA partition available");
        sendAck(OtaStatus::kPartitionError, 0);
        return;
    }

    ESP_LOGI(TAG, "Writing to partition: %s", updatePartition_->label);

    // Check partition size
    if (firmwareSize_ > updatePartition_->size) {
        ESP_LOGE(TAG, "Firmware too large for partition (%zu > %lu)", firmwareSize_,
                 updatePartition_->size);
        sendAck(OtaStatus::kSizeMismatch, 0);
        return;
    }

    // Begin OTA
    esp_err_t espErr = esp_ota_begin(updatePartition_, firmwareSize_, &otaHandle_);
    if (espErr != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(espErr));
        sendAck(OtaStatus::kFlashError, 0);
        return;
    }

    otaInProgress_.store(true);
    bytesReceived_ = 0;
    expectedOffset_ = 0;

    sendAck(OtaStatus::kOk, 0);
}

void SerialOtaReceiver::handleOtaData(const uint8_t* payload, size_t len) {
    if (!otaInProgress_.load()) {
        ESP_LOGW(TAG, "Received OTA_DATA without OTA_BEGIN");
        sendAck(OtaStatus::kAborted, 0);
        return;
    }

    // Deserialize
    uint32_t offset;
    const uint8_t* data;
    size_t dataLen;

    TransportError err = deserializeOtaData(payload, len, &offset, &data, &dataLen);
    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to deserialize OTA_DATA");
        sendAbortAndCleanup(OtaStatus::kAborted);
        return;
    }

    // Check offset
    if (offset != expectedOffset_) {
        ESP_LOGE(TAG, "Offset mismatch: expected %lu, got %lu", expectedOffset_, offset);
        sendAbortAndCleanup(OtaStatus::kOffsetMismatch);
        return;
    }

    // Write to flash
    esp_err_t espErr = esp_ota_write(otaHandle_, data, dataLen);
    if (espErr != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(espErr));
        sendAbortAndCleanup(OtaStatus::kFlashError);
        return;
    }

    bytesReceived_ += dataLen;
    expectedOffset_ += static_cast<uint32_t>(dataLen);

    // Log progress periodically
    if (bytesReceived_ % (64 * 1024) == 0 || bytesReceived_ == firmwareSize_) {
        ESP_LOGI(TAG, "OTA progress: %zu / %zu bytes (%.1f%%)", bytesReceived_, firmwareSize_,
                 100.0f * bytesReceived_ / firmwareSize_);
    }

    sendAck(OtaStatus::kOk, expectedOffset_);
}

void SerialOtaReceiver::handleOtaEnd() {
    ESP_LOGI(TAG, "Received OTA_END");

    if (!otaInProgress_.load()) {
        ESP_LOGW(TAG, "Received OTA_END without OTA_BEGIN");
        sendAck(OtaStatus::kAborted, 0);
        return;
    }

    // Check size
    if (bytesReceived_ != firmwareSize_) {
        ESP_LOGE(TAG, "Size mismatch: received %zu, expected %zu", bytesReceived_, firmwareSize_);
        sendAbortAndCleanup(OtaStatus::kSizeMismatch);
        return;
    }

    // Finish OTA (validates image)
    esp_err_t espErr = esp_ota_end(otaHandle_);
    otaHandle_ = 0;

    if (espErr != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(espErr));
        otaInProgress_.store(false);
        sendAck(OtaStatus::kVerifyFailed, bytesReceived_);
        return;
    }

    // Set boot partition
    espErr = esp_ota_set_boot_partition(updatePartition_);
    if (espErr != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(espErr));
        otaInProgress_.store(false);
        sendAck(OtaStatus::kPartitionError, bytesReceived_);
        return;
    }

    ESP_LOGI(TAG, "OTA complete! Rebooting in 1 second...");

    otaInProgress_.store(false);
    sendAck(OtaStatus::kOk, bytesReceived_);

    // Give time for ACK to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();
}

void SerialOtaReceiver::sendAck(OtaStatus status, uint32_t nextOffset) {
    std::array<uint8_t, 16> payloadBuf;
    size_t payloadLen = 0;

    TransportError err =
        serializeOtaAck(status, nextOffset, payloadBuf.data(), payloadBuf.size(), &payloadLen);
    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to serialize OTA_ACK");
        return;
    }

    std::array<uint8_t, kMaxFrameSize> frameBuf;
    size_t frameLen = 0;

    err = encodeFrame(static_cast<uint8_t>(OtaMsgType::kAck), payloadBuf.data(), payloadLen,
                      frameBuf.data(), frameBuf.size(), &frameLen);
    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to encode OTA_ACK frame");
        return;
    }

    err = transport_.send(frameBuf.data(), frameLen);
    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to send OTA_ACK: %s", transportErrorToString(err));
    }
}

void SerialOtaReceiver::sendAbortAndCleanup(OtaStatus reason) {
    std::array<uint8_t, 8> payloadBuf;
    size_t payloadLen = 0;

    TransportError err =
        serializeOtaAbort(reason, payloadBuf.data(), payloadBuf.size(), &payloadLen);
    if (isOk(err)) {
        std::array<uint8_t, kMaxFrameSize> frameBuf;
        size_t frameLen = 0;

        err = encodeFrame(static_cast<uint8_t>(OtaMsgType::kAbort), payloadBuf.data(), payloadLen,
                          frameBuf.data(), frameBuf.size(), &frameLen);
        if (isOk(err)) {
            transport_.send(frameBuf.data(), frameLen);
        }
    }

    cleanupOta();
}

void SerialOtaReceiver::cleanupOta() {
    if (otaHandle_ != 0) {
        esp_ota_abort(otaHandle_);
        otaHandle_ = 0;
    }

    otaInProgress_.store(false);
    updatePartition_ = nullptr;
    firmwareSize_ = 0;
    bytesReceived_ = 0;
    expectedOffset_ = 0;
}

}  // namespace domes
