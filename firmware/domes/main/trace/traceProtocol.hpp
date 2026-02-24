#pragma once

/**
 * @file traceProtocol.hpp
 * @brief Wire protocol definitions for trace commands
 *
 * ALL TYPE DEFINITIONS ARE SOURCED FROM trace.proto via nanopb-generated trace.pb.h.
 * This file provides C++ enum class wrappers for type safety only.
 * DO NOT add new message types or enums here - add them to trace.proto instead.
 *
 * Wire format: All control/metadata messages use protobuf encoding (nanopb).
 * TraceEvent data is carried as raw binary inside protobuf 'bytes' fields.
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
    kUnknown     = domes_trace_MsgType_MSG_TYPE_UNKNOWN,
    kStart       = domes_trace_MsgType_MSG_TYPE_START,
    kStop        = domes_trace_MsgType_MSG_TYPE_STOP,
    kDump        = domes_trace_MsgType_MSG_TYPE_DUMP,
    kData        = domes_trace_MsgType_MSG_TYPE_DATA,
    kEnd         = domes_trace_MsgType_MSG_TYPE_END,
    kClear       = domes_trace_MsgType_MSG_TYPE_CLEAR,
    kStatusReq   = domes_trace_MsgType_MSG_TYPE_STATUS_REQ,
    kStatusResp  = domes_trace_MsgType_MSG_TYPE_STATUS_RESP,
    kStreamCfg   = domes_trace_MsgType_MSG_TYPE_STREAM_CFG,
    kStreamData  = domes_trace_MsgType_MSG_TYPE_STREAM_DATA,
    kSessionInfo = domes_trace_MsgType_MSG_TYPE_SESSION_INFO,
    kAck         = domes_trace_MsgType_MSG_TYPE_ACK,
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
    return (type >= static_cast<uint8_t>(MsgType::kStart) &&
            type <= static_cast<uint8_t>(MsgType::kAck));
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

/// Maximum events per data chunk (16 events * 16 bytes = 256 bytes)
constexpr size_t kEventsPerChunk = 16;

/// Maximum payload size for protobuf-encoded trace data
/// TraceDataChunk: ~8 bytes protobuf overhead + 256 bytes events
constexpr size_t kMaxTraceDataPayload = 280;

/// Maximum frame size for trace messages
constexpr size_t kMaxFrameSize = 512;

/// Maximum protobuf encoding buffer
constexpr size_t kMaxProtobufPayload = 512;

}  // namespace domes::trace
