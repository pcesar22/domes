/**
 * @file espNowTransport.cpp
 * @brief ESP-NOW transport implementation
 */

#include "espNowTransport.hpp"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "infra/logging.hpp"
#include "trace/traceApi.hpp"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kEspNow;

namespace domes {

// Global instance for ESP-NOW callbacks (ESP-NOW uses C callbacks)
static std::atomic<EspNowTransport*> g_espNowTransport{nullptr};

// ============================================================================
// ESP-NOW C Callbacks (route to singleton)
// ============================================================================

static void espNowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (auto* transport = g_espNowTransport.load(std::memory_order_acquire)) {
        transport->onReceive(info, data, len);
    }
}

static void espNowSendCb(const uint8_t* macAddr, esp_now_send_status_t status) {
    if (auto* transport = g_espNowTransport.load(std::memory_order_acquire)) {
        transport->onSendComplete(macAddr, status);
    }
}

// ============================================================================
// EspNowTransport Implementation
// ============================================================================

EspNowTransport::EspNowTransport() = default;

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
    rxSemaphore_ = xSemaphoreCreateCounting(32, 0);
    txMutex_ = xSemaphoreCreateMutex();
    txDoneSemaphore_ = xSemaphoreCreateBinary();

    if (!rxSemaphore_ || !txMutex_ || !txDoneSemaphore_) {
        ESP_LOGE(kTag, "Failed to create semaphores");
        disconnect();
        return TransportError::kNoMemory;
    }

    // Initialize ESP-NOW
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_now_init failed: %s", esp_err_to_name(err));
        disconnect();
        return TransportError::kIoError;
    }

    // Register callbacks
    err = esp_now_register_recv_cb(espNowRecvCb);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register recv callback: %s", esp_err_to_name(err));
        esp_now_deinit();
        disconnect();
        return TransportError::kIoError;
    }

    err = esp_now_register_send_cb(espNowSendCb);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register send callback: %s", esp_err_to_name(err));
        esp_now_deinit();
        disconnect();
        return TransportError::kIoError;
    }

    // Add broadcast peer by default
    esp_now_peer_info_t broadcastPeer = {};
    std::memcpy(broadcastPeer.peer_addr, kEspNowBroadcastAddr, ESP_NOW_ETH_ALEN);
    broadcastPeer.channel = 0;  // Use current channel
    broadcastPeer.encrypt = false;

    err = esp_now_add_peer(&broadcastPeer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to add broadcast peer: %s", esp_err_to_name(err));
        esp_now_deinit();
        disconnect();
        return TransportError::kIoError;
    }

    refreshPeerCount();
    initialized_.store(true, std::memory_order_release);
    g_espNowTransport.store(this, std::memory_order_release);
    ESP_LOGI(kTag, "ESP-NOW transport initialized (broadcast peer added)");

    TRACE_INSTANT(TRACE_ID("EspNow.Initialized"), trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::send(const uint8_t* data, size_t len) {
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

    // Take TX mutex for thread-safe sending
    {
        uint32_t t0 = static_cast<uint32_t>(esp_timer_get_time());
        if (xSemaphoreTake(txMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return TransportError::kTimeout;
        }
        uint32_t waited = static_cast<uint32_t>(esp_timer_get_time()) - t0;
        TRACE_MUTEX_LOCK(TRACE_ID("EspNow.TxMutex"));
        if (waited > 100) {  // Log contention > 100us
            TRACE_MUTEX_CONTENTION(TRACE_ID("EspNow.TxMutex"), waited);
        }
    }

    if (txPoisoned_.load(std::memory_order_acquire)) {
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kDisconnected;
    }

    // Each fresh transport session starts a send with an empty completion semaphore.
    xSemaphoreTake(txDoneSemaphore_, 0);

    // Send to broadcast address
    uint32_t sendStartTick = xTaskGetTickCount();
    esp_err_t err = esp_now_send(kEspNowBroadcastAddr, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_now_send failed: %s", esp_err_to_name(err));
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kIoError;
    }

    // Wait for send callback. Broadcast has no MAC-level ACK so the callback
    // fires when the frame leaves the radio. 500ms accommodates BLE advertising
    // contention — fast-interval advertising can delay ESP-NOW by >50ms.
    if (xSemaphoreTake(txDoneSemaphore_, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(kTag, "Broadcast send callback timeout");
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

    // Broadcast always reports success (no peer ACK), so don't check status.
    txCount_.fetch_add(1, std::memory_order_relaxed);
    TRACE_COUNTER(TRACE_ID("EspNow.BytesSent"), static_cast<uint32_t>(len),
                  trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::sendTo(const uint8_t macAddr[ESP_NOW_ETH_ALEN], const uint8_t* data,
                                       size_t len) {
    TRACE_SCOPE(TRACE_ID("EspNow.Send"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (txPoisoned_.load(std::memory_order_acquire)) {
        return TransportError::kDisconnected;
    }

    if (!macAddr || !data || len == 0) {
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

    // Start the send with an empty completion semaphore.
    xSemaphoreTake(txDoneSemaphore_, 0);

    uint32_t sendStartTick = xTaskGetTickCount();
    esp_err_t err = esp_now_send(macAddr, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_now_send (unicast) failed: %s", esp_err_to_name(err));
        TRACE_MUTEX_UNLOCK(TRACE_ID("EspNow.TxMutex"));
        xSemaphoreGive(txMutex_);
        return TransportError::kIoError;
    }

    if (xSemaphoreTake(txDoneSemaphore_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(kTag, "Send callback timeout (unicast)");
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

    if (lastSendStatus_.load() != ESP_NOW_SEND_SUCCESS) {
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
    for (size_t i = 0; i < ESP_NOW_ETH_ALEN; ++i) {
        lastReceivedSource_[i].store(queued[kEspNowRxSourceOffset + i], std::memory_order_relaxed);
    }
    lastReceivedSourceValid_.store(hasSource, std::memory_order_release);

    const size_t payloadSize = itemSize - kEspNowRxMetadataSize;
    size_t toCopy = (*len < payloadSize) ? *len : payloadSize;
    std::memcpy(buf, queued + kEspNowRxMetadataSize, toCopy);
    vRingbufferReturnItem(rxRingBuf_, item);

    *len = toCopy;

    TRACE_COUNTER(TRACE_ID("EspNow.BytesReceived"), static_cast<uint32_t>(toCopy),
                  trace::Category::kEspNow);
    return TransportError::kOk;
}

bool EspNowTransport::isConnected() const {
    return initialized_.load(std::memory_order_acquire) &&
           !txPoisoned_.load(std::memory_order_acquire);
}

void EspNowTransport::disconnect() {
    // Stop callback routing before ESP-NOW teardown so a late completion cannot
    // signal semaphores belonging to a subsequent transport generation.
    g_espNowTransport.store(nullptr, std::memory_order_release);
    if (initialized_.exchange(false, std::memory_order_acq_rel)) {
        esp_now_deinit();
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
    ESP_LOGI(kTag, "ESP-NOW transport disconnected");
}

TransportError EspNowTransport::addPeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]) {
    TRACE_SCOPE(TRACE_ID("EspNow.AddPeer"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (!macAddr) {
        return TransportError::kInvalidArg;
    }

    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, macAddr, ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        refreshPeerCount();
        ESP_LOGD(kTag, "Peer already exists");
        return TransportError::kOk;  // Not an error
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to add peer: %s", esp_err_to_name(err));
        return TransportError::kIoError;
    }

    refreshPeerCount();
    ESP_LOGI(kTag, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1], macAddr[2],
             macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

TransportError EspNowTransport::removePeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }
    if (!macAddr) {
        return TransportError::kInvalidArg;
    }

    esp_err_t err = esp_now_del_peer(macAddr);
    if (err == ESP_ERR_ESPNOW_NOT_FOUND) {
        refreshPeerCount();
        return TransportError::kOk;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to remove peer: %s", esp_err_to_name(err));
        return TransportError::kIoError;
    }

    refreshPeerCount();

    ESP_LOGI(kTag, "Peer removed: %02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1],
             macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

void EspNowTransport::onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    TRACE_INSTANT(TRACE_ID("EspNow.OnReceive"), trace::Category::kEspNow);

    if (!initialized_.load(std::memory_order_acquire) || rxRingBuf_ == nullptr ||
        rxSemaphore_ == nullptr || !data || len <= 0 ||
        static_cast<size_t>(len) > kEspNowMaxPayload) {
        return;
    }

    std::array<uint8_t, kEspNowRxMetadataSize + kEspNowMaxPayload> queued = {};
    const bool hasRssi = info && info->rx_ctrl;
    const bool hasSource = info && info->src_addr;
    queued[kEspNowRxRssiValidOffset] = hasRssi ? 1 : 0;
    queued[kEspNowRxRssiOffset] = hasRssi ? static_cast<uint8_t>(info->rx_ctrl->rssi) : 0;
    queued[kEspNowRxSourceValidOffset] = hasSource ? 1 : 0;
    if (hasSource) {
        std::memcpy(queued.data() + kEspNowRxSourceOffset, info->src_addr, ESP_NOW_ETH_ALEN);
    }
    std::memcpy(queued.data() + kEspNowRxMetadataSize, data, static_cast<size_t>(len));

    // Keep radio metadata and payload in the same queue item so RSSI cannot be
    // paired with a later frame when the consumer falls behind.
    BaseType_t ret = xRingbufferSend(rxRingBuf_, queued.data(),
                                     kEspNowRxMetadataSize + static_cast<size_t>(len), 0);
    if (ret != pdTRUE) {
        ESP_LOGW(kTag, "RX buffer full, dropping %d bytes", len);
        return;
    }

    // Signal data available
    xSemaphoreGive(rxSemaphore_);
    rxCount_.fetch_add(1, std::memory_order_relaxed);

    if (hasSource) {
        ESP_LOGD(kTag, "Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X", len,
                 info->src_addr[0], info->src_addr[1], info->src_addr[2], info->src_addr[3],
                 info->src_addr[4], info->src_addr[5]);
    }
}

void EspNowTransport::refreshPeerCount() {
    esp_now_peer_num_t counts = {};
    esp_err_t err = esp_now_get_peer_num(&counts);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to query peer count: %s", esp_err_to_name(err));
        return;
    }

    int unicastCount = counts.total_num;
    if (esp_now_is_peer_exist(kEspNowBroadcastAddr) && unicastCount > 0) {
        --unicastCount;
    }
    peerCount_.store(static_cast<uint8_t>(unicastCount), std::memory_order_relaxed);
}

void EspNowTransport::onSendComplete(const uint8_t* macAddr, esp_now_send_status_t status) {
    // Once a wait times out, callback ownership is ambiguous. Ignore every
    // completion until disconnect/init creates a fresh transport session.
    if (!initialized_.load(std::memory_order_acquire) ||
        txPoisoned_.load(std::memory_order_acquire) || txDoneSemaphore_ == nullptr) {
        return;
    }

    lastSendStatus_.store(status);
    xSemaphoreGive(txDoneSemaphore_);

    if (status != ESP_NOW_SEND_SUCCESS) {
        TRACE_INSTANT(TRACE_ID("EspNow.SendCallbackFail"), trace::Category::kEspNow);
        ESP_LOGW(kTag, "Send to %02X:%02X:%02X:%02X:%02X:%02X failed", macAddr[0], macAddr[1],
                 macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    }
}

}  // namespace domes
