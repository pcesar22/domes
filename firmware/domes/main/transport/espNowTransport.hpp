#pragma once

/**
 * @file espNowTransport.hpp
 * @brief ESP-NOW peer-to-peer transport
 *
 * Implements ITransport for ESP-NOW communication between pods.
 * Uses broadcast by default, with optional unicast to specific peers.
 *
 * Requires WiFi to be initialized in station mode before use.
 */

#include "interfaces/iTransport.hpp"
#include "trace/traceApi.hpp"

#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace domes {

/// Broadcast MAC address for ESP-NOW
static constexpr uint8_t kEspNowBroadcastAddr[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/// Maximum ESP-NOW payload size
static constexpr size_t kEspNowMaxPayload = 250;

/// Default RX ring buffer size
static constexpr size_t kEspNowRxBufSize = 2048;

/**
 * @brief ESP-NOW transport for peer-to-peer communication
 *
 * Thread-safe transport using ESP-NOW for direct pod-to-pod communication.
 * Pattern follows BleOtaService (ring buffer RX, semaphore signaling).
 *
 * Lifecycle:
 * 1. WiFi must be initialized in STA mode (done in main.cpp)
 * 2. Call init() to initialize ESP-NOW and register callbacks
 * 3. Call addPeer() for specific peers, or use broadcast (default)
 * 4. Use send()/receive() for communication
 * 5. Call disconnect() to clean up
 */
class EspNowTransport : public ITransport {
public:
    EspNowTransport();
    ~EspNowTransport() override;

    // Non-copyable
    EspNowTransport(const EspNowTransport&) = delete;
    EspNowTransport& operator=(const EspNowTransport&) = delete;

    // =========================================================================
    // ITransport interface
    // =========================================================================

    TransportError init() override;
    TransportError send(const uint8_t* data, size_t len) override;
    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;

    // =========================================================================
    // ESP-NOW specific methods
    // =========================================================================

    /**
     * @brief Send data to a specific peer (unicast)
     *
     * Peer must be registered via addPeer() first.
     *
     * @param macAddr 6-byte destination MAC address
     * @param data Payload buffer
     * @param len Payload length (max 250 bytes)
     * @return TransportError::kOk on success
     */
    TransportError sendTo(const uint8_t macAddr[ESP_NOW_ETH_ALEN],
                          const uint8_t* data, size_t len);

    /**
     * @brief Add a peer by MAC address
     * @param macAddr 6-byte MAC address
     * @return TransportError::kOk on success
     */
    TransportError addPeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]);

    /**
     * @brief Remove a peer
     * @param macAddr 6-byte MAC address
     * @return TransportError::kOk on success
     */
    TransportError removePeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]);

    /**
     * @brief Get number of registered peers
     */
    uint8_t getPeerCount() const { return peerCount_.load(std::memory_order_relaxed); }

    // =========================================================================
    // Internal callbacks (called from ESP-NOW stack)
    // =========================================================================

    /// Called when data is received from a peer
    void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len);

    /// Called when send completes
    void onSendComplete(const uint8_t* macAddr, esp_now_send_status_t status);

private:
    /// RX ring buffer
    RingbufHandle_t rxRingBuf_ = nullptr;

    /// Semaphore signaled when data is available in RX buffer
    SemaphoreHandle_t rxSemaphore_ = nullptr;

    /// Mutex for send operations
    SemaphoreHandle_t txMutex_ = nullptr;

    /// Send completion semaphore (signaled from send callback)
    SemaphoreHandle_t txDoneSemaphore_ = nullptr;

    /// Last send status (set in callback)
    std::atomic<esp_now_send_status_t> lastSendStatus_{ESP_NOW_SEND_FAIL};

    /// State
    std::atomic<bool> initialized_{false};
    std::atomic<uint8_t> peerCount_{0};
};

}  // namespace domes
