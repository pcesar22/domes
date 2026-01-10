#pragma once

/**
 * @file iTransport.hpp
 * @brief Abstract transport interface for communication layers
 *
 * Provides a platform-agnostic interface for bidirectional byte-stream
 * communication. Implementations include:
 * - ESP32: USB-CDC, UART, BLE, ESP-NOW
 * - Host: Serial port (termios), TCP socket, BlueZ BLE
 *
 * This interface enables code reuse between ESP32 firmware and
 * Linux host simulator/tools.
 */

#include "result.hpp"

#include <cstddef>
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for bidirectional byte-stream transport
 *
 * All transport implementations (serial, BLE, TCP, etc.) implement this
 * interface, allowing protocol code to be transport-agnostic.
 *
 * Thread Safety:
 * - Implementations should be thread-safe for concurrent send/receive
 * - init() and disconnect() should only be called from one thread
 *
 * Usage Pattern:
 * @code
 * auto transport = std::make_unique<SerialTransport>("/dev/ttyACM0");
 * if (!isOk(transport->init())) { return error; }
 *
 * uint8_t buf[256];
 * size_t len = sizeof(buf);
 * if (isOk(transport->receive(buf, &len, 1000))) {
 *     // Process received data
 * }
 *
 * transport->disconnect();
 * @endcode
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Initialize the transport
     *
     * Opens the underlying communication channel and prepares for I/O.
     * Must be called before any send/receive operations.
     *
     * @return TransportError::kOk on success
     * @return TransportError::kAlreadyInit if already initialized
     * @return TransportError::kIoError on hardware/OS failure
     */
    virtual TransportError init() = 0;

    /**
     * @brief Send data over the transport
     *
     * Transmits the specified bytes. May block until all data is sent
     * or an error occurs.
     *
     * @param data Pointer to data buffer
     * @param len Number of bytes to send
     * @return TransportError::kOk on success
     * @return TransportError::kInvalidArg if data is null or len is 0
     * @return TransportError::kNotInitialized if not initialized
     * @return TransportError::kDisconnected if connection lost
     * @return TransportError::kIoError on send failure
     */
    virtual TransportError send(const uint8_t* data, size_t len) = 0;

    /**
     * @brief Receive data from the transport
     *
     * Reads up to *len bytes into the buffer. On return, *len contains
     * the actual number of bytes received.
     *
     * @param buf Buffer to store received data
     * @param len [in/out] On input: buffer size. On output: bytes received.
     * @param timeoutMs Maximum time to wait for data (0 = non-blocking)
     * @return TransportError::kOk on success (data received)
     * @return TransportError::kTimeout if no data within timeout
     * @return TransportError::kInvalidArg if buf is null or len is 0
     * @return TransportError::kNotInitialized if not initialized
     * @return TransportError::kDisconnected if connection lost
     * @return TransportError::kIoError on receive failure
     */
    virtual TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) = 0;

    /**
     * @brief Check if transport is connected
     *
     * @return true if transport is initialized and connection is active
     * @return false if not initialized or disconnected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Disconnect and release resources
     *
     * Closes the underlying communication channel. Safe to call multiple
     * times or if not initialized.
     */
    virtual void disconnect() = 0;

    /**
     * @brief Flush any pending transmit data
     *
     * Blocks until all buffered data has been transmitted.
     * Default implementation does nothing (for transports without buffering).
     *
     * @return TransportError::kOk on success
     */
    virtual TransportError flush() {
        return TransportError::kOk;
    }

    /**
     * @brief Get number of bytes available to read without blocking
     *
     * Default implementation returns 0 (unknown/unsupported).
     *
     * @return Number of bytes available, or 0 if unknown
     */
    virtual size_t available() const {
        return 0;
    }
};

}  // namespace domes
