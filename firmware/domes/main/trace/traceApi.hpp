#pragma once

/**
 * @file traceApi.hpp
 * @brief User-facing trace API macros
 *
 * Provides convenient macros for adding trace points to application code.
 * All macros are no-ops when tracing is disabled, with minimal overhead.
 *
 * Usage:
 * @code
 * #include "trace/traceApi.hpp"
 *
 * void processGameTick() {
 *     // Automatic scope tracing
 *     TRACE_SCOPE(TRACE_ID("Game.Tick"), Category::kGame);
 *
 *     // Manual begin/end
 *     TRACE_BEGIN(TRACE_ID("Game.Input"), Category::kGame);
 *     processInput();
 *     TRACE_END(TRACE_ID("Game.Input"), Category::kGame);
 *
 *     // Instant event
 *     TRACE_INSTANT(TRACE_ID("Game.Event"), Category::kGame);
 *
 *     // Counter value
 *     TRACE_COUNTER(TRACE_ID("Game.Score"), score, Category::kGame);
 * }
 * @endcode
 */

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "traceEvent.hpp"
#include "traceRecorder.hpp"
#include "traceStrings.hpp"

namespace domes::trace {

/**
 * @brief Get current task ID for trace events
 *
 * Returns the FreeRTOS task number of the current task.
 * Safe to call from any context.
 */
inline uint16_t getCurrentTaskId() {
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    if (handle == nullptr) {
        return 0;
    }
    return static_cast<uint16_t>(uxTaskGetTaskNumber(handle));
}

/**
 * @brief Get current timestamp in microseconds
 */
inline uint32_t getTimestampUs() {
    return static_cast<uint32_t>(esp_timer_get_time());
}

/**
 * @brief Create a trace event with current context
 *
 * @param type Event type
 * @param category Event category
 * @param arg1 Primary argument
 * @param arg2 Secondary argument
 * @return Populated TraceEvent
 */
inline TraceEvent makeEvent(EventType type, Category category, uint32_t arg1 = 0,
                            uint32_t arg2 = 0) {
    TraceEvent event;
    event.timestamp = getTimestampUs();
    event.taskId = getCurrentTaskId();
    event.eventType = static_cast<uint8_t>(type);
    event.flags = static_cast<uint8_t>(category) << 4;
    event.arg1 = arg1;
    event.arg2 = arg2;
    return event;
}

/**
 * @brief RAII scope trace guard
 *
 * Records a span begin event on construction and end event on destruction.
 * Automatically handles early returns and exceptions.
 */
class ScopeTrace {
public:
    /**
     * @brief Construct and record begin event
     *
     * @param spanId Span identifier (use TRACE_ID("name"))
     * @param category Event category
     */
    ScopeTrace(uint32_t spanId, Category category) : spanId_(spanId), category_(category) {
        if (Recorder::isEnabled()) {
            Recorder::record(makeEvent(EventType::kSpanBegin, category_, spanId_));
        }
    }

    /**
     * @brief Destructor records end event
     */
    ~ScopeTrace() {
        if (Recorder::isEnabled()) {
            Recorder::record(makeEvent(EventType::kSpanEnd, category_, spanId_));
        }
    }

    // Non-copyable
    ScopeTrace(const ScopeTrace&) = delete;
    ScopeTrace& operator=(const ScopeTrace&) = delete;

private:
    uint32_t spanId_;
    Category category_;
};

}  // namespace domes::trace

/**
 * @brief Record the beginning of a duration span
 *
 * Must be paired with TRACE_END using the same spanId.
 *
 * @param spanId Span identifier (use TRACE_ID("name"))
 * @param category Category enum value
 */
#define TRACE_BEGIN(spanId, category)                                               \
    do {                                                                            \
        if (::domes::trace::Recorder::isEnabled()) {                                \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(             \
                ::domes::trace::EventType::kSpanBegin, (category), (spanId))); \
        }                                                                           \
    } while (0)

/**
 * @brief Record the end of a duration span
 *
 * Must be paired with TRACE_BEGIN using the same spanId.
 *
 * @param spanId Span identifier (use TRACE_ID("name"))
 * @param category Category enum value
 */
#define TRACE_END(spanId, category)                                               \
    do {                                                                          \
        if (::domes::trace::Recorder::isEnabled()) {                              \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(           \
                ::domes::trace::EventType::kSpanEnd, (category), (spanId))); \
        }                                                                         \
    } while (0)

