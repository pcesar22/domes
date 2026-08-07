#include "peerDrillCodec.hpp"

#include <algorithm>
#include <cstring>

namespace domes::peer_drill {
namespace {

constexpr size_t kArmTouchSize = 20;
constexpr size_t kSetColorSize = 14;
constexpr size_t kSimulateTouchSize = 16;
constexpr size_t kTouchEventSize = 20;
constexpr size_t kTimeoutEventSize = 15;

void writeU32Le(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t readU32Le(const uint8_t* input) {
    return static_cast<uint32_t>(input[0]) | (static_cast<uint32_t>(input[1]) << 8) |
           (static_cast<uint32_t>(input[2]) << 16) | (static_cast<uint32_t>(input[3]) << 24);
}

bool validFeedbackMode(domes_peer_drill_FeedbackMode mode) {
    return mode >= domes_peer_drill_FeedbackMode_FEEDBACK_MODE_NONE &&
           mode <= domes_peer_drill_FeedbackMode_FEEDBACK_MODE_LED_AND_AUDIO;
}

CodecError validatePayload(const domes_peer_drill_PeerMessage& message) {
    switch (message.which_payload) {
        case domes_peer_drill_PeerMessage_beacon_tag:
        case domes_peer_drill_PeerMessage_ping_tag:
        case domes_peer_drill_PeerMessage_pong_tag:
        case domes_peer_drill_PeerMessage_join_game_tag:
        case domes_peer_drill_PeerMessage_stop_all_tag:
            return CodecError::kOk;
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            if (message.payload.arm_touch.round_token == 0) {
                return CodecError::kZeroToken;
            }
            return validFeedbackMode(message.payload.arm_touch.feedback_mode)
                       ? CodecError::kOk
                       : CodecError::kBadEnum;
        case domes_peer_drill_PeerMessage_set_color_tag:
            if (message.payload.set_color.red > kMaxColorChannel ||
                message.payload.set_color.green > kMaxColorChannel ||
                message.payload.set_color.blue > kMaxColorChannel) {
                return CodecError::kBadChannel;
            }
            return CodecError::kOk;
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            if (message.payload.simulate_touch.round_token == 0) {
                return CodecError::kZeroToken;
            }
            return message.payload.simulate_touch.pad_index <= kMaxPadIndex ? CodecError::kOk
                                                                            : CodecError::kBadPad;
        case domes_peer_drill_PeerMessage_touch_event_tag:
            if (message.payload.touch_event.round_token == 0) {
                return CodecError::kZeroToken;
            }
            return message.payload.touch_event.pad_index <= kMaxPadIndex ? CodecError::kOk
                                                                         : CodecError::kBadPad;
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            return message.payload.timeout_event.round_token == 0 ? CodecError::kZeroToken
                                                                  : CodecError::kOk;
        default:
            return CodecError::kMalformed;
    }
}

size_t expectedSizeForType(uint8_t type) {
    switch (type) {
        case domes_peer_drill_PeerMessage_beacon_tag:
        case domes_peer_drill_PeerMessage_ping_tag:
        case domes_peer_drill_PeerMessage_pong_tag:
        case domes_peer_drill_PeerMessage_join_game_tag:
        case domes_peer_drill_PeerMessage_stop_all_tag:
            return kLegacyV1HeaderSize;
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            return kArmTouchSize;
        case domes_peer_drill_PeerMessage_set_color_tag:
            return kSetColorSize;
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            return kSimulateTouchSize;
        case domes_peer_drill_PeerMessage_touch_event_tag:
            return kTouchEventSize;
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            return kTimeoutEventSize;
        default:
            return 0;
    }
}

size_t legacySize(pb_size_t payloadTag) {
    return payloadTag <= UINT8_MAX ? expectedSizeForType(static_cast<uint8_t>(payloadTag)) : 0;
}

bool isDiscoveryPayload(pb_size_t payloadTag) {
    return payloadTag == domes_peer_drill_PeerMessage_beacon_tag ||
           payloadTag == domes_peer_drill_PeerMessage_ping_tag ||
           payloadTag == domes_peer_drill_PeerMessage_pong_tag;
}

domes_peer_drill_PeerRole requiredSenderRole(pb_size_t payloadTag) {
    switch (payloadTag) {
        case domes_peer_drill_PeerMessage_join_game_tag:
        case domes_peer_drill_PeerMessage_arm_touch_tag:
        case domes_peer_drill_PeerMessage_set_color_tag:
        case domes_peer_drill_PeerMessage_stop_all_tag:
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            return domes_peer_drill_PeerRole_PEER_ROLE_MASTER;
        case domes_peer_drill_PeerMessage_touch_event_tag:
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            return domes_peer_drill_PeerRole_PEER_ROLE_SLAVE;
        default:
            return domes_peer_drill_PeerRole_PEER_ROLE_UNSPECIFIED;
    }
}

}  // namespace

CodecError validate(const domes_peer_drill_PeerMessage& message) {
    if (message.protocol_version != kLegacyV1ProtocolVersion) {
        return CodecError::kUnsupportedVersion;
    }
    if (message.sender_mac.size != kSenderMacSize) {
        return CodecError::kBadMacLength;
    }
    return validatePayload(message);
}

CodecError validateForSenderRole(const domes_peer_drill_PeerMessage& message,
                                 domes_peer_drill_PeerRole senderRole) {
    const CodecError structural = validate(message);
    if (structural != CodecError::kOk) {
        return structural;
    }
    if (isDiscoveryPayload(message.which_payload)) {
        return CodecError::kOk;
    }
    if (senderRole == domes_peer_drill_PeerRole_PEER_ROLE_UNSPECIFIED ||
        senderRole != requiredSenderRole(message.which_payload)) {
        return CodecError::kBadRole;
    }
    return CodecError::kOk;
}

CodecError encodeLegacyV1(const domes_peer_drill_PeerMessage& message, std::span<uint8_t> output,
                          size_t& encodedSize) {
    encodedSize = 0;
    const CodecError validation = validate(message);
    if (validation != CodecError::kOk) {
        return validation;
    }

    const size_t requiredSize = legacySize(message.which_payload);
    if (output.size() < requiredSize) {
        return CodecError::kOutputTooSmall;
    }

    output[0] = static_cast<uint8_t>(message.which_payload);
    std::copy_n(message.sender_mac.bytes, kSenderMacSize, output.begin() + 1);
    writeU32Le(output.data() + 7, message.timestamp_us);

    switch (message.which_payload) {
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            writeU32Le(output.data() + 11, message.payload.arm_touch.round_token);
            writeU32Le(output.data() + 15, message.payload.arm_touch.timeout_ms);
            output[19] = static_cast<uint8_t>(message.payload.arm_touch.feedback_mode);
            break;
        case domes_peer_drill_PeerMessage_set_color_tag:
            output[11] = static_cast<uint8_t>(message.payload.set_color.red);
            output[12] = static_cast<uint8_t>(message.payload.set_color.green);
            output[13] = static_cast<uint8_t>(message.payload.set_color.blue);
            break;
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            writeU32Le(output.data() + 11, message.payload.simulate_touch.round_token);
            output[15] = static_cast<uint8_t>(message.payload.simulate_touch.pad_index);
            break;
        case domes_peer_drill_PeerMessage_touch_event_tag:
            writeU32Le(output.data() + 11, message.payload.touch_event.round_token);
            writeU32Le(output.data() + 15, message.payload.touch_event.reaction_time_us);
            output[19] = static_cast<uint8_t>(message.payload.touch_event.pad_index);
            break;
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            writeU32Le(output.data() + 11, message.payload.timeout_event.round_token);
            break;
        default:
            break;
    }

    encodedSize = requiredSize;
    return CodecError::kOk;
}

CodecError encodeLegacyV1(const domes_peer_drill_PeerMessage& message, LegacyV1Packet& output) {
    output.size = 0;
    return encodeLegacyV1(message, output.bytes, output.size);
}

CodecError decodeLegacyV1(std::span<const uint8_t> input, domes_peer_drill_PeerMessage& message) {
    if (input.empty()) {
        return CodecError::kMalformed;
    }

    const size_t expectedSize = expectedSizeForType(input[0]);
    if (expectedSize == 0) {
        return CodecError::kUnknownType;
    }
    if (input.size() != expectedSize) {
        return CodecError::kBadLength;
    }

    message = domes_peer_drill_PeerMessage_init_zero;
    message.protocol_version = kLegacyV1ProtocolVersion;
    message.sender_mac.size = kSenderMacSize;
    std::copy_n(input.begin() + 1, kSenderMacSize, message.sender_mac.bytes);
    message.timestamp_us = readU32Le(input.data() + 7);
    message.which_payload = input[0];

    switch (message.which_payload) {
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            message.payload.arm_touch.round_token = readU32Le(input.data() + 11);
            message.payload.arm_touch.timeout_ms = readU32Le(input.data() + 15);
            message.payload.arm_touch.feedback_mode =
                static_cast<domes_peer_drill_FeedbackMode>(input[19]);
            break;
        case domes_peer_drill_PeerMessage_set_color_tag:
            message.payload.set_color.red = input[11];
            message.payload.set_color.green = input[12];
            message.payload.set_color.blue = input[13];
            break;
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            message.payload.simulate_touch.round_token = readU32Le(input.data() + 11);
            message.payload.simulate_touch.pad_index = input[15];
            break;
        case domes_peer_drill_PeerMessage_touch_event_tag:
            message.payload.touch_event.round_token = readU32Le(input.data() + 11);
            message.payload.touch_event.reaction_time_us = readU32Le(input.data() + 15);
            message.payload.touch_event.pad_index = input[19];
            break;
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            message.payload.timeout_event.round_token = readU32Le(input.data() + 11);
            break;
        default:
            break;
    }

    return validate(message);
}

const char* payloadName(pb_size_t payloadTag) {
    switch (payloadTag) {
        case domes_peer_drill_PeerMessage_beacon_tag:
            return "BEACON";
        case domes_peer_drill_PeerMessage_ping_tag:
            return "PING";
        case domes_peer_drill_PeerMessage_pong_tag:
            return "PONG";
        case domes_peer_drill_PeerMessage_join_game_tag:
            return "JOIN_GAME";
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            return "ARM_TOUCH";
        case domes_peer_drill_PeerMessage_set_color_tag:
            return "SET_COLOR";
        case domes_peer_drill_PeerMessage_stop_all_tag:
            return "STOP_ALL";
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            return "SIMULATE_TOUCH";
        case domes_peer_drill_PeerMessage_touch_event_tag:
            return "TOUCH_EVENT";
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            return "TIMEOUT_EVENT";
        default:
            return "UNKNOWN";
    }
}

}  // namespace domes::peer_drill
