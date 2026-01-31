#pragma once

/**
 * @file tcpConfigServer.hpp
 * @brief TCP server for runtime configuration commands
 *
 * Listens on a TCP port and handles config protocol commands
 * from network clients (e.g., domes-cli --wifi).
 *
 * Uses the same frame protocol and ConfigCommandHandler as the
 * USB-CDC transport, allowing the same host tool to work over WiFi.
 */

#include "tcpTransport.hpp"
#include "config/configCommandHandler.hpp"
#include "config/featureManager.hpp"
#include "config/modeManager.hpp"
#include "interfaces/iTaskRunner.hpp"

#include <atomic>
#include <cstdint>

namespace domes {

class LedService;  // Forward declaration
class ImuService;  // Forward declaration

/**
 * @brief Default TCP port for config server
 */
constexpr uint16_t kConfigServerPort = 5000;

/**
 * @brief Maximum number of concurrent TCP clients
 */
constexpr size_t kMaxTcpClients = 2;

/**
 * @brief TCP server for config commands
 *
 * Runs as a FreeRTOS task, accepting TCP connections and
 * processing config commands from each client.
 *
 * Features:
 * - Supports multiple concurrent clients (up to kMaxTcpClients)
 * - Uses frame protocol (same as USB-CDC)
 * - Graceful shutdown with client cleanup
 *
 * Usage:
 * @code
 * FeatureManager features;
 * TcpConfigServer server(features, 5000);
 *
 * TaskManager::createTask(server, "tcp_config", 4096, 5);
 * // Server now accepting connections on port 5000
 * @endcode
 */
class TcpConfigServer : public ITaskRunner {
public:
    /**
     * @brief Construct TCP config server
     *
     * @param features Feature manager for config commands
     * @param port TCP port to listen on (default: 5000)
     */
    explicit TcpConfigServer(config::FeatureManager& features,
                              uint16_t port = kConfigServerPort);

    ~TcpConfigServer() override;

    // Non-copyable
    TcpConfigServer(const TcpConfigServer&) = delete;
    TcpConfigServer& operator=(const TcpConfigServer&) = delete;

    // ITaskRunner implementation
    void run() override;
    esp_err_t requestStop() override;
    bool shouldRun() const override;

    /**
     * @brief Get the port the server is listening on
     */
    uint16_t getPort() const { return port_; }

    /**
     * @brief Check if server is running and accepting connections
     */
    bool isListening() const { return listenSocket_.load() >= 0; }

    /**
     * @brief Get number of connected clients
     */
    size_t getClientCount() const { return clientCount_.load(); }

    /**
     * @brief Set LED service for pattern commands
     *
     * @param ledService LED service instance
     */
    void setLedService(LedService* ledService) { ledService_ = ledService; }

    /**
     * @brief Set IMU service for triage commands
     *
     * @param imuService IMU service instance
     */
    void setImuService(ImuService* imuService) { imuService_ = imuService; }

    /**
     * @brief Set mode manager for system mode commands
     *
     * @param modeManager Mode manager instance
     */
    void setModeManager(config::ModeManager* modeManager) { modeManager_ = modeManager; }

private:
    /**
     * @brief Handle a single client connection
     *
     * Processes frames and dispatches to ConfigCommandHandler until
     * the client disconnects or server is stopped.
     *
     * @param clientSock Connected client socket
     */
    void handleClient(int clientSock);

    config::FeatureManager& features_;
    uint16_t port_;
    LedService* ledService_ = nullptr;
    ImuService* imuService_ = nullptr;
    config::ModeManager* modeManager_ = nullptr;

    std::atomic<bool> stopRequested_;
    std::atomic<int> listenSocket_;
    std::atomic<size_t> clientCount_;
};

}  // namespace domes
