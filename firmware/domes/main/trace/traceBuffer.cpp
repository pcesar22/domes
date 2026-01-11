/**
 * @file traceBuffer.cpp
 * @brief Ring buffer implementation for trace events
 */

#include "traceBuffer.hpp"

#include "esp_log.h"

#include <cstring>

namespace {
constexpr const char* kTag = "trace_buf";
}

namespace domes::trace {

TraceBuffer::TraceBuffer(size_t bufferSize)
    : ringBuf_(nullptr)
    , bufferSize_(bufferSize)
    , initialized_(false)
    , paused_(false)
    , droppedCount_(0) {
}

TraceBuffer::~TraceBuffer() {
    if (ringBuf_ != nullptr) {
        vRingbufferDelete(ringBuf_);
        ringBuf_ = nullptr;
    }
    initialized_.store(false);
}

esp_err_t TraceBuffer::init() {
    if (initialized_.load()) {
        return ESP_ERR_INVALID_STATE;
    }

    // Create ring buffer with no-split type for fixed-size events
    // RINGBUF_TYPE_NOSPLIT ensures events are not split across wrap boundary
    ringBuf_ = xRingbufferCreate(bufferSize_, RINGBUF_TYPE_NOSPLIT);
    if (ringBuf_ == nullptr) {
        ESP_LOGE(kTag, "Failed to create ring buffer (%zu bytes)", bufferSize_);
        return ESP_ERR_NO_MEM;
    }

    initialized_.store(true);
    ESP_LOGI(kTag, "Trace buffer initialized (%zu bytes, ~%zu events)",
             bufferSize_, bufferSize_ / kEventSize);

    return ESP_OK;
}

bool TraceBuffer::record(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load()) {
        return false;
    }

    // Try to send with no wait (don't block if full)
    BaseType_t result = xRingbufferSend(
        ringBuf_,
        &event,
        kEventSize,
        0  // No wait
    );

    if (result != pdTRUE) {
        droppedCount_.fetch_add(1);
        return false;
    }

    return true;
}

bool TraceBuffer::recordFromIsr(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load()) {
        return false;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xRingbufferSendFromISR(
        ringBuf_,
        &event,
        kEventSize,
        &higherPriorityTaskWoken
    );

    if (result != pdTRUE) {
        droppedCount_.fetch_add(1);
        return false;
    }

    // Yield if a higher priority task was woken
    portYIELD_FROM_ISR(higherPriorityTaskWoken);

    return true;
}

bool TraceBuffer::read(TraceEvent* event, uint32_t timeoutMs) {
    if (!initialized_.load() || event == nullptr) {
        return false;
    }

    size_t itemSize = 0;
    void* item = xRingbufferReceive(
        ringBuf_,
        &itemSize,
        pdMS_TO_TICKS(timeoutMs)
    );

    if (item == nullptr) {
        return false;
    }

    // Verify size matches
    if (itemSize != kEventSize) {
        ESP_LOGW(kTag, "Unexpected event size: %zu (expected %zu)",
                 itemSize, kEventSize);
        vRingbufferReturnItem(ringBuf_, item);
        return false;
    }

    // Copy event data
    std::memcpy(event, item, kEventSize);

    // Return item to ring buffer
    vRingbufferReturnItem(ringBuf_, item);

    return true;
}

size_t TraceBuffer::count() const {
    if (!initialized_.load()) {
        return 0;
    }

    // Get free space and estimate event count
    size_t freeSize = xRingbufferGetCurFreeSize(ringBuf_);
    if (freeSize >= bufferSize_) {
        return 0;
    }

    // Account for ring buffer overhead (8 bytes per item for NOSPLIT type)
    constexpr size_t kItemOverhead = 8;
    size_t usedSize = bufferSize_ - freeSize;
    return usedSize / (kEventSize + kItemOverhead);
}

void TraceBuffer::clear() {
    if (!initialized_.load()) {
        return;
    }

    // Drain all items from buffer
    size_t itemSize = 0;
    void* item;
    while ((item = xRingbufferReceive(ringBuf_, &itemSize, 0)) != nullptr) {
        vRingbufferReturnItem(ringBuf_, item);
    }

    droppedCount_.store(0);
    ESP_LOGD(kTag, "Trace buffer cleared");
}

}  // namespace domes::trace
