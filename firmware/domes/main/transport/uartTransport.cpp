/**
 * @file uartTransport.cpp
 * @brief UART transport implementation
 */

#include "uartTransport.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "trace/traceApi.hpp"

namespace {
constexpr const char* kTag = "uart_transport";
constexpr TickType_t kTxTimeout = pdMS_TO_TICKS(1000);
}  // namespace

namespace domes {

UartTransport::UartTransport(uart_port_t port, int txPin, int rxPin, uint32_t baudRate,
                             size_t rxBufferSize, size_t txBufferSize)
    : port_(port),
      txPin_(txPin),
      rxPin_(rxPin),
      baudRate_(baudRate),
      rxBufferSize_(rxBufferSize),
      txBufferSize_(txBufferSize),
      txMutex_(nullptr),
      initialized_(false) {}

UartTransport::~UartTransport() {
    disconnect();
}

TransportError UartTransport::init() {
    if (initialized_) {
        return TransportError::kAlreadyInit;
    }

    txMutex_ = xSemaphoreCreateMutex();
    if (txMutex_ == nullptr) {
        return TransportError::kNoMemory;
    }

    uart_config_t config = {};
    config.baud_rate = static_cast<int>(baudRate_);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config(port_, &config);
    if (err == ESP_OK) {
        err = uart_set_pin(port_, txPin_, rxPin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) {
        err = uart_driver_install(port_, static_cast<int>(rxBufferSize_),
                                  static_cast<int>(txBufferSize_), 0, nullptr, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to initialize UART%d: %s", static_cast<int>(port_),
                 esp_err_to_name(err));
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
        return TransportError::kIoError;
    }

    uart_flush_input(port_);
    initialized_ = true;
    ESP_LOGI(kTag, "UART%d protocol transport ready at %lu baud", static_cast<int>(port_),
             static_cast<unsigned long>(baudRate_));
    return TransportError::kOk;
}

TransportError UartTransport::send(const uint8_t* data, size_t len) {
    TRACE_SCOPE(TRACE_ID("Uart.Send"), trace::Category::kTransport);
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (data == nullptr || len == 0) {
        return TransportError::kInvalidArg;
    }

    const uint32_t waitStarted = static_cast<uint32_t>(esp_timer_get_time());
    if (xSemaphoreTake(txMutex_, kTxTimeout) != pdTRUE) {
        return TransportError::kTimeout;
    }
    const uint32_t waited = static_cast<uint32_t>(esp_timer_get_time()) - waitStarted;
    TRACE_MUTEX_LOCK(TRACE_ID("Uart.TxMutex"));
    if (waited > 100) {
        TRACE_MUTEX_CONTENTION(TRACE_ID("Uart.TxMutex"), waited);
    }

    const int written = uart_write_bytes(port_, data, len);
    esp_err_t flushResult = ESP_FAIL;
    if (written == static_cast<int>(len)) {
        flushResult = uart_wait_tx_done(port_, kTxTimeout);
    }

    TRACE_MUTEX_UNLOCK(TRACE_ID("Uart.TxMutex"));
    xSemaphoreGive(txMutex_);

    if (written < 0) {
        return TransportError::kIoError;
    }
    if (written != static_cast<int>(len) || flushResult == ESP_ERR_TIMEOUT) {
        return TransportError::kTimeout;
    }
    if (flushResult != ESP_OK) {
        return TransportError::kIoError;
    }

    TRACE_COUNTER(TRACE_ID("Uart.BytesSent"), len, trace::Category::kTransport);
    return TransportError::kOk;
}

TransportError UartTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (buf == nullptr || len == nullptr || *len == 0) {
        return TransportError::kInvalidArg;
    }

    const int bytesRead = uart_read_bytes(port_, buf, *len, pdMS_TO_TICKS(timeoutMs));
    if (bytesRead < 0) {
        *len = 0;
        return TransportError::kIoError;
    }
    if (bytesRead == 0) {
        *len = 0;
        return TransportError::kTimeout;
    }

    *len = static_cast<size_t>(bytesRead);
    TRACE_COUNTER(TRACE_ID("Uart.BytesReceived"), *len, trace::Category::kTransport);
    return TransportError::kOk;
}

bool UartTransport::isConnected() const {
    return initialized_;
}

void UartTransport::disconnect() {
    if (!initialized_) {
        return;
    }

    uart_driver_delete(port_);
    if (txMutex_ != nullptr) {
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
    }
    initialized_ = false;
}

TransportError UartTransport::flush() {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    const esp_err_t err = uart_wait_tx_done(port_, kTxTimeout);
    if (err == ESP_ERR_TIMEOUT) {
        return TransportError::kTimeout;
    }
    return err == ESP_OK ? TransportError::kOk : TransportError::kIoError;
}

size_t UartTransport::available() const {
    if (!initialized_) {
        return 0;
    }
    size_t buffered = 0;
    return uart_get_buffered_data_len(port_, &buffered) == ESP_OK ? buffered : 0;
}

}  // namespace domes
