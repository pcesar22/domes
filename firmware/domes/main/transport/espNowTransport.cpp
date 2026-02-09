/**
 * @file espNowTransport.cpp
 * @brief ESP-NOW transport implementation
 */

#include "espNowTransport.hpp"
#include "infra/logging.hpp"
#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kEspNow;

namespace domes {

// Global instance for ESP-NOW callbacks (ESP-NOW uses C callbacks)
static EspNowTransport* g_espNowTransport = nullptr;

// ============================================================================
// ESP-NOW C Callbacks (route to singleton)
// ============================================================================

static void espNowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (g_espNowTransport) {
        g_espNowTransport->onReceive(info, data, len);
    }
}

static void espNowSendCb(const uint8_t* macAddr, esp_now_send_status_t status) {
    if (g_espNowTransport) {
        g_espNowTransport->onSendComplete(macAddr, status);
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

    ESP_LOGI(kTag, "Initializing ESP-NOW transport");

    // Set global instance
    g_espNowTransport = this;

    // Create RX ring buffer
    rxRingBuf_ = xRingbufferCreate(kEspNowRxBufSize, RINGBUF_TYPE_NOSPLIT);
    if (!rxRingBuf_) {
        ESP_LOGE(kTag, "Failed to create RX ring buffer");
        return TransportError::kNoMemory;
    }

    // Create semaphores
    rxSemaphore_ = xSemaphoreCreateBinary();
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

    initialized_ = true;
    ESP_LOGI(kTag, "ESP-NOW transport initialized (broadcast peer added)");

    TRACE_INSTANT(TRACE_ID("EspNow.Initialized"), trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::send(const uint8_t* data, size_t len) {
    TRACE_SCOPE(TRACE_ID("EspNow.Send"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (!data || len == 0) {
        return TransportError::kInvalidArg;
    }

    if (len > kEspNowMaxPayload) {
        ESP_LOGW(kTag, "Payload too large: %zu > %zu", len, kEspNowMaxPayload);
        return TransportError::kInvalidArg;
    }

    // Take TX mutex for thread-safe sending
    if (xSemaphoreTake(txMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return TransportError::kTimeout;
    }

    // Send to broadcast address
    esp_err_t err = esp_now_send(kEspNowBroadcastAddr, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_now_send failed: %s", esp_err_to_name(err));
        xSemaphoreGive(txMutex_);
        return TransportError::kIoError;
    }

    // Wait for send callback (1 second timeout)
    if (xSemaphoreTake(txDoneSemaphore_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(kTag, "Send callback timeout");
        xSemaphoreGive(txMutex_);
        return TransportError::kTimeout;
    }

    xSemaphoreGive(txMutex_);

    // Check send result
    if (lastSendStatus_.load() != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(kTag, "Send failed (no ACK)");
        TRACE_INSTANT(TRACE_ID("EspNow.SendFail"), trace::Category::kEspNow);
        return TransportError::kIoError;
    }

    TRACE_COUNTER(TRACE_ID("EspNow.BytesSent"), static_cast<uint32_t>(len), trace::Category::kEspNow);
    return TransportError::kOk;
}

TransportError EspNowTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    TRACE_SCOPE(TRACE_ID("EspNow.Receive"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (!buf || !len || *len == 0) {
        return TransportError::kInvalidArg;
    }

    // Wait for data
    TickType_t ticks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
    if (xSemaphoreTake(rxSemaphore_, ticks) != pdTRUE) {
        *len = 0;
        return TransportError::kTimeout;
    }

    // Read from ring buffer
    size_t itemSize = 0;
    void* item = xRingbufferReceive(rxRingBuf_, &itemSize, 0);
    if (!item) {
        *len = 0;
        return TransportError::kTimeout;
    }

    size_t toCopy = (*len < itemSize) ? *len : itemSize;
    std::memcpy(buf, item, toCopy);
    vRingbufferReturnItem(rxRingBuf_, item);

    *len = toCopy;

    TRACE_COUNTER(TRACE_ID("EspNow.BytesReceived"), static_cast<uint32_t>(toCopy), trace::Category::kEspNow);
    return TransportError::kOk;
}

bool EspNowTransport::isConnected() const {
    return initialized_.load();
}

void EspNowTransport::disconnect() {
    if (initialized_.load()) {
        esp_now_deinit();
        initialized_ = false;
        TRACE_INSTANT(TRACE_ID("EspNow.Disconnected"), trace::Category::kEspNow);
    }

    g_espNowTransport = nullptr;

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
    ESP_LOGI(kTag, "ESP-NOW transport disconnected");
}

TransportError EspNowTransport::addPeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]) {
    TRACE_SCOPE(TRACE_ID("EspNow.AddPeer"), trace::Category::kEspNow);

    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, macAddr, ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGW(kTag, "Peer already exists");
        return TransportError::kOk;  // Not an error
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to add peer: %s", esp_err_to_name(err));
        return TransportError::kIoError;
    }

    peerCount_.fetch_add(1, std::memory_order_relaxed);
    ESP_LOGI(kTag, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
             macAddr[0], macAddr[1], macAddr[2],
             macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

TransportError EspNowTransport::removePeer(const uint8_t macAddr[ESP_NOW_ETH_ALEN]) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    esp_err_t err = esp_now_del_peer(macAddr);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to remove peer: %s", esp_err_to_name(err));
        return TransportError::kIoError;
    }

    uint8_t prev = peerCount_.fetch_sub(1, std::memory_order_relaxed);
    if (prev == 0) peerCount_ = 0;  // Guard underflow

    ESP_LOGI(kTag, "Peer removed: %02X:%02X:%02X:%02X:%02X:%02X",
             macAddr[0], macAddr[1], macAddr[2],
             macAddr[3], macAddr[4], macAddr[5]);
    return TransportError::kOk;
}

void EspNowTransport::onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    TRACE_INSTANT(TRACE_ID("EspNow.OnReceive"), trace::Category::kEspNow);

    if (!data || len <= 0) return;

    // Send to ring buffer (drop if full)
    BaseType_t ret = xRingbufferSend(rxRingBuf_, data, static_cast<size_t>(len), 0);
    if (ret != pdTRUE) {
        ESP_LOGW(kTag, "RX buffer full, dropping %d bytes", len);
        return;
    }

    // Signal data available
    xSemaphoreGive(rxSemaphore_);

    ESP_LOGD(kTag, "Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X",
             len,
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);
}

void EspNowTransport::onSendComplete(const uint8_t* macAddr, esp_now_send_status_t status) {
    lastSendStatus_.store(status);
    xSemaphoreGive(txDoneSemaphore_);

    if (status != ESP_NOW_SEND_SUCCESS) {
        TRACE_INSTANT(TRACE_ID("EspNow.SendCallbackFail"), trace::Category::kEspNow);
        ESP_LOGW(kTag, "Send to %02X:%02X:%02X:%02X:%02X:%02X failed",
                 macAddr[0], macAddr[1], macAddr[2],
                 macAddr[3], macAddr[4], macAddr[5]);
    }
}

}  // namespace domes
