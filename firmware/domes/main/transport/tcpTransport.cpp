/**
 * @file tcpTransport.cpp
 * @brief TCP socket transport implementation
 */

#include "tcpTransport.hpp"

#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "lwip/sockets.h"

#include <cerrno>

static const char* TAG = "tcp_transport";

namespace domes {

TcpTransport::TcpTransport()
    : sockfd_(-1)
    , initialized_(false) {
}

TcpTransport::~TcpTransport() {
    disconnect();
}

void TcpTransport::setSocket(int sockfd) {
    sockfd_.store(sockfd);
}

TransportError TcpTransport::init() {
    if (initialized_.load()) {
        return TransportError::kAlreadyInit;
    }

    if (sockfd_.load() < 0) {
        ESP_LOGE(TAG, "Socket not set");
        return TransportError::kNotInitialized;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(sockfd_.load(), F_GETFL, 0);
    if (flags < 0) {
        ESP_LOGE(TAG, "fcntl F_GETFL failed: %d", errno);
        return TransportError::kIoError;
    }

    if (fcntl(sockfd_.load(), F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "fcntl F_SETFL failed: %d", errno);
        return TransportError::kIoError;
    }

    initialized_.store(true);
    ESP_LOGI(TAG, "TCP transport initialized (socket %d)", sockfd_.load());

    return TransportError::kOk;
}

TransportError TcpTransport::send(const uint8_t* data, size_t len) {
    TRACE_SCOPE(TRACE_ID("Tcp.Send"), domes::trace::Category::kTransport);
    if (!initialized_.load()) {
        return TransportError::kNotInitialized;
    }

    if (data == nullptr || len == 0) {
        return TransportError::kInvalidArg;
    }

    int sockfd = sockfd_.load();
    if (sockfd < 0) {
        return TransportError::kDisconnected;
    }

    size_t totalSent = 0;
    while (totalSent < len) {
        ssize_t sent = ::send(sockfd, data + totalSent, len - totalSent, 0);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block - wait briefly and retry
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            if (errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN) {
                ESP_LOGW(TAG, "Connection closed during send");
                return TransportError::kDisconnected;
            }

            ESP_LOGE(TAG, "send failed: %d", errno);
            return TransportError::kIoError;
        }

        totalSent += static_cast<size_t>(sent);
    }

    TRACE_COUNTER(TRACE_ID("Tcp.BytesSent"), totalSent, domes::trace::Category::kTransport);
    return TransportError::kOk;
}

TransportError TcpTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    TRACE_SCOPE(TRACE_ID("Tcp.Receive"), domes::trace::Category::kTransport);
    if (!initialized_.load()) {
        return TransportError::kNotInitialized;
    }

    if (buf == nullptr || len == nullptr || *len == 0) {
        return TransportError::kInvalidArg;
    }

    int sockfd = sockfd_.load();
    if (sockfd < 0) {
        return TransportError::kDisconnected;
    }

    // Use select for timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(sockfd + 1, &readfds, nullptr, nullptr, &tv);

    if (ret < 0) {
        ESP_LOGE(TAG, "select failed: %d", errno);
        return TransportError::kIoError;
    }

    if (ret == 0) {
        *len = 0;
        return TransportError::kTimeout;
    }

    ssize_t received = recv(sockfd, buf, *len, 0);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *len = 0;
            return TransportError::kTimeout;
        }

        if (errno == ECONNRESET || errno == ENOTCONN) {
            ESP_LOGW(TAG, "Connection closed during receive");
            return TransportError::kDisconnected;
        }

        ESP_LOGE(TAG, "recv failed: %d", errno);
        return TransportError::kIoError;
    }

    if (received == 0) {
        // Peer closed connection
        return TransportError::kDisconnected;
    }

    *len = static_cast<size_t>(received);
    TRACE_COUNTER(TRACE_ID("Tcp.BytesReceived"), *len, domes::trace::Category::kTransport);
    return TransportError::kOk;
}

bool TcpTransport::isConnected() const {
    if (!initialized_.load()) {
        return false;
    }

    int sockfd = sockfd_.load();
    if (sockfd < 0) {
        return false;
    }

    // Check if socket is still valid with a zero-length recv
    char buf;
    ssize_t ret = recv(sockfd, &buf, 0, MSG_PEEK);
    return ret >= 0 || errno == EAGAIN || errno == EWOULDBLOCK;
}

void TcpTransport::disconnect() {
    int sockfd = sockfd_.exchange(-1);
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        ESP_LOGI(TAG, "TCP transport disconnected");
    }
    initialized_.store(false);
}

TransportError TcpTransport::flush() {
    // TCP handles buffering at the kernel level
    // Could use TCP_NODELAY or explicit flush, but not critical
    return TransportError::kOk;
}

size_t TcpTransport::available() const {
    int sockfd = sockfd_.load();
    if (sockfd < 0) {
        return 0;
    }

    int available = 0;
    if (ioctl(sockfd, FIONREAD, &available) < 0) {
        return 0;
    }

    return static_cast<size_t>(available);
}

}  // namespace domes
