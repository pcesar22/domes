#pragma once

/**
 * @file traceProtocol.hpp
 * @brief Wire protocol definitions for trace commands
 *
 * ALL TYPE DEFINITIONS ARE SOURCED FROM trace.proto via nanopb-generated trace.pb.h.
 * This file provides C++ enum class wrappers for type safety only.
 * DO NOT add new message types or enums here - add them to trace.proto instead.
 *
 * Note: Binary struct formats (TraceMetadata, etc.) are kept here for efficient
 * wire encoding. The proto messages are used for type generation only.
 */

#include "traceEvent.hpp"
#include "traceRecorder.hpp"

#include <cstddef>
#include <cstdint>

// Include the nanopb-generated protobuf definitions (source of truth)
#include "trace.pb.h"

namespace domes::trace {

/**
 * @brief Trace protocol message types (sourced from trace.proto)
 */
enum class MsgType : uint8_t {
    kUnknown = domes_trace_MsgType_MSG_TYPE_UNKNOWN,
    kStart   = domes_trace_MsgType_MSG_TYPE_START,
    kStop    = domes_trace_MsgType_MSG_TYPE_STOP,
    kDump    = domes_trace_MsgType_MSG_TYPE_DUMP,
    kData    = domes_trace_MsgType_MSG_TYPE_DATA,
    kEnd     = domes_trace_MsgType_MSG_TYPE_END,
    kClear   = domes_trace_MsgType_MSG_TYPE_CLEAR,
    kStatus  = domes_trace_MsgType_MSG_TYPE_STATUS,
    kAck     = domes_trace_MsgType_MSG_TYPE_ACK,
};

/**
 * @brief Trace status codes (sourced from trace.proto)
 */
enum class Status : uint8_t {
    kOk          = domes_trace_Status_STATUS_OK,
    kNotInit     = domes_trace_Status_STATUS_NOT_INIT,
    kAlreadyOn   = domes_trace_Status_STATUS_ALREADY_ON,
    kAlreadyOff  = domes_trace_Status_STATUS_ALREADY_OFF,
    kBufferEmpty = domes_trace_Status_STATUS_BUFFER_EMPTY,
    kError       = domes_trace_Status_STATUS_ERROR,
};

/**
 * @brief Check if a message type is a trace command (0x10-0x1F range)
 */
inline bool isTraceMessage(uint8_t type) {
    return type >= static_cast<uint8_t>(MsgType::kStart) &&
           type <= static_cast<uint8_t>(MsgType::kAck);
}

/**
 * @brief Get human-readable name for a trace status
 */
inline const char* statusToString(Status status) {
    switch (status) {
        case Status::kOk:          return "ok";
        case Status::kNotInit:     return "not-initialized";
        case Status::kAlreadyOn:   return "already-on";
        case Status::kAlreadyOff:  return "already-off";
        case Status::kBufferEmpty: return "buffer-empty";
        case Status::kError:       return "error";
        default:                   return "unknown";
    }
}

// ============================================================================
// Binary Wire Format Structures
// These are NOT protobuf messages - they are packed binary for efficient transfer.
// ============================================================================

/**
 * @brief Trace metadata sent at start of dump
 */
#pragma pack(push, 1)
struct TraceMetadata {
    uint32_t eventCount;      ///< Total events in this dump
    uint32_t droppedCount;    ///< Events dropped due to buffer full
    uint32_t startTimestamp;  ///< First event timestamp (microseconds)
    uint32_t endTimestamp;    ///< Last event timestamp (microseconds)
    uint8_t taskCount;        ///< Number of registered tasks
    // Followed by: TraceTaskEntry[taskCount]
};
static_assert(sizeof(TraceMetadata) == 17, "TraceMetadata size mismatch");
#pragma pack(pop)

/**
 * @brief Task entry in trace metadata
 */
#pragma pack(push, 1)
struct TraceTaskEntry {
    uint16_t taskId;                ///< FreeRTOS task number
    char name[kMaxTaskNameLength];  ///< Task name (null-terminated)
};
static_assert(sizeof(TraceTaskEntry) == 18, "TraceTaskEntry size mismatch");
#pragma pack(pop)

/**
 * @brief Header for trace data chunks
 */
#pragma pack(push, 1)
struct TraceDataHeader {
    uint32_t offset;  ///< Event offset in dump (0-based)
    uint16_t count;   ///< Number of events in this chunk
    // Followed by: TraceEvent[count]
};
static_assert(sizeof(TraceDataHeader) == 6, "TraceDataHeader size mismatch");
#pragma pack(pop)

/**
 * @brief End of dump message payload
 */
#pragma pack(push, 1)
struct TraceDumpEnd {
    uint32_t totalEvents;  ///< Total events sent
    uint32_t checksum;     ///< Simple checksum of all event data
};
static_assert(sizeof(TraceDumpEnd) == 8, "TraceDumpEnd size mismatch");
#pragma pack(pop)

/**
 * @brief Status response payload
 */
#pragma pack(push, 1)
struct TraceStatusResponse {
    uint8_t initialized;    ///< 1 if trace system initialized
    uint8_t enabled;        ///< 1 if tracing is currently enabled
    uint32_t eventCount;    ///< Approximate events in buffer
    uint32_t droppedCount;  ///< Events dropped since last clear
    uint32_t bufferSize;    ///< Total buffer size in bytes
};
static_assert(sizeof(TraceStatusResponse) == 14, "TraceStatusResponse size mismatch");
#pragma pack(pop)

/**
 * @brief ACK response payload
 */
#pragma pack(push, 1)
struct TraceAckResponse {
    uint8_t status;  ///< Status value
};
static_assert(sizeof(TraceAckResponse) == 1, "TraceAckResponse size mismatch");
#pragma pack(pop)

/// Maximum events per data chunk (8 events * 16 bytes = 128 bytes)
constexpr size_t kEventsPerChunk = 8;

/// Maximum payload size for trace data (header + events)
constexpr size_t kMaxTraceDataPayload =
    sizeof(TraceDataHeader) + (kEventsPerChunk * sizeof(TraceEvent));

/// Maximum frame size for trace messages
constexpr size_t kMaxFrameSize = 256;

}  // namespace domes::trace
