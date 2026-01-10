#pragma once

/**
 * @file serialTransport.hpp
 * @brief Linux serial port transport implementation
 *
 * Implements ITransport for Linux serial ports (/dev/ttyACM*, /dev/ttyUSB*).
 * Uses POSIX termios for configuration.
 */

#include "interfaces/iTransport.hpp"

#include <string>

namespace domes::host {

/**
 * @brief Serial port transport for Linux
 *
 * Opens a serial port with configurable baud rate and provides
 * ITransport interface for bidirectional communication.
 *
 * Default configuration:
 * - 8N1 (8 data bits, no parity, 1 stop bit)
 * - No hardware flow control
 * - Raw mode (no line editing)
 */
class SerialTransport : public ITransport {
public:
    /**
     * @brief Construct serial transport
     *
     * @param portPath Path to serial device (e.g., "/dev/ttyACM0")
     * @param baudRate Baud rate (default 115200)
     */
    explicit SerialTransport(const std::string& portPath, int baudRate = 115200);

    ~SerialTransport() override;

    // Non-copyable
    SerialTransport(const SerialTransport&) = delete;
    SerialTransport& operator=(const SerialTransport&) = delete;

    // Movable
    SerialTransport(SerialTransport&& other) noexcept;
    SerialTransport& operator=(SerialTransport&& other) noexcept;

    // ITransport implementation
    TransportError init() override;
    TransportError send(const uint8_t* data, size_t len) override;
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;
    TransportError flush() override;
    size_t available() const override;

    /// @return Port path
    const std::string& getPortPath() const { return portPath_; }

    /// @return Configured baud rate
    int getBaudRate() const { return baudRate_; }

private:
    std::string portPath_;
    int baudRate_;
    int fd_;  ///< File descriptor (-1 if not open)
};

}  // namespace domes::host
