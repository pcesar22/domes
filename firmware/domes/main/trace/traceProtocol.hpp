#pragma once

/**
 * @file traceProtocol.hpp
 * @brief Wire protocol definitions for trace commands
 *
 * Defines the message types and payload structures for trace operations
 * over the serial transport. Uses the same frame format as OTA protocol
 * (0xAA55 start, length, type, payload, CRC32).
 *
 * Message types are in the 0x10-0x1F range to avoid conflicts with
 * OTA message types (0x01-0x05).
 */

#include "traceEvent.hpp"
#include "traceRecorder.hpp"

#include <cstdint>
#include <cstddef>

namespace domes::trace {

/**
 * @brief Trace protocol message types
 *
 * These extend the frame protocol with trace-specific commands.
 */
enum class TraceMsgType : uint8_t {
    kStart  = 0x10,  ///< Start trace recording (host -> device)
    kStop   = 0x11,  ///< Stop trace recording (host -> device)
    kDump   = 0x12,  ///< Request trace dump (host -> device)
    kData   = 0x13,  ///< Trace data chunk (device -> host)
    kEnd    = 0x14,  ///< End of trace dump (device -> host)
    kClear  = 0x15,  ///< Clear trace buffer (host -> device)
    kStatus = 0x16,  ///< Query trace status (host -> device)
    kAck    = 0x17,  ///< Acknowledge command (device -> host)
};

/**
 * @brief Check if a message type is a trace command
 *
 * @param type Message type byte
 * @return true if this is a trace message
 */
inline bool isTraceMessage(uint8_t type) {
    return type >= static_cast<uint8_t>(TraceMsgType::kStart) &&
           type <= static_cast<uint8_t>(TraceMsgType::kAck);
}

/**
 * @brief Trace status for ACK responses
 */
enum class TraceStatus : uint8_t {
    kOk           = 0x00,  ///< Command succeeded
    kNotInit      = 0x01,  ///< Trace not initialized
    kAlreadyOn    = 0x02,  ///< Tracing already enabled
    kAlreadyOff   = 0x03,  ///< Tracing already disabled
    kBufferEmpty  = 0x04,  ///< No events to dump
    kError        = 0xFF,  ///< Generic error
};

/**
 * @brief Trace metadata sent at start of dump
 *
 * Sent as the first TRACE_DATA message during a dump operation.
 */
#pragma pack(push, 1)
struct TraceMetadata {
    uint32_t eventCount;      ///< Total events in this dump
    uint32_t droppedCount;    ///< Events dropped due to buffer full
    uint32_t startTimestamp;  ///< First event timestamp (microseconds)
    uint32_t endTimestamp;    ///< Last event timestamp (microseconds)
    uint8_t  taskCount;       ///< Number of registered tasks
    // Followed by: TraceTaskEntry[taskCount]
};
static_assert(sizeof(TraceMetadata) == 17, "TraceMetadata size mismatch");
#pragma pack(pop)

/**
 * @brief Task entry in trace metadata
 *
 * Maps task IDs to human-readable names.
 */
#pragma pack(push, 1)
struct TraceTaskEntry {
    uint16_t taskId;                      ///< FreeRTOS task number
    char name[kMaxTaskNameLength];        ///< Task name (null-terminated)
};
static_assert(sizeof(TraceTaskEntry) == 18, "TraceTaskEntry size mismatch");
#pragma pack(pop)

/**
 * @brief Header for trace data chunks
 *
 * Precedes each batch of trace events during dump.
 */
#pragma pack(push, 1)
struct TraceDataHeader {
    uint32_t offset;   ///< Event offset in dump (0-based)
    uint16_t count;    ///< Number of events in this chunk
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
    uint8_t  initialized;  ///< 1 if trace system initialized
    uint8_t  enabled;      ///< 1 if tracing is currently enabled
    uint32_t eventCount;   ///< Approximate events in buffer
    uint32_t droppedCount; ///< Events dropped since last clear
    uint32_t bufferSize;   ///< Total buffer size in bytes
};
static_assert(sizeof(TraceStatusResponse) == 14, "TraceStatusResponse size mismatch");
#pragma pack(pop)

/**
 * @brief ACK response payload
 */
#pragma pack(push, 1)
struct TraceAckResponse {
    uint8_t status;  ///< TraceStatus value
};
static_assert(sizeof(TraceAckResponse) == 1, "TraceAckResponse size mismatch");
#pragma pack(pop)

/// Maximum events per data chunk (8 events * 16 bytes = 128 bytes)
/// Smaller chunks reduce USB buffer overflow issues
constexpr size_t kEventsPerChunk = 8;

/// Maximum payload size for trace data (header + events)
constexpr size_t kMaxTraceDataPayload =
    sizeof(TraceDataHeader) + (kEventsPerChunk * sizeof(TraceEvent));

}  // namespace domes::trace
