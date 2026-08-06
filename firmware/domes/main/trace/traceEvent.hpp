#pragma once

/**
 * @file traceEvent.hpp
 * @brief Trace event structures and enums for performance profiling
 *
 * ALL TYPE DEFINITIONS ARE SOURCED FROM trace.proto via nanopb-generated trace.pb.h.
 * This file provides C++ enum class wrappers for type safety only.
 * DO NOT add new event types or categories here - add them to trace.proto instead.
 */

#include <cstdint>

// Include the nanopb-generated protobuf definitions (source of truth)
#include "trace.pb.h"

namespace domes::trace {

/**
 * @brief Categories for trace events (sourced from trace.proto)
 */
enum class Category : uint8_t {
    kKernel = domes_trace_Category_CATEGORY_KERNEL,
    kTransport = domes_trace_Category_CATEGORY_TRANSPORT,
    kOta = domes_trace_Category_CATEGORY_OTA,
    kWifi = domes_trace_Category_CATEGORY_WIFI,
    kLed = domes_trace_Category_CATEGORY_LED,
    kAudio = domes_trace_Category_CATEGORY_AUDIO,
    kTouch = domes_trace_Category_CATEGORY_TOUCH,
    kGame = domes_trace_Category_CATEGORY_GAME,
    kUser = domes_trace_Category_CATEGORY_USER,
    kHaptic = domes_trace_Category_CATEGORY_HAPTIC,
    kBle = domes_trace_Category_CATEGORY_BLE,
    kNvs = domes_trace_Category_CATEGORY_NVS,
    kEspNow = domes_trace_Category_CATEGORY_ESPNOW,
    kSync = domes_trace_Category_CATEGORY_SYNC,
};

/**
 * @brief Types of trace events (sourced from trace.proto)
 */
enum class EventType : uint8_t {
    kUnknown = domes_trace_EventType_EVENT_TYPE_UNKNOWN,
    kMutexLock = domes_trace_EventType_EVENT_TYPE_MUTEX_LOCK,
    kMutexUnlock = domes_trace_EventType_EVENT_TYPE_MUTEX_UNLOCK,
    kMutexContention = domes_trace_EventType_EVENT_TYPE_MUTEX_CONTENTION,
    kSemTake = domes_trace_EventType_EVENT_TYPE_SEM_TAKE,
    kSemGive = domes_trace_EventType_EVENT_TYPE_SEM_GIVE,
    kSchedTaskCreate = domes_trace_EventType_EVENT_TYPE_SCHED_TASK_CREATE,
    kSchedTaskDelete = domes_trace_EventType_EVENT_TYPE_SCHED_TASK_DELETE,
    kSchedTaskReady = domes_trace_EventType_EVENT_TYPE_SCHED_TASK_READY,
    kSchedTaskBlock = domes_trace_EventType_EVENT_TYPE_SCHED_TASK_BLOCK,
    kSchedSwitchIn = domes_trace_EventType_EVENT_TYPE_SCHED_SWITCH_IN,
    kSchedSwitchOut = domes_trace_EventType_EVENT_TYPE_SCHED_SWITCH_OUT,
    kSchedIsrEnter = domes_trace_EventType_EVENT_TYPE_SCHED_ISR_ENTER,
    kSchedIsrExit = domes_trace_EventType_EVENT_TYPE_SCHED_ISR_EXIT,
    kSchedQueueSend = domes_trace_EventType_EVENT_TYPE_SCHED_QUEUE_SEND,
    kSchedQueueReceive = domes_trace_EventType_EVENT_TYPE_SCHED_QUEUE_RECEIVE,
    kSchedTimeout = domes_trace_EventType_EVENT_TYPE_SCHED_TIMEOUT,
    kCallbackBegin = domes_trace_EventType_EVENT_TYPE_CALLBACK_BEGIN,
    kCallbackEnd = domes_trace_EventType_EVENT_TYPE_CALLBACK_END,
    kCausalComplete = domes_trace_EventType_EVENT_TYPE_CAUSAL_COMPLETE,

    // Application events (0x20-0x2F)
    kSpanBegin = domes_trace_EventType_EVENT_TYPE_SPAN_BEGIN,
    kSpanEnd = domes_trace_EventType_EVENT_TYPE_SPAN_END,
    kInstant = domes_trace_EventType_EVENT_TYPE_INSTANT,
    kCounter = domes_trace_EventType_EVENT_TYPE_COUNTER,
    kComplete = domes_trace_EventType_EVENT_TYPE_COMPLETE,
    kTraceOverhead = domes_trace_EventType_EVENT_TYPE_TRACE_OVERHEAD,
};

/**
 * @brief Compact trace event structure (16 bytes)
 *
 * Multi-byte fields are serialized little-endian. Designed for efficient storage:
 * - Fixed size enables simple buffer arithmetic
 * - No pointers or strings (uses IDs that map to names on host)
 * - Packed to minimize memory footprint
 */
#pragma pack(push, 1)
struct TraceEvent {
    uint32_t timestamp;  ///< Microseconds since boot (esp_timer_get_time())
    uint16_t taskId;     ///< Immutable task ID assigned by Recorder::registerTask()
    uint8_t eventType;   ///< EventType value
    uint8_t flags;       ///< Category bits 7-4; context bits 3-2; core bits 1-0
    uint32_t arg1;       ///< Primary argument (span ID, counter ID, ISR number)
    uint32_t arg2;       ///< Secondary argument (counter value, duration)

    /// Extract category from flags
    Category category() const { return static_cast<Category>((flags >> 4) & 0x0F); }

    /// Set category in flags
    void setCategory(Category cat) { flags = (flags & 0x0F) | (static_cast<uint8_t>(cat) << 4); }

    /// Get event type as enum
    EventType type() const { return static_cast<EventType>(eventType); }

    /// Format-v1 core code: 0 unknown, 1 Core 0, 2 Core 1
    uint8_t coreCode() const { return flags & 0x03; }

    /// Format-v1 context code: 0 task, 1 ISR, 2 callback
    uint8_t contextCode() const { return (flags >> 2) & 0x03; }
};
#pragma pack(pop)

static_assert(sizeof(TraceEvent) == 16, "TraceEvent must be exactly 16 bytes");

/**
 * @brief Category names for trace output (sourced from trace.proto order)
 */
constexpr const char* kCategoryNames[] = {
    "kernel",     // 0
    "transport",  // 1
    "ota",        // 2
    "wifi",       // 3
    "led",        // 4
    "audio",      // 5
    "touch",      // 6
    "game",       // 7
    "user",       // 8
    "haptic",     // 9
    "ble",        // 10
    "nvs",        // 11
    "espnow",     // 12
    "sync",       // 13
};

}  // namespace domes::trace
