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

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace domes::espnow {

/// Message types for unified ESP-NOW protocol
enum class MsgType : uint8_t {
    // Discovery (layer 1)
    kBeacon = 0x01,
    kPing = 0x02,
    kPong = 0x03,

    // Game control (master -> slave)
    kJoinGame = 0x10,
    kArmTouch = 0x11,
    kSetColor = 0x12,
    kStopAll = 0x13,

    // Sim touch injection (master -> slave)
    kSimulateTouch = 0x14,

    // Game events (slave -> master)
    kTouchEvent = 0x20,
    kTimeoutEvent = 0x21,
};

/// Get human-readable name for a message type
inline const char* msgTypeName(MsgType type) {
    switch (type) {
        case MsgType::kBeacon:
            return "BEACON";
        case MsgType::kPing:
            return "PING";
        case MsgType::kPong:
            return "PONG";
        case MsgType::kJoinGame:
            return "JOIN_GAME";
        case MsgType::kArmTouch:
            return "ARM_TOUCH";
        case MsgType::kSetColor:
            return "SET_COLOR";
        case MsgType::kStopAll:
            return "STOP_ALL";
        case MsgType::kSimulateTouch:
            return "SIMULATE_TOUCH";
        case MsgType::kTouchEvent:
            return "TOUCH_EVENT";
        case MsgType::kTimeoutEvent:
            return "TIMEOUT_EVENT";
        default:
            return "UNKNOWN";
    }
}

/// Discovery traffic may select or register a peer. All other traffic is peer-scoped.
constexpr bool isDiscoveryMessage(MsgType type) {
    return type == MsgType::kBeacon || type == MsgType::kPing || type == MsgType::kPong;
}

/// A zero token is never active, and only an exact echo may complete a round.
constexpr bool matchesActiveRound(uint32_t activeToken, uint32_t receivedToken) {
    return activeToken != 0 && receivedToken == activeToken;
}

#pragma pack(push, 1)

/// Common header for all ESP-NOW messages (11 bytes)
struct MsgHeader {
    uint8_t type;                         ///< MsgType
    uint8_t senderMac[ESP_NOW_ETH_ALEN];  ///< Sender's WiFi STA MAC
    uint32_t timestampUs;                 ///< esp_timer_get_time() truncated to 32-bit
};

static_assert(sizeof(MsgHeader) == 11, "MsgHeader must be 11 bytes");

/// ARM command: master -> slave (20 bytes)
struct ArmTouchMsg {
    MsgHeader header;
    uint32_t roundToken;   ///< Non-zero token echoed by the resulting event
    uint32_t timeoutMs;    ///< Timeout before miss
    uint8_t feedbackMode;  ///< Bitmask: 0x01=LED, 0x02=audio
};

static_assert(sizeof(ArmTouchMsg) == 20, "ArmTouchMsg must be 20 bytes");

/// SET_COLOR command: master -> slave (14 bytes)
struct SetColorMsg {
    MsgHeader header;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static_assert(sizeof(SetColorMsg) == 14, "SetColorMsg must be 14 bytes");

/// TOUCH_EVENT: slave -> master (20 bytes)
struct TouchEventMsg {
    MsgHeader header;
    uint32_t roundToken;      ///< Token from the corresponding ARM command
    uint32_t reactionTimeUs;  ///< Microseconds from arm to touch
    uint8_t padIndex;         ///< Which pad was touched
};

static_assert(sizeof(TouchEventMsg) == 20, "TouchEventMsg must be 20 bytes");

/// TIMEOUT_EVENT: slave -> master (15 bytes)
struct TimeoutEventMsg {
    MsgHeader header;
    uint32_t roundToken;  ///< Token from the corresponding ARM command
};

static_assert(sizeof(TimeoutEventMsg) == 15, "TimeoutEventMsg must be 15 bytes");

/// JOIN_GAME: master -> selected slave (header only, 11 bytes)
struct JoinGameMsg {
    MsgHeader header;
};

/// STOP_ALL: master -> selected slave (header only, 11 bytes)
struct StopAllMsg {
    MsgHeader header;
};

/// SIMULATE_TOUCH: master -> slave (16 bytes)
struct SimulateTouchMsg {
    MsgHeader header;
    uint32_t roundToken;  ///< ARM token this injection is allowed to satisfy
    uint8_t padIndex;     ///< Which pad to inject
};

static_assert(sizeof(SimulateTouchMsg) == 16, "SimulateTouchMsg must be 16 bytes");

#pragma pack(pop)

/// Return the only valid wire size for a known message type, or zero for an unknown type.
constexpr size_t expectedMessageSize(MsgType type) {
    switch (type) {
        case MsgType::kBeacon:
        case MsgType::kPing:
        case MsgType::kPong:
            return sizeof(MsgHeader);
        case MsgType::kJoinGame:
            return sizeof(JoinGameMsg);
        case MsgType::kArmTouch:
            return sizeof(ArmTouchMsg);
        case MsgType::kSetColor:
            return sizeof(SetColorMsg);
        case MsgType::kStopAll:
            return sizeof(StopAllMsg);
        case MsgType::kSimulateTouch:
            return sizeof(SimulateTouchMsg);
        case MsgType::kTouchEvent:
            return sizeof(TouchEventMsg);
        case MsgType::kTimeoutEvent:
            return sizeof(TimeoutEventMsg);
        default:
            return 0;
    }
}

/// Verify that the claimed sender is the source authenticated by the radio callback.
inline bool senderMatchesSource(const MsgHeader& header,
                                const uint8_t sourceMac[ESP_NOW_ETH_ALEN]) {
    return sourceMac != nullptr && std::memcmp(header.senderMac, sourceMac, ESP_NOW_ETH_ALEN) == 0;
}

}  // namespace domes::espnow
