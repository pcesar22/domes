#pragma once

/**
 * @file espNowProtocol.hpp
 * @brief Lossless version-1 ESP-NOW wire view of the generated peer contract
 */

#include "esp_now.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "peer_drill.pb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace domes::espnow {

using Message = domes_peer_PeerMessage;
using MsgType = domes_peer_PeerMessageType;
using Role = domes_peer_PeerRole;
using LifecycleState = domes_peer_PeerLifecycleState;

inline constexpr MsgType kBeacon = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_BEACON;
inline constexpr MsgType kPing = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_PING;
inline constexpr MsgType kPong = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_PONG;
inline constexpr MsgType kJoinGame = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_JOIN_GAME;
inline constexpr MsgType kArmTouch = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_ARM_TOUCH;
inline constexpr MsgType kSetColor = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_SET_COLOR;
inline constexpr MsgType kStopAll = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_STOP_ALL;
inline constexpr MsgType kSimulateTouch =
    domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_SIMULATE_TOUCH;
inline constexpr MsgType kTouchEvent = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_TOUCH_EVENT;
inline constexpr MsgType kTimeoutEvent = domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_TIMEOUT_EVENT;

inline constexpr Role kRoleUnspecified = domes_peer_PeerRole_PEER_ROLE_UNSPECIFIED;
inline constexpr Role kRoleMaster = domes_peer_PeerRole_PEER_ROLE_MASTER;
inline constexpr Role kRoleSlave = domes_peer_PeerRole_PEER_ROLE_SLAVE;

inline const char* msgTypeName(MsgType type) {
    switch (type) {
        case kBeacon:
            return "BEACON";
        case kPing:
            return "PING";
        case kPong:
            return "PONG";
        case kJoinGame:
            return "JOIN_GAME";
        case kArmTouch:
            return "ARM_TOUCH";
        case kSetColor:
            return "SET_COLOR";
        case kStopAll:
            return "STOP_ALL";
        case kSimulateTouch:
            return "SIMULATE_TOUCH";
        case kTouchEvent:
            return "TOUCH_EVENT";
        case kTimeoutEvent:
            return "TIMEOUT_EVENT";
        default:
            return "UNKNOWN";
    }
}

constexpr bool isDiscoveryMessage(MsgType type) {
    return type == kBeacon || type == kPing || type == kPong;
}

constexpr bool matchesActiveRound(uint32_t activeToken, uint32_t receivedToken) {
    return activeToken != 0 && receivedToken == activeToken;
}

