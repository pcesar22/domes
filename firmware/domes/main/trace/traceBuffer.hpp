#pragma once

/**
 * @file traceBuffer.hpp
 * @brief Ring buffer for storing trace events
 *
 * Provides a thread-safe, ISR-safe ring buffer for trace event storage.
 * Uses FreeRTOS ring buffer primitives with static allocation.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "traceEvent.hpp"

#include <atomic>
#include <cstddef>

namespace domes::trace {

/**
 * @brief Ring buffer for trace event storage
 *
 * Stores TraceEvent structures in a circular buffer. When full,
 * new events are silently dropped (no blocking). The buffer can
 * be paused during dump operations to ensure consistency.
 *
 * @note This class uses static allocation. Only one instance should
 *       be created, typically as a static member of TraceRecorder.
 */
class TraceBuffer {
public:
    /// Default buffer size: 32KB = 2048 events at 16 bytes each
    static constexpr size_t kDefaultBufferSize = 32 * 1024;

    /// Size of each trace event
    static constexpr size_t kEventSize = sizeof(TraceEvent);

    /// Maximum events that can be stored (approximate, due to ring buffer overhead)
    static constexpr size_t kMaxEvents = kDefaultBufferSize / kEventSize;

    /**
     * @brief Construct trace buffer
     *
     * @param bufferSize Size of the ring buffer in bytes (default 32KB)
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
     * @brief Get approximate number of events in buffer
     *
     * @return Estimated event count (may be slightly inaccurate)
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
    void pause() { paused_.store(true); }

    /**
     * @brief Resume recording after pause
     */
    void resume() { paused_.store(false); }

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
};

}  // namespace domes::trace
