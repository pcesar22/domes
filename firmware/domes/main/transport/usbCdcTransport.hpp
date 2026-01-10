#pragma once

/**
 * @file usbCdcTransport.hpp
 * @brief USB-CDC transport for ESP32-S3
 *
 * Implements ITransport using the built-in USB Serial/JTAG peripheral.
 * This uses the USB-CDC interface that appears as a serial port on the host.
 *
 * On ESP32-S3 DevKitC-1, this is the same port used for programming and
 * console output when CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG is enabled.
 */

#include "interfaces/iTransport.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

namespace domes {

/**
 * @brief USB-CDC transport for ESP32-S3
 *
 * Wraps the USB Serial/JTAG peripheral to provide ITransport interface.
 * Uses a ring buffer for non-blocking receive operations.
 *
 * @note This transport shares the USB-CDC with ESP-IDF console output.
 *       Log messages may interfere with protocol data if not careful.
 *       Consider using a separate UART for debug logging in production.
 */
class UsbCdcTransport : public ITransport {
public:
    /**
     * @brief Construct USB-CDC transport
     *
     * @param rxBufSize Size of receive ring buffer (default 4KB)
     */
    explicit UsbCdcTransport(size_t rxBufSize = 4096);

    ~UsbCdcTransport() override;

    // Non-copyable
    UsbCdcTransport(const UsbCdcTransport&) = delete;
    UsbCdcTransport& operator=(const UsbCdcTransport&) = delete;

    // ITransport implementation
    TransportError init() override;
    TransportError send(const uint8_t* data, size_t len) override;
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;
    TransportError flush() override;
    size_t available() const override;

private:
    size_t rxBufSize_;
    RingbufHandle_t rxRingBuf_;
    SemaphoreHandle_t txMutex_;
    bool initialized_;
};

}  // namespace domes
