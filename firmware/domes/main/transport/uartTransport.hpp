#pragma once

/**
 * @file uartTransport.hpp
 * @brief UART byte-stream transport for the NFF development board
 *
 * The NFF board's CP2102N bridge is connected to ESP32-S3 UART0. The native
 * USB Serial/JTAG peripheral remains dedicated to logs and debugging, keeping
 * framed protocol traffic free from console output.
 */

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "interfaces/iTransport.hpp"

namespace domes {

class UartTransport : public ITransport {
public:
    explicit UartTransport(uart_port_t port, int txPin, int rxPin, uint32_t baudRate = 115200,
                           size_t rxBufferSize = 4096, size_t txBufferSize = 2048);
    ~UartTransport() override;

    UartTransport(const UartTransport&) = delete;
    UartTransport& operator=(const UartTransport&) = delete;

    TransportError init() override;
    TransportError send(const uint8_t* data, size_t len) override;
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;
    TransportError flush() override;
    size_t available() const override;

private:
    uart_port_t port_;
    int txPin_;
    int rxPin_;
    uint32_t baudRate_;
    size_t rxBufferSize_;
    size_t txBufferSize_;
    SemaphoreHandle_t txMutex_;
    bool initialized_;
};

}  // namespace domes
