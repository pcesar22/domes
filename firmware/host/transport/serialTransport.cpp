/**
 * @file serialTransport.cpp
 * @brief Linux serial port transport implementation
 */

#include "serialTransport.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace domes::host {

namespace {

/**
 * @brief Convert baud rate to termios constant
 */
speed_t baudRateToSpeed(int baudRate) {
    switch (baudRate) {
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
        case 500000:  return B500000;
        case 576000:  return B576000;
        case 921600:  return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        default:      return B115200;
    }
}

}  // namespace

SerialTransport::SerialTransport(const std::string& portPath, int baudRate)
    : portPath_(portPath)
    , baudRate_(baudRate)
    , fd_(-1) {
}

SerialTransport::~SerialTransport() {
    disconnect();
}

SerialTransport::SerialTransport(SerialTransport&& other) noexcept
    : portPath_(std::move(other.portPath_))
    , baudRate_(other.baudRate_)
    , fd_(other.fd_) {
    other.fd_ = -1;
}

SerialTransport& SerialTransport::operator=(SerialTransport&& other) noexcept {
    if (this != &other) {
        disconnect();
        portPath_ = std::move(other.portPath_);
        baudRate_ = other.baudRate_;
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

TransportError SerialTransport::init() {
    if (fd_ >= 0) {
        return TransportError::kAlreadyInit;
    }

    // Open serial port
    fd_ = ::open(portPath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return TransportError::kIoError;
    }

    // Configure port
    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return TransportError::kIoError;
    }

    // Set baud rate
    speed_t speed = baudRateToSpeed(baudRate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;   // No parity
    tty.c_cflag &= ~CSTOPB;   // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;       // 8 data bits

    // No hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;

    // Raw input mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    // No software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Raw output mode
    tty.c_oflag &= ~OPOST;

    // No input processing
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Minimum characters for read: 0 (non-blocking)
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // Apply settings
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return TransportError::kIoError;
    }

    // Flush any stale data
    tcflush(fd_, TCIOFLUSH);

    return TransportError::kOk;
}

TransportError SerialTransport::send(const uint8_t* data, size_t len) {
    if (fd_ < 0) {
        return TransportError::kNotInitialized;
    }
    if (data == nullptr || len == 0) {
        return TransportError::kInvalidArg;
    }

    size_t totalWritten = 0;
    while (totalWritten < len) {
        ssize_t written = ::write(fd_, data + totalWritten, len - totalWritten);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Wait for write to be possible
                struct pollfd pfd{};
                pfd.fd = fd_;
                pfd.events = POLLOUT;
                int ret = ::poll(&pfd, 1, 1000);  // 1 second timeout
                if (ret <= 0) {
                    return TransportError::kTimeout;
                }
                continue;
            }
            return TransportError::kIoError;
        }
        totalWritten += static_cast<size_t>(written);
    }

    return TransportError::kOk;
}

TransportError SerialTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    if (fd_ < 0) {
        return TransportError::kNotInitialized;
    }
    if (buf == nullptr || len == nullptr || *len == 0) {
        return TransportError::kInvalidArg;
    }

    // Use poll for timeout
    struct pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int ret = ::poll(&pfd, 1, static_cast<int>(timeoutMs));
    if (ret < 0) {
        return TransportError::kIoError;
    }
    if (ret == 0) {
        *len = 0;
        return TransportError::kTimeout;
    }

    ssize_t bytesRead = ::read(fd_, buf, *len);
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *len = 0;
            return TransportError::kTimeout;
        }
        return TransportError::kIoError;
    }

    *len = static_cast<size_t>(bytesRead);
    return TransportError::kOk;
}

bool SerialTransport::isConnected() const {
    return fd_ >= 0;
}

void SerialTransport::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

TransportError SerialTransport::flush() {
    if (fd_ < 0) {
        return TransportError::kNotInitialized;
    }

    if (tcdrain(fd_) != 0) {
        return TransportError::kIoError;
    }
    return TransportError::kOk;
}

size_t SerialTransport::available() const {
    if (fd_ < 0) {
        return 0;
    }

    int bytes = 0;
    if (ioctl(fd_, FIONREAD, &bytes) < 0) {
        return 0;
    }
    return static_cast<size_t>(bytes);
}

}  // namespace domes::host
