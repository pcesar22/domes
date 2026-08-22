#pragma once

/**
 * @file traceBuffer.hpp
 * @brief Ring buffer for storing trace events
 *
 * Provides a thread-safe, ISR-safe ring buffer for trace event storage.
 * Uses a FreeRTOS ring buffer with bounded capacity allocated during initialization.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "traceEvent.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace domes::trace {

/**
 * @brief Ring buffer for trace event storage
 *
 * Stores TraceEvent structures in a circular buffer. When full,
 * new events are silently dropped (no blocking). The buffer can
 * be paused during dump operations to ensure consistency.
 *
 * @note Backing storage is allocated once during init(). Only one instance should be created,
 *       typically under TraceRecorder ownership.
 */
class TraceBuffer {
public:
    /// Default requested capacity: 48 KiB.
    ///
    /// FreeRTOS's no-split ring stores metadata beside every 16-byte event, so
    /// a 32 KiB allocation retains only about 1,000 events. A bounded physical
    /// ping/pong correlation capture produces roughly 1,200 events while the
    /// scheduler hooks are enabled. Keep enough headroom to retain that proof
    /// without weakening the recorder's fail-closed overflow semantics.
    static constexpr size_t kDefaultBufferSize = 48 * 1024;

    /// Size of each trace event
    static constexpr size_t kEventSize = sizeof(TraceEvent);

    /// Maximum events that can be stored (approximate, due to ring buffer overhead)
    static constexpr size_t kMaxEvents = kDefaultBufferSize / kEventSize;

    /**
     * @brief Construct trace buffer
     *
     * @param bufferSize Requested ring buffer capacity in bytes (default 48 KiB)
     */
    explicit TraceBuffer(size_t bufferSize = kDefaultBufferSize);

    /// Destructor - releases ring buffer resources
    ~TraceBuffer();

    // Non-copyable
    TraceBuffer(const TraceBuffer&) = delete;
    TraceBuffer& operator=(const TraceBuffer&) = delete;

    /**
     * @brief Initialize the ring buffer
     *
     * Must be called once before recording events.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t init();

    /**
     * @brief Record an event to the buffer (task context)
     *
     * Thread-safe. If the buffer is full or paused, the event is dropped.
     *
     * @param event Event to record
     * @return true if event was recorded, false if dropped
     */
    bool record(const TraceEvent& event);

    /**
     * @brief Record an event from ISR context
     *
     * Non-blocking. If the buffer is full or paused, the event is dropped.
     *
     * @param event Event to record
     * @return true if event was recorded, false if dropped
     */
    bool recordFromIsr(const TraceEvent& event);

    /**
     * @brief Read and remove next event from buffer
     *
     * Used during dump operations. Blocks until an event is available
     * or timeout expires.
     *
     * @param event [out] Event to populate
     * @param timeoutMs Maximum time to wait (0 for non-blocking)
     * @return true if event was read, false if buffer empty or timeout
     */
    bool read(TraceEvent* event, uint32_t timeoutMs = 0);

    /**
     * @brief Acquire the next event without removing it from buffer storage
     *
     * Acquired events must be released in acquisition order. This lets trace
     * dumps keep their source events intact until every response frame has
     * been delivered successfully.
     *
     * @param timeoutMs Maximum time to wait (0 for non-blocking)
     * @return Pointer to an event owned by the buffer, or nullptr
     */
    const TraceEvent* acquire(uint32_t timeoutMs = 0);

    /**
     * @brief Release a previously acquired event and remove it from the buffer
     *
     * @param event Pointer returned by acquire()
     */
    void release(const TraceEvent* event);

    /** Claim the single retained dump snapshot for a transport handler. */
    bool tryClaimDumpSnapshot(const void* owner);

    /** Acquire all currently queued events into the claimed dump snapshot. */
    size_t captureDumpSnapshot(const void* owner);

    /** Return the number of retained events for the owning handler. */
    size_t dumpSnapshotCount(const void* owner) const;

    /** Return one retained event for the owning handler. */
    const TraceEvent* dumpSnapshotEvent(const void* owner, size_t index) const;

    /** Release a successfully delivered snapshot and its ownership claim. */
    bool completeDumpSnapshot(const void* owner);

    /** Clear a retained snapshot and all subsequently recorded events. */
    bool clearDumpSnapshot(const void* owner);

    /**
     * @brief Get the number of events currently in the buffer
     */
    size_t count() const;

    /**
     * @brief Clear all events from buffer
     */
    void clear();

    /**
     * @brief Check if buffer is initialized
     */
    bool isInitialized() const { return initialized_.load(); }

    /**
     * @brief Pause recording (for consistent dump)
     *
     * While paused, new events are silently dropped.
     */
    void pause();

    /**
     * @brief Resume recording after pause
     */
    void resume() { paused_.store(false, std::memory_order_release); }

    /**
     * @brief Check if recording is paused
     */
    bool isPaused() const { return paused_.load(); }

    /**
     * @brief Get count of dropped events (due to buffer full)
     */
    uint32_t droppedCount() const { return droppedCount_.load(); }

    /**
     * @brief Reset dropped event counter
     */
    void resetDroppedCount() { droppedCount_.store(0); }

private:
    RingbufHandle_t ringBuf_;
    size_t bufferSize_;
    std::atomic<bool> initialized_;
    std::atomic<bool> paused_;
    std::atomic<uint32_t> droppedCount_;
    std::atomic<size_t> eventCount_;
    std::atomic<uint32_t> activeWriters_{0};
    std::array<const TraceEvent*, kMaxEvents> dumpSnapshot_{};
    size_t dumpSnapshotCount_ = 0;
    std::atomic<uintptr_t> dumpOwner_{0};

    void releaseDumpSnapshotEvents(const void* owner);
};

}  // namespace domes::trace
