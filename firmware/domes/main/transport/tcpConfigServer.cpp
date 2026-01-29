/**
 * @file tcpConfigServer.cpp
 * @brief TCP config server implementation
 */

#include "tcpConfigServer.hpp"
#include "protocol/frameCodec.hpp"
#include "config/configProtocol.hpp"
#include "services/imuService.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <array>
#include <cerrno>

static const char* TAG = "tcp_config";

namespace domes {

TcpConfigServer::TcpConfigServer(config::FeatureManager& features, uint16_t port)
    : features_(features)
    , port_(port)
    , stopRequested_(false)
    , listenSocket_(-1)
    , clientCount_(0) {
}

TcpConfigServer::~TcpConfigServer() {
    requestStop();
}

void TcpConfigServer::run() {
    ESP_LOGI(TAG, "TCP config server starting on port %u", port_);

    // Create listening socket
    int listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        return;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port_);

    if (bind(listenSock, reinterpret_cast<struct sockaddr*>(&serverAddr),
             sizeof(serverAddr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(listenSock);
        return;
    }

    // Start listening
    if (listen(listenSock, kMaxTcpClients) < 0) {
        ESP_LOGE(TAG, "Failed to listen: %d", errno);
        close(listenSock);
        return;
    }

    // Set non-blocking for accept loop
    int flags = fcntl(listenSock, F_GETFL, 0);
    fcntl(listenSock, F_SETFL, flags | O_NONBLOCK);

    listenSocket_.store(listenSock);
    ESP_LOGI(TAG, "TCP config server listening on port %u", port_);

    while (shouldRun()) {
        // Accept with timeout using select
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        int ret = select(listenSock + 1, &readfds, nullptr, nullptr, &tv);

        if (ret < 0) {
            if (errno == EINTR) continue;
            ESP_LOGE(TAG, "select failed: %d", errno);
            break;
        }

        if (ret == 0) {
            // Timeout, check stop flag
            continue;
        }

        // Accept new connection
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSock = accept(listenSock,
                                 reinterpret_cast<struct sockaddr*>(&clientAddr),
                                 &clientAddrLen);

        if (clientSock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            ESP_LOGE(TAG, "accept failed: %d", errno);
            continue;
        }

        // Log client connection
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        ESP_LOGI(TAG, "Client connected from %s:%u",
                 addrStr, ntohs(clientAddr.sin_port));

        // Check client limit
        if (clientCount_.load() >= kMaxTcpClients) {
            ESP_LOGW(TAG, "Max clients reached, rejecting connection");
            close(clientSock);
            continue;
        }

        clientCount_.fetch_add(1);

        // Handle client in current task (simple sequential handling)
        // For truly concurrent clients, would spawn separate tasks
        handleClient(clientSock);

        clientCount_.fetch_sub(1);
        ESP_LOGI(TAG, "Client from %s disconnected", addrStr);
    }

    // Cleanup
    listenSocket_.store(-1);
    close(listenSock);
    ESP_LOGI(TAG, "TCP config server stopped");
}

esp_err_t TcpConfigServer::requestStop() {
    stopRequested_.store(true);

    // Close listen socket to unblock accept
    int sock = listenSocket_.exchange(-1);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    return ESP_OK;
}

bool TcpConfigServer::shouldRun() const {
    return !stopRequested_.load();
}

void TcpConfigServer::handleClient(int clientSock) {
    // Create transport for this client
    TcpTransport transport;
    transport.setSocket(clientSock);

    TransportError err = transport.init();
    if (!isOk(err)) {
        ESP_LOGE(TAG, "Failed to init transport: %s", transportErrorToString(err));
        close(clientSock);
        return;
    }

    // Create config handler for this connection
    config::ConfigCommandHandler handler(transport, features_);
    handler.setLedService(ledService_);
    handler.setImuService(imuService_);

    // Frame decoder
    FrameDecoder decoder;
    std::array<uint8_t, 256> rxBuf;

    while (shouldRun() && transport.isConnected()) {
        size_t rxLen = rxBuf.size();
        err = transport.receive(rxBuf.data(), &rxLen, 100);

        if (err == TransportError::kTimeout) {
            continue;
        }

        if (err == TransportError::kDisconnected) {
            break;
        }

        if (!isOk(err)) {
            ESP_LOGE(TAG, "Receive error: %s", transportErrorToString(err));
            break;
        }

        // Feed bytes to decoder
        for (size_t i = 0; i < rxLen; ++i) {
            decoder.feedByte(rxBuf[i]);

            if (decoder.isComplete()) {
                uint8_t msgType = decoder.getType();
                const uint8_t* payload = decoder.getPayload();
                size_t payloadLen = decoder.getPayloadLen();

                // Only handle config messages over TCP
                if (config::isConfigMessage(msgType)) {
                    handler.handleCommand(msgType, payload, payloadLen);
                } else {
                    ESP_LOGW(TAG, "Ignoring non-config message: 0x%02X", msgType);
                }

                decoder.reset();
            } else if (decoder.isError()) {
                ESP_LOGW(TAG, "Frame decode error");
                decoder.reset();
            }
        }
    }

    // Transport destructor will close socket
}

}  // namespace domes
