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
    kKernel    = domes_trace_Category_CATEGORY_KERNEL,
    kTransport = domes_trace_Category_CATEGORY_TRANSPORT,
    kOta       = domes_trace_Category_CATEGORY_OTA,
    kWifi      = domes_trace_Category_CATEGORY_WIFI,
    kLed       = domes_trace_Category_CATEGORY_LED,
    kAudio     = domes_trace_Category_CATEGORY_AUDIO,
    kTouch     = domes_trace_Category_CATEGORY_TOUCH,
    kGame      = domes_trace_Category_CATEGORY_GAME,
    kUser      = domes_trace_Category_CATEGORY_USER,
    kHaptic    = domes_trace_Category_CATEGORY_HAPTIC,
    kBle       = domes_trace_Category_CATEGORY_BLE,
    kNvs       = domes_trace_Category_CATEGORY_NVS,
};

/**
 * @brief Types of trace events (sourced from trace.proto)
 */
enum class EventType : uint8_t {
    // FreeRTOS kernel events (0x01-0x0F)
    kUnknown       = domes_trace_EventType_EVENT_TYPE_UNKNOWN,
    kTaskSwitchIn  = domes_trace_EventType_EVENT_TYPE_TASK_SWITCH_IN,
    kTaskSwitchOut = domes_trace_EventType_EVENT_TYPE_TASK_SWITCH_OUT,
    kTaskCreate    = domes_trace_EventType_EVENT_TYPE_TASK_CREATE,
    kTaskDelete    = domes_trace_EventType_EVENT_TYPE_TASK_DELETE,
    kIsrEnter      = domes_trace_EventType_EVENT_TYPE_ISR_ENTER,
    kIsrExit       = domes_trace_EventType_EVENT_TYPE_ISR_EXIT,
    kQueueSend     = domes_trace_EventType_EVENT_TYPE_QUEUE_SEND,
    kQueueReceive  = domes_trace_EventType_EVENT_TYPE_QUEUE_RECEIVE,
    kMutexLock     = domes_trace_EventType_EVENT_TYPE_MUTEX_LOCK,
    kMutexUnlock   = domes_trace_EventType_EVENT_TYPE_MUTEX_UNLOCK,

    // Application events (0x20-0x2F)
    kSpanBegin = domes_trace_EventType_EVENT_TYPE_SPAN_BEGIN,
    kSpanEnd   = domes_trace_EventType_EVENT_TYPE_SPAN_END,
    kInstant   = domes_trace_EventType_EVENT_TYPE_INSTANT,
    kCounter   = domes_trace_EventType_EVENT_TYPE_COUNTER,
    kComplete  = domes_trace_EventType_EVENT_TYPE_COMPLETE,
};

/**
 * @brief Compact trace event structure (16 bytes)
 *
 * Designed for efficient storage in a ring buffer:
 * - Fixed size enables simple buffer arithmetic
 * - No pointers or strings (uses IDs that map to names on host)
 * - Packed to minimize memory footprint
 */
#pragma pack(push, 1)
struct TraceEvent {
    uint32_t timestamp;  ///< Microseconds since boot (esp_timer_get_time())
    uint16_t taskId;     ///< FreeRTOS task number (uxTaskGetTaskNumber())
    uint8_t eventType;   ///< EventType value
    uint8_t flags;       ///< Category in bits 7-4, reserved in bits 3-0
    uint32_t arg1;       ///< Primary argument (span ID, counter ID, ISR number)
    uint32_t arg2;       ///< Secondary argument (counter value, duration)

    /// Extract category from flags
    Category category() const { return static_cast<Category>((flags >> 4) & 0x0F); }

    /// Set category in flags
    void setCategory(Category cat) {
        flags = (flags & 0x0F) | (static_cast<uint8_t>(cat) << 4);
    }

    /// Get event type as enum
    EventType type() const { return static_cast<EventType>(eventType); }
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
};

}  // namespace domes::trace
