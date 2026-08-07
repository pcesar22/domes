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
    kOutputTooSmall,
};

struct LegacyV1Packet {
    std::array<uint8_t, kLegacyV1MaxPacketSize> bytes{};
    size_t size = 0;

    std::span<const uint8_t> view() const { return {bytes.data(), size}; }
};

/** Validate a generated semantic message before it crosses a compatibility boundary. */
CodecError validate(const domes_peer_drill_PeerMessage& message);

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
