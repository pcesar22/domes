/**
 * @file espNowTransport.cpp
 * @brief ESP-NOW transport implementation
 */

#include "espNowTransport.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "infra/logging.hpp"
#include "trace/traceApi.hpp"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kEspNow;

namespace domes {

// ============================================================================
// EspNowTransport Implementation
// ============================================================================

EspNowTransport::EspNowTransport(IEspNowRadio& radio) : radio_(radio) {}

EspNowTransport::~EspNowTransport() {
    disconnect();
}

TransportError EspNowTransport::init() {
    TRACE_SCOPE(TRACE_ID("EspNow.Init"), trace::Category::kEspNow);

    if (initialized_.load()) {
        return TransportError::kAlreadyInit;
    }

    txPoisoned_.store(false, std::memory_order_release);

    ESP_LOGI(kTag, "Initializing ESP-NOW transport");

    // Create RX ring buffer
    rxRingBuf_ = xRingbufferCreate(kEspNowRxBufSize, RINGBUF_TYPE_NOSPLIT);
    if (!rxRingBuf_) {
        ESP_LOGE(kTag, "Failed to create RX ring buffer");
        return TransportError::kNoMemory;
    }

    // Create semaphores
    // Counting semaphore: each onReceive() gives once, receive() takes once.
    // Binary semaphore would silently drop signals when multiple messages arrive
    // before the consumer reads — causing stuck messages in the ring buffer.
    rxSemaphore_ = xSemaphoreCreateCounting(kEspNowRxMaxFrames, 0);
    txMutex_ = xSemaphoreCreateMutex();
    txDoneSemaphore_ = xSemaphoreCreateBinary();

    if (!rxSemaphore_ || !txMutex_ || !txDoneSemaphore_) {
        ESP_LOGE(kTag, "Failed to create semaphores");
        disconnect();
        return TransportError::kNoMemory;
    }

    if (radio_.init(this, radioReceiveCallback, radioSendCallback) != EspNowRadioResult::kOk) {
        ESP_LOGE(kTag, "ESP-NOW radio initialization failed");
        disconnect();
        return TransportError::kIoError;
    }

    // Add broadcast peer by default
    const EspNowRadioResult result = radio_.addPeer(kEspNowBroadcastAddress);
    if (result != EspNowRadioResult::kOk && result != EspNowRadioResult::kAlreadyExists) {
        ESP_LOGE(kTag, "Failed to add ESP-NOW broadcast peer");
        disconnect();
        return TransportError::kIoError;
    }

    refreshPeerCount();
    initialized_.store(true, std::memory_order_release);
    ESP_LOGI(kTag, "ESP-NOW transport initialized (broadcast peer added)");

    TRACE_INSTANT(TRACE_ID("EspNow.Initialized"), trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::send(const uint8_t* data, size_t len) {
    return sendToAddress(kEspNowBroadcastAddress, data, len, 500, false);
}

TransportError EspNowTransport::sendTo(const uint8_t macAddr[kEspNowAddressSize],
                                       const uint8_t* data, size_t len) {
    if (!macAddr) {
        return TransportError::kInvalidArg;
    }
    EspNowAddress address{};
    std::memcpy(address.data(), macAddr, address.size());
    return sendToAddress(address, data, len, 1000, true);
}

TransportError EspNowTransport::sendToAddress(const EspNowAddress& address, const uint8_t* data,
                                              size_t len, uint32_t timeoutMs, bool requireAck) {
    TRACE_SCOPE(TRACE_ID("EspNow.Send"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (txPoisoned_.load(std::memory_order_acquire)) {
        return TransportError::kDisconnected;
    }

    if (!data || len == 0) {
        return TransportError::kInvalidArg;
    }

    if (len > kEspNowMaxPayload) {
        ESP_LOGW(kTag, "Payload too large: %zu > %zu", len, kEspNowMaxPayload);
        return TransportError::kInvalidArg;
    }

    {
        uint32_t t0 = static_cast<uint32_t>(esp_timer_get_time());
        if (xSemaphoreTake(txMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return TransportError::kTimeout;
        }
        uint32_t waited = static_cast<uint32_t>(esp_timer_get_time()) - t0;
        TRACE_MUTEX_LOCK(TRACE_ID("EspNow.TxMutex"));
        if (waited > 100) {
            TRACE_MUTEX_CONTENTION(TRACE_ID("EspNow.TxMutex"), waited);
        }
    }

    if (txPoisoned_.load(std::memory_order_acquire)) {
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kDisconnected;
    }

    xSemaphoreTake(txDoneSemaphore_, 0);

    const EspNowCorrelationToken token = nextToken(txToken_);
    pendingTxToken_.store(token, std::memory_order_release);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSchedQueueSend,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.TxSubmit"), token));
    }
    const uint32_t sendStartTick = xTaskGetTickCount();
    if (radio_.send(address, data, len, token) != EspNowRadioResult::kOk) {
        pendingTxToken_.store(0, std::memory_order_release);
        ESP_LOGE(kTag, "ESP-NOW synchronous send submission failed");
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kIoError;
    }

    if (xSemaphoreTake(txDoneSemaphore_, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        ESP_LOGW(kTag, "ESP-NOW send callback timeout");
        txPoisoned_.store(true, std::memory_order_release);
        txFailCount_.fetch_add(1, std::memory_order_relaxed);
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kTimeout;
    }

    uint32_t sendLatencyMs = (xTaskGetTickCount() - sendStartTick) * portTICK_PERIOD_MS;
    TRACE_COUNTER(TRACE_ID("EspNow.SendLatencyMs"), sendLatencyMs, trace::Category::kEspNow);

    TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
    xSemaphoreGive(txMutex_);

    if (requireAck && lastSendStatus_.load() != EspNowRadioSendStatus::kSuccess) {
        ESP_LOGW(kTag, "Unicast send failed (no ACK)");
        txFailCount_.fetch_add(1, std::memory_order_relaxed);
        TRACE_INSTANT(TRACE_ID("EspNow.SendFail"), trace::Category::kEspNow);
        return TransportError::kIoError;
    }

    txCount_.fetch_add(1, std::memory_order_relaxed);
    TRACE_COUNTER(TRACE_ID("EspNow.BytesSent"), static_cast<uint32_t>(len),
                  trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (!buf || !len || *len == 0) {
        return TransportError::kInvalidArg;
    }

    // Try non-blocking read first — items may already be queued from prior signals
    size_t itemSize = 0;
    void* item = xRingbufferReceive(rxRingBuf_, &itemSize, 0);

    if (item) {
        // Keep the counting semaphore in step with direct queue reads.
        xSemaphoreTake(rxSemaphore_, 0);
    } else {
        // Ring buffer empty — block on semaphore for new data
        TickType_t ticks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
        if (xSemaphoreTake(rxSemaphore_, ticks) != pdTRUE) {
            *len = 0;
            return TransportError::kTimeout;
        }

        item = xRingbufferReceive(rxRingBuf_, &itemSize, 0);
        if (!item) {
            *len = 0;
            return TransportError::kTimeout;
        }
    }

    if (itemSize < kEspNowRxMetadataSize) {
        vRingbufferReturnItem(rxRingBuf_, item);
        *len = 0;
        lastReceivedRssiValid_.store(false, std::memory_order_release);
        lastReceivedSourceValid_.store(false, std::memory_order_release);
        return TransportError::kIoError;
    }

    const auto* queued = static_cast<const uint8_t*>(item);
    const bool hasRssi = queued[kEspNowRxRssiValidOffset] != 0;
    lastReceivedRssi_.store(static_cast<int8_t>(queued[kEspNowRxRssiOffset]),
                            std::memory_order_relaxed);
    lastReceivedRssiValid_.store(hasRssi, std::memory_order_release);

    lastReceivedSourceValid_.store(false, std::memory_order_relaxed);
    const bool hasSource = queued[kEspNowRxSourceValidOffset] != 0;
    for (size_t i = 0; i < kEspNowAddressSize; ++i) {
        lastReceivedSource_[i].store(queued[kEspNowRxSourceOffset + i], std::memory_order_relaxed);
    }
    lastReceivedSourceValid_.store(hasSource, std::memory_order_release);

    EspNowCorrelationToken token = 0;
    std::memcpy(&token, queued + kEspNowRxTokenOffset, sizeof(token));
    lastReceivedToken_.store(token, std::memory_order_release);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSemTake,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxReady"), token));
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSchedQueueReceive,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxQueue"), token));
    }

    const size_t payloadSize = itemSize - kEspNowRxMetadataSize;
    size_t toCopy = (*len < payloadSize) ? *len : payloadSize;
    std::memcpy(buf, queued + kEspNowRxMetadataSize, toCopy);
    vRingbufferReturnItem(rxRingBuf_, item);

    *len = toCopy;

    TRACE_COUNTER(TRACE_ID("EspNow.BytesReceived"), static_cast<uint32_t>(toCopy),
                  trace::Category::kEspNow);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCausalComplete,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxDispatch"), token));
    }
    return TransportError::kOk;
}

bool EspNowTransport::isConnected() const {
    return initialized_.load(std::memory_order_acquire) &&
           !txPoisoned_.load(std::memory_order_acquire);
}

void EspNowTransport::disconnect() {
    const bool wasInitialized = initialized_.exchange(false, std::memory_order_acq_rel);
    // The adapter revokes callback ownership before vendor teardown.
    radio_.deinit();
    if (wasInitialized) {
        TRACE_INSTANT(TRACE_ID("EspNow.Disconnected"), trace::Category::kEspNow);
    }

    if (rxRingBuf_) {
        vRingbufferDelete(rxRingBuf_);
        rxRingBuf_ = nullptr;
    }
    if (rxSemaphore_) {
        vSemaphoreDelete(rxSemaphore_);
        rxSemaphore_ = nullptr;
    }
    if (txMutex_) {
        vSemaphoreDelete(txMutex_);
        txMutex_ = nullptr;
    }
    if (txDoneSemaphore_) {
        vSemaphoreDelete(txDoneSemaphore_);
        txDoneSemaphore_ = nullptr;
    }

    peerCount_ = 0;
    // Packet counters are boot-lifetime observability and survive service-level recovery.
    txPoisoned_ = false;
    lastReceivedRssi_ = 0;
    lastReceivedRssiValid_ = false;
    for (auto& byte : lastReceivedSource_) {
        byte.store(0, std::memory_order_relaxed);
    }
    lastReceivedSourceValid_ = false;
    lastReceivedToken_ = 0;
    pendingTxToken_ = 0;
    ESP_LOGI(kTag, "ESP-NOW transport disconnected");
}

TransportError EspNowTransport::addPeer(const uint8_t macAddr[kEspNowAddressSize]) {
    TRACE_SCOPE(TRACE_ID("EspNow.AddPeer"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (!macAddr) {
        return TransportError::kInvalidArg;
    }

    EspNowAddress address{};
    std::memcpy(address.data(), macAddr, address.size());
    const EspNowRadioResult result = radio_.addPeer(address);
    if (result == EspNowRadioResult::kAlreadyExists) {
        refreshPeerCount();
        ESP_LOGD(kTag, "Peer already exists");
        return TransportError::kOk;  // Not an error
    }
    if (result != EspNowRadioResult::kOk) {
        ESP_LOGE(kTag, "Failed to add ESP-NOW peer");
        return TransportError::kIoError;
    }

    refreshPeerCount();
    ESP_LOGI(kTag, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1], macAddr[2],
             macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

TransportError EspNowTransport::removePeer(const uint8_t macAddr[kEspNowAddressSize]) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (!macAddr) {
        return TransportError::kInvalidArg;
    }

    EspNowAddress address{};
    std::memcpy(address.data(), macAddr, address.size());
    const EspNowRadioResult result = radio_.removePeer(address);
    if (result == EspNowRadioResult::kNotFound) {
        refreshPeerCount();
        return TransportError::kOk;
    }
    if (result != EspNowRadioResult::kOk) {
        ESP_LOGE(kTag, "Failed to remove ESP-NOW peer");
        return TransportError::kIoError;
    }

    refreshPeerCount();

    ESP_LOGI(kTag, "Peer removed: %02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1],
             macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

void EspNowTransport::radioReceiveCallback(void* context, EspNowCorrelationToken token,
                                           const EspNowReceiveMetadata& metadata,
                                           const uint8_t* data, size_t len) {
    static_cast<EspNowTransport*>(context)->onReceive(token, metadata, data, len);
}

void EspNowTransport::radioSendCallback(void* context, EspNowCorrelationToken token,
                                        const EspNowAddress& destination,
                                        EspNowRadioSendStatus status) {
    static_cast<EspNowTransport*>(context)->onSendComplete(token, destination, status);
}

void EspNowTransport::onReceive(EspNowCorrelationToken token, const EspNowReceiveMetadata& metadata,
                                const uint8_t* data, size_t len) {
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackBegin,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxCallback"), token));
    }

    if (!initialized_.load(std::memory_order_acquire) || rxRingBuf_ == nullptr ||
        rxSemaphore_ == nullptr || !data || len == 0 || len > kEspNowMaxPayload) {
        if (trace::Recorder::isEnabled()) {
            trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackEnd,
                                                     trace::Category::kEspNow,
                                                     TRACE_ID("EspNow.RxCallback"), token));
        }
        return;
    }

    std::array<uint8_t, kEspNowRxMetadataSize + kEspNowMaxPayload> queued = {};
    queued[kEspNowRxRssiValidOffset] = metadata.rssiValid ? 1 : 0;
    queued[kEspNowRxRssiOffset] = metadata.rssiValid ? static_cast<uint8_t>(metadata.rssi) : 0;
    queued[kEspNowRxSourceValidOffset] = metadata.sourceValid ? 1 : 0;
    if (metadata.sourceValid) {
        std::memcpy(queued.data() + kEspNowRxSourceOffset, metadata.source.data(),
                    metadata.source.size());
    }
    std::memcpy(queued.data() + kEspNowRxTokenOffset, &token, sizeof(token));
    std::memcpy(queued.data() + kEspNowRxMetadataSize, data, static_cast<size_t>(len));

    // Keep radio metadata and payload in the same queue item so RSSI cannot be
    // paired with a later frame when the consumer falls behind.
    BaseType_t ret = xRingbufferSend(rxRingBuf_, queued.data(),
                                     kEspNowRxMetadataSize + static_cast<size_t>(len), 0);
    if (ret != pdTRUE) {
        ESP_LOGW(kTag, "RX buffer full, dropping %zu bytes", len);
        if (trace::Recorder::isEnabled()) {
            trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackEnd,
                                                     trace::Category::kEspNow,
                                                     TRACE_ID("EspNow.RxCallback"), token));
        }
        return;
    }

    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSchedQueueSend,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxQueue"), token));
    }

    // Signal data available
    xSemaphoreGive(rxSemaphore_);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSemGive,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxReady"), token));
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackEnd,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.RxCallback"), token));
    }
    rxCount_.fetch_add(1, std::memory_order_relaxed);

    if (metadata.sourceValid) {
        ESP_LOGD(kTag, "Received %zu bytes from %02X:%02X:%02X:%02X:%02X:%02X", len,
                 metadata.source[0], metadata.source[1], metadata.source[2], metadata.source[3],
                 metadata.source[4], metadata.source[5]);
    }
}

void EspNowTransport::refreshPeerCount() {
    EspNowPeerCounts counts{};
    if (radio_.getPeerCounts(counts) != EspNowRadioResult::kOk) {
        ESP_LOGW(kTag, "Failed to query ESP-NOW peer count");
        return;
    }

    int unicastCount = counts.total;
    if (radio_.peerExists(kEspNowBroadcastAddress) && unicastCount > 0) {
        --unicastCount;
    }
    peerCount_.store(static_cast<uint8_t>(unicastCount), std::memory_order_relaxed);
}

void EspNowTransport::onSendComplete(EspNowCorrelationToken token, const EspNowAddress& destination,
                                     EspNowRadioSendStatus status) {
    // Once a wait times out, callback ownership is ambiguous. Ignore every
    // completion until disconnect/init creates a fresh transport session.
    if (!initialized_.load(std::memory_order_acquire) ||
        txPoisoned_.load(std::memory_order_acquire) || txDoneSemaphore_ == nullptr) {
        return;
    }
    EspNowCorrelationToken expected = token;
    if (!pendingTxToken_.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
        return;
    }

    lastSendStatus_.store(status);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackBegin,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.TxCallback"), token));
    }
    xSemaphoreGive(txDoneSemaphore_);

    if (status != EspNowRadioSendStatus::kSuccess) {
        TRACE_INSTANT(TRACE_ID("EspNow.SendCallbackFail"), trace::Category::kEspNow);
        ESP_LOGW(kTag, "Send to %02X:%02X:%02X:%02X:%02X:%02X failed", destination[0],
                 destination[1], destination[2], destination[3], destination[4], destination[5]);
    }
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCallbackEnd,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.TxCallback"), token));
        trace::Recorder::record(trace::makeEvent(trace::EventType::kCausalComplete,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("EspNow.TxComplete"), token));
    }
}

EspNowCorrelationToken EspNowTransport::nextToken(std::atomic<EspNowCorrelationToken>& counter) {
    EspNowCorrelationToken token = counter.fetch_add(1, std::memory_order_relaxed) + 1;
    if (token == 0) {
        token = counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    return token;
}

}  // namespace domes
