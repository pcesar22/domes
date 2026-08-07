#include "protocol/peerDrillCodec.hpp"
#include "services/espNowProtocol.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <gtest/gtest.h>

namespace {

using domes::peer_drill::CodecError;
using domes::peer_drill::LegacyV1Packet;

constexpr std::array<uint8_t, 6> kMac = {0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0};
constexpr uint32_t kTimestamp = 0x11223344;
constexpr uint32_t kToken = 0xA1B2C3D4;

struct Fixture {
    domes_peer_drill_PeerMessage message;
    std::vector<uint8_t> bytes;
};

domes_peer_drill_PeerMessage baseMessage(pb_size_t payloadTag) {
    domes_peer_drill_PeerMessage message = domes_peer_drill_PeerMessage_init_zero;
    message.protocol_version = domes::peer_drill::kLegacyV1ProtocolVersion;
    message.sender_mac.size = kMac.size();
    std::copy(kMac.begin(), kMac.end(), message.sender_mac.bytes);
    message.sender_timestamp_us = kTimestamp;
    message.which_payload = payloadTag;
    return message;
}

std::vector<Fixture> allFixtures() {
    std::vector<Fixture> fixtures;

    fixtures.push_back({baseMessage(domes_peer_drill_PeerMessage_beacon_tag),
                        {0x01, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11}});
    fixtures.push_back({baseMessage(domes_peer_drill_PeerMessage_ping_tag),
                        {0x02, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11}});
    fixtures.push_back({baseMessage(domes_peer_drill_PeerMessage_pong_tag),
                        {0x03, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11}});
    fixtures.push_back({baseMessage(domes_peer_drill_PeerMessage_join_game_tag),
                        {0x10, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11}});

    auto arm = baseMessage(domes_peer_drill_PeerMessage_arm_touch_tag);
    arm.payload.arm_touch.round_token = kToken;
    arm.payload.arm_touch.timeout_ms = 0x01020304;
    arm.payload.arm_touch.feedback_mode = domes_peer_drill_FeedbackMode_FEEDBACK_MODE_LED_AND_AUDIO;
    fixtures.push_back({arm, {0x11, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22,
                              0x11, 0xD4, 0xC3, 0xB2, 0xA1, 0x04, 0x03, 0x02, 0x01, 0x03}});

    auto color = baseMessage(domes_peer_drill_PeerMessage_set_color_tag);
    color.payload.set_color.red = 0xFE;
    color.payload.set_color.green = 0x80;
    color.payload.set_color.blue = 0x01;
    fixtures.push_back(
        {color,
         {0x12, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11, 0xFE, 0x80, 0x01}});

    fixtures.push_back({baseMessage(domes_peer_drill_PeerMessage_stop_all_tag),
                        {0x13, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11}});

    auto simulate = baseMessage(domes_peer_drill_PeerMessage_simulate_touch_tag);
    simulate.payload.simulate_touch.round_token = kToken;
    simulate.payload.simulate_touch.pad_index = 3;
    fixtures.push_back({simulate,
                        {0x14, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11, 0xD4,
                         0xC3, 0xB2, 0xA1, 0x03}});

    auto touch = baseMessage(domes_peer_drill_PeerMessage_touch_event_tag);
    touch.payload.touch_event.round_token = kToken;
    touch.payload.touch_event.reaction_time_us = 0x55667788;
    touch.payload.touch_event.pad_index = 2;
    fixtures.push_back({touch, {0x20, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22,
                                0x11, 0xD4, 0xC3, 0xB2, 0xA1, 0x88, 0x77, 0x66, 0x55, 0x02}});

    auto timeout = baseMessage(domes_peer_drill_PeerMessage_timeout_event_tag);
    timeout.payload.timeout_event.round_token = kToken;
    fixtures.push_back({timeout,
                        {0x21, 0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0, 0x44, 0x33, 0x22, 0x11, 0xD4,
                         0xC3, 0xB2, 0xA1}});

    return fixtures;
}

template <typename Message>
std::vector<uint8_t> objectBytes(const Message& message) {
    const auto* begin = reinterpret_cast<const uint8_t*>(&message);
    return {begin, begin + sizeof(message)};
}

void fillLegacyHeader(domes::espnow::MsgHeader& header, domes::espnow::MsgType type) {
    header.type = static_cast<uint8_t>(type);
    std::copy(kMac.begin(), kMac.end(), header.senderMac);
    header.timestampUs = kTimestamp;
}

std::vector<uint8_t> productionStructBytes(const domes_peer_drill_PeerMessage& semantic) {
    using namespace domes::espnow;
    switch (semantic.which_payload) {
        case domes_peer_drill_PeerMessage_beacon_tag: {
            MsgHeader message{};
            fillLegacyHeader(message, MsgType::kBeacon);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_ping_tag: {
            MsgHeader message{};
            fillLegacyHeader(message, MsgType::kPing);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_pong_tag: {
            MsgHeader message{};
            fillLegacyHeader(message, MsgType::kPong);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_join_game_tag: {
            JoinGameMsg message{};
            fillLegacyHeader(message.header, MsgType::kJoinGame);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_arm_touch_tag: {
            ArmTouchMsg message{};
            fillLegacyHeader(message.header, MsgType::kArmTouch);
            message.roundToken = semantic.payload.arm_touch.round_token;
            message.timeoutMs = semantic.payload.arm_touch.timeout_ms;
            message.feedbackMode = static_cast<uint8_t>(semantic.payload.arm_touch.feedback_mode);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_set_color_tag: {
            SetColorMsg message{};
            fillLegacyHeader(message.header, MsgType::kSetColor);
            message.r = static_cast<uint8_t>(semantic.payload.set_color.red);
            message.g = static_cast<uint8_t>(semantic.payload.set_color.green);
            message.b = static_cast<uint8_t>(semantic.payload.set_color.blue);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_stop_all_tag: {
            StopAllMsg message{};
            fillLegacyHeader(message.header, MsgType::kStopAll);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_simulate_touch_tag: {
            SimulateTouchMsg message{};
            fillLegacyHeader(message.header, MsgType::kSimulateTouch);
            message.roundToken = semantic.payload.simulate_touch.round_token;
            message.padIndex = static_cast<uint8_t>(semantic.payload.simulate_touch.pad_index);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_touch_event_tag: {
            TouchEventMsg message{};
            fillLegacyHeader(message.header, MsgType::kTouchEvent);
            message.roundToken = semantic.payload.touch_event.round_token;
            message.reactionTimeUs = semantic.payload.touch_event.reaction_time_us;
            message.padIndex = static_cast<uint8_t>(semantic.payload.touch_event.pad_index);
            return objectBytes(message);
        }
        case domes_peer_drill_PeerMessage_timeout_event_tag: {
            TimeoutEventMsg message{};
            fillLegacyHeader(message.header, MsgType::kTimeoutEvent);
            message.roundToken = semantic.payload.timeout_event.round_token;
            return objectBytes(message);
        }
        default:
            return {};
    }
}

void expectDecodedFields(const domes_peer_drill_PeerMessage& expected,
                         const domes_peer_drill_PeerMessage& actual) {
    EXPECT_EQ(actual.protocol_version, 1u);
    ASSERT_EQ(actual.sender_mac.size, kMac.size());
    EXPECT_TRUE(std::equal(kMac.begin(), kMac.end(), actual.sender_mac.bytes));
    EXPECT_EQ(actual.sender_timestamp_us, kTimestamp);
    ASSERT_EQ(actual.which_payload, expected.which_payload);

    switch (expected.which_payload) {
        case domes_peer_drill_PeerMessage_arm_touch_tag:
            EXPECT_EQ(actual.payload.arm_touch.round_token, expected.payload.arm_touch.round_token);
            EXPECT_EQ(actual.payload.arm_touch.timeout_ms, expected.payload.arm_touch.timeout_ms);
            EXPECT_EQ(actual.payload.arm_touch.feedback_mode,
                      expected.payload.arm_touch.feedback_mode);
            break;
        case domes_peer_drill_PeerMessage_set_color_tag:
            EXPECT_EQ(actual.payload.set_color.red, expected.payload.set_color.red);
            EXPECT_EQ(actual.payload.set_color.green, expected.payload.set_color.green);
            EXPECT_EQ(actual.payload.set_color.blue, expected.payload.set_color.blue);
            break;
        case domes_peer_drill_PeerMessage_simulate_touch_tag:
            EXPECT_EQ(actual.payload.simulate_touch.round_token,
                      expected.payload.simulate_touch.round_token);
            EXPECT_EQ(actual.payload.simulate_touch.pad_index,
                      expected.payload.simulate_touch.pad_index);
            break;
        case domes_peer_drill_PeerMessage_touch_event_tag:
            EXPECT_EQ(actual.payload.touch_event.round_token,
                      expected.payload.touch_event.round_token);
            EXPECT_EQ(actual.payload.touch_event.reaction_time_us,
                      expected.payload.touch_event.reaction_time_us);
            EXPECT_EQ(actual.payload.touch_event.pad_index, expected.payload.touch_event.pad_index);
            break;
        case domes_peer_drill_PeerMessage_timeout_event_tag:
            EXPECT_EQ(actual.payload.timeout_event.round_token,
                      expected.payload.timeout_event.round_token);
            break;
        default:
            break;
    }
}

TEST(PeerDrillCodecTest, EveryVariantMatchesExactFixtureAndProductionStruct) {
    for (const auto& fixture : allFixtures()) {
        LegacyV1Packet encoded;
        ASSERT_EQ(domes::peer_drill::encodeLegacyV1(fixture.message, encoded), CodecError::kOk)
            << domes::peer_drill::payloadName(fixture.message.which_payload);
        EXPECT_EQ(std::vector<uint8_t>(encoded.view().begin(), encoded.view().end()),
                  fixture.bytes);
        EXPECT_EQ(productionStructBytes(fixture.message), fixture.bytes);

        domes_peer_drill_PeerMessage decoded = domes_peer_drill_PeerMessage_init_zero;
        ASSERT_EQ(domes::peer_drill::decodeLegacyV1(fixture.bytes, decoded), CodecError::kOk);
        expectDecodedFields(fixture.message, decoded);
    }
}

TEST(PeerDrillCodecTest, EveryVariantRejectsShortAndLongWireLengths) {
    for (const auto& fixture : allFixtures()) {
        auto shortBytes = fixture.bytes;
        shortBytes.pop_back();
        domes_peer_drill_PeerMessage decoded = domes_peer_drill_PeerMessage_init_zero;
        EXPECT_EQ(domes::peer_drill::decodeLegacyV1(shortBytes, decoded), CodecError::kBadLength);

        auto longBytes = fixture.bytes;
        longBytes.push_back(0);
        EXPECT_EQ(domes::peer_drill::decodeLegacyV1(longBytes, decoded), CodecError::kBadLength);
    }
}

TEST(PeerDrillCodecTest, RejectsMalformedUnknownVersionAndMacInputs) {
    domes_peer_drill_PeerMessage decoded = domes_peer_drill_PeerMessage_init_zero;
    EXPECT_EQ(domes::peer_drill::decodeLegacyV1({}, decoded), CodecError::kMalformed);

    std::array<uint8_t, 11> unknown{};
    unknown[0] = 0xFF;
    EXPECT_EQ(domes::peer_drill::decodeLegacyV1(unknown, decoded), CodecError::kUnknownType);

    auto message = baseMessage(domes_peer_drill_PeerMessage_beacon_tag);
    for (uint32_t version : {0u, 2u}) {
        message.protocol_version = version;
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(message, encoded),
                  CodecError::kUnsupportedVersion);
    }

    message.protocol_version = 1;
    for (pb_size_t size : {0u, 5u, 7u}) {
        message.sender_mac.size = size;
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(message, encoded), CodecError::kBadMacLength);
    }

    message.sender_mac.size = kMac.size();
    for (pb_size_t tag : {0u, 99u}) {
        message.which_payload = tag;
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(message, encoded), CodecError::kMalformed);
    }
}

TEST(PeerDrillCodecTest, RejectsBadEnumChannelsAndOutputCapacity) {
    auto arm = baseMessage(domes_peer_drill_PeerMessage_arm_touch_tag);
    arm.payload.arm_touch.round_token = kToken;
    for (int mode : {-1, 4}) {
        arm.payload.arm_touch.feedback_mode = static_cast<domes_peer_drill_FeedbackMode>(mode);
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(arm, encoded), CodecError::kBadEnum);
    }

    auto badArmBytes = allFixtures()[4].bytes;
    badArmBytes[19] = 4;
    domes_peer_drill_PeerMessage decoded = domes_peer_drill_PeerMessage_init_zero;
    EXPECT_EQ(domes::peer_drill::decodeLegacyV1(badArmBytes, decoded), CodecError::kBadEnum);

    auto color = baseMessage(domes_peer_drill_PeerMessage_set_color_tag);
    for (uint32_t value : {256u, UINT32_MAX}) {
        color.payload.set_color.red = value;
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(color, encoded), CodecError::kBadChannel);
    }

    color.payload.set_color.red = 255;
    color.payload.set_color.green = 0;
    color.payload.set_color.blue = 0;
    std::array<uint8_t, 13> tooSmall{};
    size_t encodedSize = 99;
    EXPECT_EQ(domes::peer_drill::encodeLegacyV1(color, tooSmall, encodedSize),
              CodecError::kOutputTooSmall);
    EXPECT_EQ(encodedSize, 0u);
}

TEST(PeerDrillCodecTest, RejectsBadPadsAndZeroRoundTokens) {
    auto simulate = baseMessage(domes_peer_drill_PeerMessage_simulate_touch_tag);
    simulate.payload.simulate_touch.round_token = kToken;
    auto touch = baseMessage(domes_peer_drill_PeerMessage_touch_event_tag);
    touch.payload.touch_event.round_token = kToken;

    for (uint32_t pad : {4u, 255u}) {
        simulate.payload.simulate_touch.pad_index = pad;
        touch.payload.touch_event.pad_index = pad;
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(simulate, encoded), CodecError::kBadPad);
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(touch, encoded), CodecError::kBadPad);
    }

    auto badSimBytes = allFixtures()[7].bytes;
    badSimBytes[15] = 4;
    auto badTouchBytes = allFixtures()[8].bytes;
    badTouchBytes[19] = 4;
    domes_peer_drill_PeerMessage decoded = domes_peer_drill_PeerMessage_init_zero;
    EXPECT_EQ(domes::peer_drill::decodeLegacyV1(badSimBytes, decoded), CodecError::kBadPad);
    EXPECT_EQ(domes::peer_drill::decodeLegacyV1(badTouchBytes, decoded), CodecError::kBadPad);

    for (size_t index : {4u, 7u, 8u, 9u}) {
        auto message = allFixtures()[index].message;
        switch (message.which_payload) {
            case domes_peer_drill_PeerMessage_arm_touch_tag:
                message.payload.arm_touch.round_token = 0;
                break;
            case domes_peer_drill_PeerMessage_simulate_touch_tag:
                message.payload.simulate_touch.round_token = 0;
                break;
            case domes_peer_drill_PeerMessage_touch_event_tag:
                message.payload.touch_event.round_token = 0;
                break;
            case domes_peer_drill_PeerMessage_timeout_event_tag:
                message.payload.timeout_event.round_token = 0;
                break;
        }
        LegacyV1Packet encoded;
        EXPECT_EQ(domes::peer_drill::encodeLegacyV1(message, encoded), CodecError::kZeroToken);

        auto bytes = allFixtures()[index].bytes;
        std::fill_n(bytes.begin() + 11, 4, 0);
        EXPECT_EQ(domes::peer_drill::decodeLegacyV1(bytes, decoded), CodecError::kZeroToken);
    }
}

TEST(PeerDrillCodecTest, AcceptsMaximumFixed32RoundToken) {
    auto arm = baseMessage(domes_peer_drill_PeerMessage_arm_touch_tag);
    arm.payload.arm_touch.round_token = UINT32_MAX;
    arm.payload.arm_touch.feedback_mode = domes_peer_drill_FeedbackMode_FEEDBACK_MODE_NONE;

    LegacyV1Packet encoded;
    ASSERT_EQ(domes::peer_drill::encodeLegacyV1(arm, encoded), CodecError::kOk);
    EXPECT_EQ(std::vector<uint8_t>(encoded.bytes.begin() + 11, encoded.bytes.begin() + 15),
              (std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF}));
}

}  // namespace
