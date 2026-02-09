/**
 * @file usbCdcTransport.cpp
 * @brief USB-CDC transport implementation for ESP32-S3
 */

#include "usbCdcTransport.hpp"

#include "trace/traceApi.hpp"

#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

static const char* TAG = "usb_cdc";

namespace domes {

UsbCdcTransport::UsbCdcTransport(size_t rxBufSize)
    : rxBufSize_(rxBufSize), rxRingBuf_(nullptr), txMutex_(nullptr), initialized_(false) {}

UsbCdcTransport::~UsbCdcTransport() {
    disconnect();
}

TransportError UsbCdcTransport::init() {
    if (initialized_) {
        return TransportError::kAlreadyInit;
    }

    // Create TX mutex
    txMutex_ = xSemaphoreCreateMutex();
    if (txMutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create TX mutex");
        return TransportError::kNoMemory;
    }

    // Create RX ring buffer
    rxRingBuf_ = xRingbufferCreate(rxBufSize_, RINGBUF_TYPE_BYTEBUF);
    if (rxRingBuf_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create RX ring buffer");
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
        return TransportError::kNoMemory;
    }

    // Configure USB Serial/JTAG driver
    usb_serial_jtag_driver_config_t config = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };

    esp_err_t err = usb_serial_jtag_driver_install(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Serial/JTAG driver: %s", esp_err_to_name(err));
        vRingbufferDelete(rxRingBuf_);
        rxRingBuf_ = nullptr;
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
        return TransportError::kIoError;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "USB-CDC transport initialized");
    return TransportError::kOk;
}

TransportError UsbCdcTransport::send(const uint8_t* data, size_t len) {
    TRACE_SCOPE(TRACE_ID("UsbCdc.Send"), domes::trace::Category::kTransport);
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (data == nullptr || len == 0) {
        return TransportError::kInvalidArg;
    }

    // Take mutex for thread-safe TX
    if (xSemaphoreTake(txMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return TransportError::kTimeout;
    }

    size_t totalWritten = 0;
    while (totalWritten < len) {
        int written = usb_serial_jtag_write_bytes(data + totalWritten, len - totalWritten,
                                                  pdMS_TO_TICKS(1000));

        if (written < 0) {
            xSemaphoreGive(txMutex_);
            return TransportError::kIoError;
        }
        if (written == 0) {
            // Timeout waiting for USB host to read
            xSemaphoreGive(txMutex_);
            return TransportError::kTimeout;
        }

        totalWritten += static_cast<size_t>(written);
    }

    TRACE_COUNTER(TRACE_ID("UsbCdc.BytesSent"), totalWritten, domes::trace::Category::kTransport);
    xSemaphoreGive(txMutex_);
    return TransportError::kOk;
}

TransportError UsbCdcTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    TRACE_SCOPE(TRACE_ID("UsbCdc.Receive"), domes::trace::Category::kTransport);
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (buf == nullptr || len == nullptr || *len == 0) {
        return TransportError::kInvalidArg;
    }

    // Read from USB driver into our buffer
    int bytesRead = usb_serial_jtag_read_bytes(buf, *len, pdMS_TO_TICKS(timeoutMs));

    if (bytesRead < 0) {
        *len = 0;
        return TransportError::kIoError;
    }
    if (bytesRead == 0) {
        *len = 0;
        return TransportError::kTimeout;
    }

    *len = static_cast<size_t>(bytesRead);
    TRACE_COUNTER(TRACE_ID("UsbCdc.BytesReceived"), *len, domes::trace::Category::kTransport);
    return TransportError::kOk;
}

bool UsbCdcTransport::isConnected() const {
    return initialized_;
}

void UsbCdcTransport::disconnect() {
    if (!initialized_) {
        return;
    }

    usb_serial_jtag_driver_uninstall();

    if (rxRingBuf_ != nullptr) {
        vRingbufferDelete(rxRingBuf_);
        rxRingBuf_ = nullptr;
    }

    if (txMutex_ != nullptr) {
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
    }

    initialized_ = false;
    ESP_LOGI(TAG, "USB-CDC transport disconnected");
}

TransportError UsbCdcTransport::flush() {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    // USB Serial/JTAG driver doesn't have explicit flush
    // Data is sent when the USB host reads it
    return TransportError::kOk;
}

size_t UsbCdcTransport::available() const {
    // The USB Serial/JTAG driver doesn't expose RX buffer level
    // Return 0 (unknown)
    return 0;
}

}  // namespace domes
