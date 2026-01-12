#pragma once

/**
 * @file tcpTransport.hpp
 * @brief TCP socket transport implementation
 *
 * Provides ITransport implementation over a TCP socket connection.
 * Used by TcpConfigServer to handle client connections.
 */

#include "interfaces/iTransport.hpp"

#include "lwip/sockets.h"

#include <atomic>

namespace domes {

/**
 * @brief TCP socket transport
 *
 * Wraps a connected TCP socket as an ITransport.
 * Does not own the socket lifecycle - caller manages accept/close.
 *
 * Thread Safety:
 * - send() and receive() are thread-safe for concurrent use
 * - setSocket() should only be called when no I/O is in progress
 */
class TcpTransport : public ITransport {
public:
    TcpTransport();
    ~TcpTransport() override;

    // Non-copyable
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    /**
     * @brief Set the socket file descriptor
     *
     * @param sockfd Connected socket file descriptor
     */
    void setSocket(int sockfd);

    /**
     * @brief Get the socket file descriptor
     *
     * @return Socket fd, or -1 if not set
     */
    int getSocket() const { return sockfd_.load(); }

    // ITransport implementation
    TransportError init() override;
    TransportError send(const uint8_t* data, size_t len) override;
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;
    TransportError flush() override;
    size_t available() const override;

private:
    std::atomic<int> sockfd_;
    std::atomic<bool> initialized_;
};

}  // namespace domes
