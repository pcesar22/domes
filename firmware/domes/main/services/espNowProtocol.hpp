#pragma once

/**
 * @file espNowProtocol.hpp
 * @brief ESP-NOW message definitions for discovery and game protocol
 *
 * Packed binary structs for all ESP-NOW messages. Fits within 250-byte payload.
 * Matches simulation protocol (simProtocol.hpp) message types.
 *
 * Message layout: [MsgHeader (11 bytes)][type-specific payload]
 */

#include "esp_now.h"
#include <cstdint>

namespace domes::espnow {

/// Message types for unified ESP-NOW protocol
enum class MsgType : uint8_t {
    // Discovery (layer 1)
    kBeacon       = 0x01,
    kPing         = 0x02,
    kPong         = 0x03,

    // Game control (master -> slave)
    kJoinGame     = 0x10,
    kArmTouch     = 0x11,
    kSetColor     = 0x12,
    kStopAll      = 0x13,

    // Game events (slave -> master)
    kTouchEvent   = 0x20,
    kTimeoutEvent = 0x21,
};

/// Get human-readable name for a message type
inline const char* msgTypeName(MsgType type) {
    switch (type) {
        case MsgType::kBeacon:       return "BEACON";
        case MsgType::kPing:         return "PING";
        case MsgType::kPong:         return "PONG";
        case MsgType::kJoinGame:     return "JOIN_GAME";
        case MsgType::kArmTouch:     return "ARM_TOUCH";
        case MsgType::kSetColor:     return "SET_COLOR";
        case MsgType::kStopAll:      return "STOP_ALL";
        case MsgType::kTouchEvent:   return "TOUCH_EVENT";
        case MsgType::kTimeoutEvent: return "TIMEOUT_EVENT";
        default:                     return "UNKNOWN";
    }
}

#pragma pack(push, 1)

/// Common header for all ESP-NOW messages (11 bytes)
struct MsgHeader {
    uint8_t type;                          ///< MsgType
    uint8_t senderMac[ESP_NOW_ETH_ALEN];   ///< Sender's WiFi STA MAC
    uint32_t timestampUs;                  ///< esp_timer_get_time() truncated to 32-bit
};

static_assert(sizeof(MsgHeader) == 11, "MsgHeader must be 11 bytes");

/// ARM command: master -> slave (16 bytes)
struct ArmTouchMsg {
    MsgHeader header;
    uint32_t timeoutMs;       ///< Timeout before miss
    uint8_t feedbackMode;     ///< Bitmask: 0x01=LED, 0x02=audio
};

static_assert(sizeof(ArmTouchMsg) == 16, "ArmTouchMsg must be 16 bytes");

/// SET_COLOR command: master -> slave (14 bytes)
struct SetColorMsg {
    MsgHeader header;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static_assert(sizeof(SetColorMsg) == 14, "SetColorMsg must be 14 bytes");

/// TOUCH_EVENT: slave -> master (16 bytes)
struct TouchEventMsg {
    MsgHeader header;
    uint32_t reactionTimeUs;   ///< Microseconds from arm to touch
    uint8_t padIndex;          ///< Which pad was touched
};

static_assert(sizeof(TouchEventMsg) == 16, "TouchEventMsg must be 16 bytes");

/// TIMEOUT_EVENT: slave -> master (header only, 11 bytes)
struct TimeoutEventMsg {
    MsgHeader header;
};

/// JOIN_GAME: master -> broadcast (header only, 11 bytes)
struct JoinGameMsg {
    MsgHeader header;
};

/// STOP_ALL: master -> broadcast (header only, 11 bytes)
struct StopAllMsg {
    MsgHeader header;
};

#pragma pack(pop)

}  // namespace domes::espnow
