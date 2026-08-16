#pragma once

#include "protocol/peerDrillCodec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace sim {

constexpr uint16_t kBroadcastPodId = 0xFFFF;
constexpr auto kMasterPeerRole = domes_peer_drill_PeerRole_PEER_ROLE_MASTER;
constexpr auto kSlavePeerRole = domes_peer_drill_PeerRole_PEER_ROLE_SLAVE;

/**
 * Simulator-only transport metadata around the generated production semantic message.
 *
 * Destination, virtual time, sequence, fault policy, and replay scheduling are deliberately
 * outside domes.peer_drill.PeerMessage. The legacy bytes are produced by the production codec.
 */
struct SimMessage {
    uint16_t srcPodId = 0;
    uint16_t dstPodId = 0;
    uint64_t sentAtUs = 0;
    uint32_t sequence = 0;
    domes_peer_drill_PeerMessage semantic = domes_peer_drill_PeerMessage_init_zero;
    domes::peer_drill::LegacyV1Packet legacy;
};

inline std::array<uint8_t, domes::peer_drill::kSenderMacSize> senderMacForPod(uint16_t podId) {
    return {0x02, 0x44, 0x4F, 0x4D, static_cast<uint8_t>(podId >> 8), static_cast<uint8_t>(podId)};
}

inline SimMessage makeMessage(uint16_t srcPodId, uint16_t dstPodId, pb_size_t payloadTag) {
    SimMessage message;
    message.srcPodId = srcPodId;
    message.dstPodId = dstPodId;
    message.semantic.protocol_version = domes::peer_drill::kLegacyV1ProtocolVersion;
    const auto mac = senderMacForPod(srcPodId);
    message.semantic.sender_mac.size = mac.size();
    std::copy(mac.begin(), mac.end(), message.semantic.sender_mac.bytes);
    message.semantic.which_payload = payloadTag;
    return message;
}

inline pb_size_t messageTag(const SimMessage& message) {
    return message.semantic.which_payload;
}

inline const char* messageTypeName(pb_size_t payloadTag) {
    return domes::peer_drill::payloadName(payloadTag);
}

}  // namespace sim