#pragma pack(push, 1)
struct MsgHeader {
    uint8_t type;
    uint8_t senderMac[ESP_NOW_ETH_ALEN];
    uint32_t timestampUs;
};
struct ArmTouchMsg {
    MsgHeader header;
    uint32_t roundToken;
    uint32_t timeoutMs;
    uint8_t feedbackMode;
};
struct SetColorMsg {
    MsgHeader header;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
struct TouchEventMsg {
    MsgHeader header;
    uint32_t roundToken;
    uint32_t reactionTimeUs;
    uint8_t padIndex;
};
struct TimeoutEventMsg {
    MsgHeader header;
    uint32_t roundToken;
};
struct JoinGameMsg {
    MsgHeader header;
};
struct StopAllMsg {
    MsgHeader header;
};
struct SimulateTouchMsg {
    MsgHeader header;
    uint32_t roundToken;
    uint8_t padIndex;
};
#pragma pack(pop)

static_assert(sizeof(MsgHeader) == 11);
static_assert(sizeof(ArmTouchMsg) == 20);
static_assert(sizeof(SetColorMsg) == 14);
static_assert(sizeof(TouchEventMsg) == 20);
static_assert(sizeof(TimeoutEventMsg) == 15);
static_assert(sizeof(SimulateTouchMsg) == 16);

constexpr size_t expectedMessageSize(MsgType type) {
    switch (type) {
        case kBeacon:
        case kPing:
        case kPong:
            return sizeof(MsgHeader);
        case kJoinGame:
            return sizeof(JoinGameMsg);
        case kArmTouch:
            return sizeof(ArmTouchMsg);
        case kSetColor:
            return sizeof(SetColorMsg);
        case kStopAll:
            return sizeof(StopAllMsg);
        case kSimulateTouch:
            return sizeof(SimulateTouchMsg);
        case kTouchEvent:
            return sizeof(TouchEventMsg);
        case kTimeoutEvent:
            return sizeof(TimeoutEventMsg);
        default:
            return 0;
    }
}

inline MsgType messageType(const Message& message) {
    switch (message.which_payload) {
        case domes_peer_PeerMessage_beacon_tag:
            return kBeacon;
        case domes_peer_PeerMessage_ping_tag:
            return kPing;
        case domes_peer_PeerMessage_pong_tag:
            return kPong;
        case domes_peer_PeerMessage_join_game_tag:
            return kJoinGame;
        case domes_peer_PeerMessage_arm_touch_tag:
            return kArmTouch;
        case domes_peer_PeerMessage_set_color_tag:
            return kSetColor;
        case domes_peer_PeerMessage_stop_all_tag:
            return kStopAll;
        case domes_peer_PeerMessage_simulate_touch_tag:
            return kSimulateTouch;
        case domes_peer_PeerMessage_touch_event_tag:
            return kTouchEvent;
        case domes_peer_PeerMessage_timeout_event_tag:
            return kTimeoutEvent;
        default:
            return domes_peer_PeerMessageType_PEER_MESSAGE_TYPE_UNKNOWN;
    }
}

inline bool hasValidFields(const Message& message) {
    if (!message.has_header ||
        message.header.version != domes_peer_ContractVersion_CONTRACT_VERSION_1 ||
        message.header.src_pod_id > UINT16_MAX || message.header.dst_pod_id > UINT16_MAX ||
        (message.header.sender_mac.size != 0 && message.header.sender_mac.size != ESP_NOW_ETH_ALEN))
        return false;
    switch (messageType(message)) {
        case kBeacon:
        case kPing:
        case kPong:
        case kStopAll:
            return true;
        case kJoinGame:
            return message.payload.join_game.assigned_role == kRoleSlave;
        case kArmTouch:
            return message.payload.arm_touch.round_token != 0 &&
                   message.payload.arm_touch.timeout_ms > 0 &&
                   message.payload.arm_touch.timeout_ms <= 60000 &&
                   message.payload.arm_touch.feedback_mode <= 0x03;
        case kSetColor:
            return message.payload.set_color.r <= 255 && message.payload.set_color.g <= 255 &&
                   message.payload.set_color.b <= 255;
        case kSimulateTouch:
            return message.payload.simulate_touch.round_token != 0 &&
                   message.payload.simulate_touch.pad_index <= 3;
        case kTouchEvent:
            return message.payload.touch_event.round_token != 0 &&
                   message.payload.touch_event.pad_index <= 3;
        case kTimeoutEvent:
            return message.payload.timeout_event.round_token != 0;
        default:
            return false;
    }
}

inline bool hasValidRole(const Message& message) {
    const auto type = messageType(message);
    const auto role = message.header.sender_role;
    if (isDiscoveryMessage(type))
        return role == kRoleUnspecified;
    if (type == kTouchEvent || type == kTimeoutEvent)
        return role == kRoleSlave;
    return role == kRoleMaster;
}

inline bool encodePortableMessage(const Message& message, uint8_t* output, size_t capacity,
                                  size_t& encodedSize) {
    encodedSize = 0;
    if (output == nullptr || capacity == 0 || !hasValidFields(message) || !hasValidRole(message))
        return false;
    pb_ostream_t stream = pb_ostream_from_buffer(output, capacity);
    if (!pb_encode(&stream, domes_peer_PeerMessage_fields, &message))
        return false;
    encodedSize = stream.bytes_written;
    return true;
}

inline bool decodePortableMessage(const uint8_t* data, size_t size, Message& message) {
    if (data == nullptr || size == 0 || size > domes_peer_PeerMessage_size)
        return false;
    message = domes_peer_PeerMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    if (!pb_decode(&stream, domes_peer_PeerMessage_fields, &message) || stream.bytes_left != 0 ||
        !hasValidFields(message) || !hasValidRole(message))
        return false;

    // Nanopb intentionally skips unknown fields. This contract fails closed,
    // so require the input to be exactly the canonical generated encoding.
    std::array<uint8_t, domes_peer_PeerMessage_size> canonical{};
    size_t canonicalSize = 0;
    return encodePortableMessage(message, canonical.data(), canonical.size(), canonicalSize) &&
           canonicalSize == size && std::memcmp(canonical.data(), data, size) == 0;
}

inline bool allowedInState(const Message& message, Role receiverRole, LifecycleState state) {
    if (receiverRole != kRoleMaster && receiverRole != kRoleSlave)
        return false;
    const auto type = messageType(message);
    if (isDiscoveryMessage(type))
        return state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_DISCOVERY ||
               state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_READY ||
               state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_ARMED;
    if (state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_DISCOVERY)
        return receiverRole == kRoleSlave && type == kJoinGame;
    if (state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_READY) {
        if (type == kStopAll)
            return true;
        return (receiverRole == kRoleSlave && (type == kArmTouch || type == kSetColor)) ||
               (receiverRole == kRoleMaster && (type == kTouchEvent || type == kTimeoutEvent));
    }
    if (state == domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_ARMED)
        return receiverRole == kRoleSlave && (type == kSimulateTouch || type == kStopAll);
    return false;
}

inline void initializeMessage(Message& message, MsgType type) {
    message = domes_peer_PeerMessage_init_zero;
    message.has_header = true;
    message.header.version = domes_peer_ContractVersion_CONTRACT_VERSION_1;
    switch (type) {
        case kBeacon:
            message.which_payload = domes_peer_PeerMessage_beacon_tag;
            break;
        case kPing:
            message.which_payload = domes_peer_PeerMessage_ping_tag;
            break;
        case kPong:
            message.which_payload = domes_peer_PeerMessage_pong_tag;
            break;
        case kJoinGame:
            message.which_payload = domes_peer_PeerMessage_join_game_tag;
            break;
        case kArmTouch:
            message.which_payload = domes_peer_PeerMessage_arm_touch_tag;
            break;
        case kSetColor:
            message.which_payload = domes_peer_PeerMessage_set_color_tag;
            break;
        case kStopAll:
            message.which_payload = domes_peer_PeerMessage_stop_all_tag;
            break;
        case kSimulateTouch:
            message.which_payload = domes_peer_PeerMessage_simulate_touch_tag;
            break;
        case kTouchEvent:
            message.which_payload = domes_peer_PeerMessage_touch_event_tag;
            break;
        case kTimeoutEvent:
            message.which_payload = domes_peer_PeerMessage_timeout_event_tag;
            break;
        default:
            break;
    }
}

inline bool decodeLegacyMessage(const uint8_t* data, size_t len, Message& message) {
    if (data == nullptr || len < sizeof(MsgHeader))
        return false;
    const auto* header = reinterpret_cast<const MsgHeader*>(data);
    const auto type = static_cast<MsgType>(header->type);
    if (expectedMessageSize(type) == 0 || len != expectedMessageSize(type))
        return false;
    initializeMessage(message, type);
    message.header.timestamp_us = header->timestampUs;
    message.header.sender_mac.size = ESP_NOW_ETH_ALEN;
    std::memcpy(message.header.sender_mac.bytes, header->senderMac, ESP_NOW_ETH_ALEN);
    switch (type) {
        case kJoinGame:
            message.payload.join_game.assigned_role = kRoleSlave;
            break;
        case kArmTouch: {
            const auto* value = reinterpret_cast<const ArmTouchMsg*>(data);
            message.payload.arm_touch = {value->roundToken, value->timeoutMs, value->feedbackMode};
            break;
        }
        case kSetColor: {
            const auto* value = reinterpret_cast<const SetColorMsg*>(data);
            message.payload.set_color = {value->r, value->g, value->b};
            break;
        }
        case kSimulateTouch: {
            const auto* value = reinterpret_cast<const SimulateTouchMsg*>(data);
            message.payload.simulate_touch = {value->roundToken, value->padIndex};
            break;
        }
        case kTouchEvent: {
            const auto* value = reinterpret_cast<const TouchEventMsg*>(data);
            message.payload.touch_event = {value->roundToken, value->reactionTimeUs,
                                           value->padIndex};
            break;
        }
        case kTimeoutEvent: {
            const auto* value = reinterpret_cast<const TimeoutEventMsg*>(data);
            message.payload.timeout_event = {value->roundToken};
            break;
        }
        default:
            break;
    }
    return hasValidFields(message);
}

inline bool senderMatchesSource(const Message& message, const uint8_t sourceMac[ESP_NOW_ETH_ALEN]) {
    return sourceMac != nullptr && message.header.sender_mac.size == ESP_NOW_ETH_ALEN &&
           std::memcmp(message.header.sender_mac.bytes, sourceMac, ESP_NOW_ETH_ALEN) == 0;
}

inline bool senderMatchesSource(const MsgHeader& header,
                                const uint8_t sourceMac[ESP_NOW_ETH_ALEN]) {
    return sourceMac != nullptr && std::memcmp(header.senderMac, sourceMac, ESP_NOW_ETH_ALEN) == 0;
}

}  // namespace domes::espnow