/**
 * @brief Record an instant (point) event
 *
 * Use for marking specific moments in time without duration.
 *
 * @param eventId Event identifier (use TRACE_ID("name"))
 * @param category Category enum value
 */
#define TRACE_INSTANT(eventId, category)                                           \
    do {                                                                           \
        if (::domes::trace::Recorder::isEnabled()) {                               \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(            \
                ::domes::trace::EventType::kInstant, (category), (eventId))); \
        }                                                                          \
    } while (0)

/**
 * @brief Record a counter value
 *
 * Use for tracking numeric values over time (e.g., queue depth, memory usage).
 *
 * @param counterId Counter identifier (use TRACE_ID("name"))
 * @param value Current counter value
 * @param category Category enum value
 */
#define TRACE_COUNTER(counterId, value, category)                                               \
    do {                                                                                        \
        if (::domes::trace::Recorder::isEnabled()) {                                            \
            ::domes::trace::Recorder::record(                                                   \
                ::domes::trace::makeEvent(::domes::trace::EventType::kCounter, (category), \
                                          (counterId), static_cast<uint32_t>(value)));          \
        }                                                                                       \
    } while (0)

/**
 * @brief Automatic scope tracing
 *
 * Records begin on entry, end on scope exit (including early returns).
 *
 * @param spanId Span identifier (use TRACE_ID("name"))
 * @param category Category enum value
 */
#define TRACE_SCOPE(spanId, category) \
    ::domes::trace::ScopeTrace _scopeTrace_##__LINE__((spanId), (category))

/**
 * @brief Record a mutex lock acquisition
 *
 * Use TRACE_ID("Name.Mutex") for mutexId.
 *
 * @param mutexId Mutex identifier (use TRACE_ID("name"))
 */
#define TRACE_MUTEX_LOCK(mutexId)                                                                \
    do {                                                                                         \
        if (::domes::trace::Recorder::isEnabled()) {                                             \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(                          \
                ::domes::trace::EventType::kMutexLock, ::domes::trace::Category::kSync,          \
                (mutexId)));                                                                     \
        }                                                                                        \
    } while (0)

/**
 * @brief Record a mutex unlock
 *
 * @param mutexId Mutex identifier (use TRACE_ID("name"))
 */
#define TRACE_MUTEX_UNLOCK(mutexId)                                                              \
    do {                                                                                         \
        if (::domes::trace::Recorder::isEnabled()) {                                             \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(                          \
                ::domes::trace::EventType::kMutexUnlock, ::domes::trace::Category::kSync,        \
                (mutexId)));                                                                     \
        }                                                                                        \
    } while (0)

/**
 * @brief Record mutex contention (waited to acquire)
 *
 * @param mutexId Mutex identifier (use TRACE_ID("name"))
 * @param waitTimeUs Time spent waiting in microseconds
 */
#define TRACE_MUTEX_CONTENTION(mutexId, waitTimeUs)                                              \
    do {                                                                                         \
        if (::domes::trace::Recorder::isEnabled()) {                                             \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(                          \
                ::domes::trace::EventType::kMutexContention, ::domes::trace::Category::kSync,    \
                (mutexId), static_cast<uint32_t>(waitTimeUs)));                                  \
        }                                                                                        \
    } while (0)

/**
 * @brief Record a semaphore take
 *
 * @param semId Semaphore identifier (use TRACE_ID("name"))
 */
#define TRACE_SEM_TAKE(semId)                                                                    \
    do {                                                                                         \
        if (::domes::trace::Recorder::isEnabled()) {                                             \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(                          \
                ::domes::trace::EventType::kSemTake, ::domes::trace::Category::kSync,            \
                (semId)));                                                                       \
        }                                                                                        \
    } while (0)

/**
 * @brief Record a semaphore give
 *
 * @param semId Semaphore identifier (use TRACE_ID("name"))
 */
#define TRACE_SEM_GIVE(semId)                                                                    \
    do {                                                                                         \
        if (::domes::trace::Recorder::isEnabled()) {                                             \
            ::domes::trace::Recorder::record(::domes::trace::makeEvent(                          \
                ::domes::trace::EventType::kSemGive, ::domes::trace::Category::kSync,            \
                (semId)));                                                                       \
        }                                                                                        \
    } while (0)
