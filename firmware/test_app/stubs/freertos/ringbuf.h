#pragma once

#include "freertos/FreeRTOS.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

typedef enum {
    RINGBUF_TYPE_NOSPLIT = 0,
    RINGBUF_TYPE_ALLOWSPLIT,
    RINGBUF_TYPE_BYTEBUF,
    RINGBUF_TYPE_MAX,
} RingbufferType_t;

struct HostRingBuffer {
    explicit HostRingBuffer(size_t capacity) : capacity(capacity) {}
    size_t capacity;
    size_t used = 0;
    std::deque<std::vector<uint8_t>> queued;
    std::unordered_map<void*, size_t> acquired;
    std::mutex mutex;
};

using RingbufHandle_t = HostRingBuffer*;

inline size_t hostRingStorage(size_t size) {
    return 8 + ((size + 3U) & ~size_t{3U});
}

inline RingbufHandle_t xRingbufferCreate(size_t size, RingbufferType_t) {
    return new HostRingBuffer(size);
}

inline void vRingbufferDelete(RingbufHandle_t ring) {
    delete ring;
}

inline BaseType_t xRingbufferSend(RingbufHandle_t ring, const void* data, size_t size, TickType_t) {
    std::lock_guard lock(ring->mutex);
    const size_t storage = hostRingStorage(size);
    if (ring->used + storage > ring->capacity) {
        return pdFALSE;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    ring->queued.emplace_back(bytes, bytes + size);
    ring->used += storage;
    return pdTRUE;
}

inline BaseType_t xRingbufferSendFromISR(RingbufHandle_t ring, const void* data, size_t size,
                                         BaseType_t*) {
    return xRingbufferSend(ring, data, size, 0);
}

inline void* xRingbufferReceive(RingbufHandle_t ring, size_t* size, TickType_t) {
    std::lock_guard lock(ring->mutex);
    if (ring->queued.empty()) {
        return nullptr;
    }
    std::vector<uint8_t> item = std::move(ring->queued.front());
    ring->queued.pop_front();
    auto* result = new uint8_t[item.size()];
    std::memcpy(result, item.data(), item.size());
    *size = item.size();
    ring->acquired[result] = hostRingStorage(item.size());
    return result;
}

inline void vRingbufferReturnItem(RingbufHandle_t ring, void* item) {
    std::lock_guard lock(ring->mutex);
    const auto found = ring->acquired.find(item);
    if (found != ring->acquired.end()) {
        ring->used -= found->second;
        ring->acquired.erase(found);
    }
    delete[] static_cast<uint8_t*>(item);
}

inline size_t xRingbufferGetCurFreeSize(RingbufHandle_t ring) {
    std::lock_guard lock(ring->mutex);
    return ring->capacity - ring->used;
}
