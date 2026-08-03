/**
 * @file traceBuffer.cpp
 * @brief Ring buffer implementation for trace events
 */

#include "traceBuffer.hpp"

#include "esp_log.h"
#include "freertos/task.h"

#include <cstring>

namespace {
constexpr const char* kTag = "trace_buf";
}

namespace domes::trace {

TraceBuffer::TraceBuffer(size_t bufferSize)
    : ringBuf_(nullptr),
      bufferSize_(bufferSize),
      initialized_(false),
      paused_(false),
      droppedCount_(0),
      eventCount_(0) {}

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

    // Allocate the fixed-capacity buffer once at initialization. No-split mode
    // keeps each fixed-size event contiguous across the wrap boundary.
    ringBuf_ = xRingbufferCreate(bufferSize_, RINGBUF_TYPE_NOSPLIT);
    if (ringBuf_ == nullptr) {
        ESP_LOGE(kTag, "Failed to create ring buffer (%zu bytes)", bufferSize_);
        return ESP_ERR_NO_MEM;
    }

    initialized_.store(true);
    ESP_LOGI(kTag, "Trace buffer initialized (%zu bytes, ~%zu events)", bufferSize_,
             bufferSize_ / kEventSize);

    return ESP_OK;
}

bool TraceBuffer::record(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load()) {
        return false;
    }

    activeWriters_.fetch_add(1);
    if (paused_.load()) {
        activeWriters_.fetch_sub(1);
        return false;
    }

    // Try to send with no wait (don't block if full)
    BaseType_t result = xRingbufferSend(ringBuf_, &event, kEventSize,
                                        0  // No wait
    );

    if (result != pdTRUE) {
        droppedCount_.fetch_add(1);
        activeWriters_.fetch_sub(1);
        return false;
    }

    eventCount_.fetch_add(1, std::memory_order_relaxed);
    activeWriters_.fetch_sub(1);
    return true;
}

bool TraceBuffer::recordFromIsr(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load()) {
        return false;
    }

    activeWriters_.fetch_add(1);
    if (paused_.load()) {
        activeWriters_.fetch_sub(1);
        return false;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t result =
        xRingbufferSendFromISR(ringBuf_, &event, kEventSize, &higherPriorityTaskWoken);

    if (result != pdTRUE) {
        droppedCount_.fetch_add(1);
        activeWriters_.fetch_sub(1);
        return false;
    }

    eventCount_.fetch_add(1, std::memory_order_relaxed);
    activeWriters_.fetch_sub(1);

    // Yield if a higher priority task was woken
    portYIELD_FROM_ISR(higherPriorityTaskWoken);

    return true;
}

bool TraceBuffer::read(TraceEvent* event, uint32_t timeoutMs) {
    if (!initialized_.load() || event == nullptr) {
        return false;
    }

    const TraceEvent* acquired = acquire(timeoutMs);
    if (acquired == nullptr) {
        return false;
    }

    std::memcpy(event, acquired, kEventSize);
    release(acquired);
    return true;
}

const TraceEvent* TraceBuffer::acquire(uint32_t timeoutMs) {
    if (!initialized_.load()) {
        return nullptr;
    }

    size_t itemSize = 0;
    void* item = xRingbufferReceive(ringBuf_, &itemSize, pdMS_TO_TICKS(timeoutMs));

    if (item == nullptr) {
        return nullptr;
    }

    if (itemSize != kEventSize) {
        ESP_LOGW(kTag, "Unexpected event size: %zu (expected %zu)", itemSize, kEventSize);
        vRingbufferReturnItem(ringBuf_, item);
        return nullptr;
    }

    return static_cast<const TraceEvent*>(item);
}

void TraceBuffer::release(const TraceEvent* event) {
    if (!initialized_.load() || event == nullptr) {
        return;
    }

    vRingbufferReturnItem(ringBuf_, const_cast<TraceEvent*>(event));
    eventCount_.fetch_sub(1, std::memory_order_relaxed);
}

size_t TraceBuffer::count() const {
    return initialized_.load() ? eventCount_.load(std::memory_order_relaxed) : 0;
}

void TraceBuffer::pause() {
    paused_.store(true);
    while (activeWriters_.load() != 0) {
        taskYIELD();
    }
}

void TraceBuffer::clear() {
    if (!initialized_.load()) {
        return;
    }

    const bool wasPaused = paused_.load();
    pause();

    // Drain all items from buffer without relying on the reported free size.
    size_t itemSize = 0;
    void* item;
    while ((item = xRingbufferReceive(ringBuf_, &itemSize, 0)) != nullptr) {
        vRingbufferReturnItem(ringBuf_, item);
    }

    eventCount_.store(0, std::memory_order_relaxed);
    droppedCount_.store(0);
    paused_.store(wasPaused, std::memory_order_release);
    ESP_LOGD(kTag, "Trace buffer cleared");
}

}  // namespace domes::trace
