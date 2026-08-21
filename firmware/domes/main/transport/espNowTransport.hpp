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

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "interfaces/iTransport.hpp"
#include "trace/traceApi.hpp"
#include "transport/iEspNowRadio.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace domes {

inline constexpr EspNowAddress kEspNowBroadcastAddress = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/// Internal metadata layout prepended to each received frame in the RX queue.
static constexpr size_t kEspNowRxRssiValidOffset = 0;
static constexpr size_t kEspNowRxRssiOffset = 1;
static constexpr size_t kEspNowRxSourceValidOffset = 2;
static constexpr size_t kEspNowRxSourceOffset = 3;
static constexpr size_t kEspNowRxTokenOffset = kEspNowRxSourceOffset + kEspNowAddressSize;
static constexpr size_t kEspNowRxMetadataSize =
    kEspNowRxTokenOffset + sizeof(EspNowCorrelationToken);

/// Default RX ring buffer size
static constexpr size_t kEspNowRxBufSize = 2048;
static constexpr size_t kEspNowRingItemOverhead = 8;
constexpr size_t espNowRingAlignedSize(size_t size) {
    return (size + 3U) & ~size_t{3U};
}
constexpr size_t espNowRingItemStorage(size_t metadataSize) {
    return kEspNowRingItemOverhead + espNowRingAlignedSize(metadataSize + kEspNowMaxPayload);
}
static constexpr size_t kEspNowRxBaselineMetadataSize = kEspNowRxSourceOffset + kEspNowAddressSize;
static constexpr size_t kEspNowRxBaselineMaxFrames =
    kEspNowRxBufSize / espNowRingItemStorage(kEspNowRxBaselineMetadataSize);
static constexpr size_t kEspNowRxMaxFrames =
    kEspNowRxBufSize / espNowRingItemStorage(kEspNowRxMetadataSize);
static_assert(kEspNowRxMaxFrames >= kEspNowRxBaselineMaxFrames,
              "correlation metadata must not reduce maximum-frame queue capacity");

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
    explicit EspNowTransport(IEspNowRadio& radio);
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
    TransportError sendTo(const uint8_t macAddr[kEspNowAddressSize], const uint8_t* data,
                          size_t len);

    /**
     * @brief Add a peer by MAC address
     * @param macAddr 6-byte MAC address
     * @return TransportError::kOk on success
     */
    TransportError addPeer(const uint8_t macAddr[kEspNowAddressSize]);

    /**
     * @brief Remove a peer
     * @param macAddr 6-byte MAC address
     * @return TransportError::kOk on success
     */
    TransportError removePeer(const uint8_t macAddr[kEspNowAddressSize]);

    /**
     * @brief Get number of registered peers
     */
    uint8_t getPeerCount() const { return peerCount_.load(std::memory_order_relaxed); }

    /**
     * @brief Get TX packet count
     */
    uint32_t getTxCount() const { return txCount_.load(std::memory_order_relaxed); }

    /**
     * @brief Get RX packet count
     */
    uint32_t getRxCount() const { return rxCount_.load(std::memory_order_relaxed); }

    /**
     * @brief Get TX failure count
     */
    uint32_t getTxFailCount() const { return txFailCount_.load(std::memory_order_relaxed); }

    EspNowCorrelationToken lastReceivedCorrelationToken() const {
        return lastReceivedToken_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the RSSI attached to the most recently dequeued frame
     * @param rssi Receives the RSSI in dBm when available
     * @return true when ESP-IDF supplied RSSI metadata for that frame
     */
    bool lastReceivedRssi(int8_t& rssi) const {
        if (!lastReceivedRssiValid_.load(std::memory_order_acquire)) {
            return false;
        }
        rssi = static_cast<int8_t>(lastReceivedRssi_.load(std::memory_order_relaxed));
        return true;
    }

    /**
     * @brief Get the radio source MAC attached to the most recently dequeued frame
     * @param sourceMac Receives the 6-byte source MAC
     * @return true when ESP-IDF supplied source metadata for that frame
     */
    bool lastReceivedSource(uint8_t sourceMac[kEspNowAddressSize]) const {
        if (!sourceMac || !lastReceivedSourceValid_.load(std::memory_order_acquire)) {
            return false;
        }
        for (size_t i = 0; i < kEspNowAddressSize; ++i) {
            sourceMac[i] = lastReceivedSource_[i].load(std::memory_order_relaxed);
        }
        return true;
    }

private:
    static void radioReceiveCallback(void* context, EspNowCorrelationToken token,
                                     const EspNowReceiveMetadata& metadata, const uint8_t* data,
                                     size_t len);
    static void radioSendCallback(void* context, EspNowCorrelationToken token,
                                  const EspNowAddress& destination, EspNowRadioSendStatus status);
    void onReceive(EspNowCorrelationToken token, const EspNowReceiveMetadata& metadata,
                   const uint8_t* data, size_t len);
    void onSendComplete(EspNowCorrelationToken token, const EspNowAddress& destination,
                        EspNowRadioSendStatus status);
    TransportError sendToAddress(const EspNowAddress& address, const uint8_t* data, size_t len,
                                 uint32_t timeoutMs, bool requireAck);
    static EspNowCorrelationToken nextToken(std::atomic<EspNowCorrelationToken>& counter);
    void refreshPeerCount();

    IEspNowRadio& radio_;

    /// RX ring buffer
    RingbufHandle_t rxRingBuf_ = nullptr;

    /// Semaphore signaled when data is available in RX buffer
    SemaphoreHandle_t rxSemaphore_ = nullptr;

    /// Mutex for send operations
    SemaphoreHandle_t txMutex_ = nullptr;

    /// Send completion semaphore (signaled from send callback)
    SemaphoreHandle_t txDoneSemaphore_ = nullptr;

    /// Last send status (set in callback)
    std::atomic<EspNowRadioSendStatus> lastSendStatus_{EspNowRadioSendStatus::kFailure};
    std::atomic<EspNowCorrelationToken> txToken_{0};
    std::atomic<EspNowCorrelationToken> pendingTxToken_{0};

    /// A timed-out send has ambiguous completion; require disconnect/init before reuse.
    std::atomic<bool> txPoisoned_{false};

    /// State
    std::atomic<bool> initialized_{false};
    std::atomic<uint8_t> peerCount_{0};

    /// Packet counters for observability
    std::atomic<uint32_t> txCount_{0};
    std::atomic<uint32_t> rxCount_{0};
    std::atomic<uint32_t> txFailCount_{0};

    /// Metadata for the frame returned by the latest receive() call
    std::atomic<int32_t> lastReceivedRssi_{0};
    std::atomic<bool> lastReceivedRssiValid_{false};
    std::array<std::atomic<uint8_t>, kEspNowAddressSize> lastReceivedSource_{};
    std::atomic<bool> lastReceivedSourceValid_{false};
    std::atomic<EspNowCorrelationToken> lastReceivedToken_{0};
};

}  // namespace domes
