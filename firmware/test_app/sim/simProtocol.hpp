#pragma once

#include "pb_encode.h"
#include "peer_drill.pb.h"
#include "services/espNowProtocol.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace sim {

using SimMessage = domes_peer_PeerMessage;
using SimMessageHeader = domes_peer_PeerHeader;
using SimMessageType = domes_peer_PeerMessageType;

inline constexpr uint32_t kBroadcastPodId = 0xFFFF;

inline SimMessage makeMessage(SimMessageType type, uint32_t srcPodId, uint32_t dstPodId) {
    SimMessage message = domes_peer_PeerMessage_init_zero;
    domes::espnow::initializeMessage(message, type);
    message.header.src_pod_id = srcPodId;
    message.header.dst_pod_id = dstPodId;
    message.header.sender_role =
        domes::espnow::isDiscoveryMessage(type)
            ? domes::espnow::kRoleUnspecified
            : ((type == domes::espnow::kTouchEvent || type == domes::espnow::kTimeoutEvent)
                   ? domes::espnow::kRoleSlave
                   : domes::espnow::kRoleMaster);
    if (type == domes::espnow::kJoinGame)
        message.payload.join_game.assigned_role = domes::espnow::kRoleSlave;
    return message;
}

inline SimMessage makeSetColor(uint32_t src, uint32_t dst, uint8_t r, uint8_t g, uint8_t b) {
    auto message = makeMessage(domes::espnow::kSetColor, src, dst);
    message.payload.set_color = {r, g, b};
    return message;
}

inline SimMessage makeArmTouch(uint32_t src, uint32_t dst, uint32_t timeoutMs, uint8_t feedbackMode,
                               uint32_t roundToken) {
    auto message = makeMessage(domes::espnow::kArmTouch, src, dst);
    message.payload.arm_touch = {roundToken, timeoutMs, feedbackMode};
    return message;
}

inline SimMessage makeTouchEvent(uint32_t src, uint32_t dst, uint32_t reactionTimeUs,
                                 uint8_t padIndex, uint32_t roundToken) {
    auto message = makeMessage(domes::espnow::kTouchEvent, src, dst);
    message.payload.touch_event = {roundToken, reactionTimeUs, padIndex};
    return message;
}

inline SimMessage makeTimeoutEvent(uint32_t src, uint32_t dst, uint32_t roundToken) {
    auto message = makeMessage(domes::espnow::kTimeoutEvent, src, dst);
    message.payload.timeout_event = {roundToken};
    return message;
}

inline const SimMessageHeader& getHeader(const SimMessage& message) {
    return message.header;
}
inline SimMessageHeader& getMutableHeader(SimMessage& message) {
    return message.header;
}
inline SimMessageType messageType(const SimMessage& message) {
    return domes::espnow::messageType(message);
}

inline std::vector<uint8_t> canonicalPayload(const SimMessage& message) {
    std::array<uint8_t, domes_peer_PeerMessage_size> encoded{};
    pb_ostream_t stream = pb_ostream_from_buffer(encoded.data(), encoded.size());
    if (!pb_encode(&stream, domes_peer_PeerMessage_fields, &message))
        return {};
    return {encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(stream.bytes_written)};
}

inline const char* messageTypeName(SimMessageType type) {
    return domes::espnow::msgTypeName(type);
}

}  // namespace sim
