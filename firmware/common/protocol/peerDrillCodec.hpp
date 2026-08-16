#pragma once

#include "peer_drill.pb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace domes::peer_drill {

constexpr uint32_t kLegacyV1ProtocolVersion = 1;
constexpr size_t kSenderMacSize = 6;
constexpr size_t kLegacyV1HeaderSize = 11;
constexpr size_t kLegacyV1MaxPacketSize = 20;
constexpr uint32_t kMaxColorChannel = 255;
constexpr uint32_t kMaxPadIndex = 3;

static_assert(domes_peer_drill_PeerMessage_beacon_tag == 0x01);
static_assert(domes_peer_drill_PeerMessage_ping_tag == 0x02);
static_assert(domes_peer_drill_PeerMessage_pong_tag == 0x03);
static_assert(domes_peer_drill_PeerMessage_join_game_tag == 0x10);
static_assert(domes_peer_drill_PeerMessage_arm_touch_tag == 0x11);
static_assert(domes_peer_drill_PeerMessage_set_color_tag == 0x12);
static_assert(domes_peer_drill_PeerMessage_stop_all_tag == 0x13);
static_assert(domes_peer_drill_PeerMessage_simulate_touch_tag == 0x14);
static_assert(domes_peer_drill_PeerMessage_touch_event_tag == 0x20);
static_assert(domes_peer_drill_PeerMessage_timeout_event_tag == 0x21);
static_assert(domes_peer_drill_PeerMessage_size <= 250,
              "The bounded semantic message must fit one ESP-NOW payload");

enum class CodecError : uint8_t {
    kOk,
    kMalformed,
    kUnknownType,
    kUnsupportedVersion,
    kBadLength,
    kBadMacLength,
    kBadEnum,
    kBadChannel,
    kBadPad,
    kZeroToken,
    kBadRole,
    kOutputTooSmall,
};

struct LegacyV1Packet {
    std::array<uint8_t, kLegacyV1MaxPacketSize> bytes{};
    size_t size = 0;

    std::span<const uint8_t> view() const { return {bytes.data(), size}; }
};

/** Validate a generated semantic message before it crosses a compatibility boundary. */
CodecError validate(const domes_peer_drill_PeerMessage& message);

/** Validate semantic fields plus the authenticated sender's protocol role. */
CodecError validateForSenderRole(const domes_peer_drill_PeerMessage& message,
                                 domes_peer_drill_PeerRole senderRole);

/** Encode the generated semantic message into the exact current ESP-NOW Legacy-V1 bytes. */
CodecError encodeLegacyV1(const domes_peer_drill_PeerMessage& message, std::span<uint8_t> output,
                          size_t& encodedSize);

/** Encode into fixed bounded storage sized for the largest current Legacy-V1 variant. */
CodecError encodeLegacyV1(const domes_peer_drill_PeerMessage& message, LegacyV1Packet& output);

/** Decode exact Legacy-V1 bytes into the generated semantic message and fail closed. */
CodecError decodeLegacyV1(std::span<const uint8_t> input, domes_peer_drill_PeerMessage& message);

/** Stable name derived from the generated oneof discriminator. */
const char* payloadName(pb_size_t payloadTag);

}  // namespace domes::peer_drill
