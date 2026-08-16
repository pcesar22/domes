#include "services/espNowProtocol.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace {

using domes::espnow::ArmTouchMsg;
using domes::espnow::JoinGameMsg;
using domes::espnow::Message;
using domes::espnow::MsgHeader;
using domes::espnow::MsgType;
using domes::espnow::SetColorMsg;
using domes::espnow::SimulateTouchMsg;
using domes::espnow::StopAllMsg;
using domes::espnow::TimeoutEventMsg;
using domes::espnow::TouchEventMsg;
using namespace domes::espnow;

TEST(EspNowProtocolTest, DefinesExactWireSizeForEveryMessage) {
    EXPECT_EQ(domes::espnow::expectedMessageSize(kBeacon), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kPing), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kPong), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kJoinGame), sizeof(JoinGameMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kArmTouch), sizeof(ArmTouchMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kSetColor), sizeof(SetColorMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kStopAll), sizeof(StopAllMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kSimulateTouch), sizeof(SimulateTouchMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kTouchEvent), sizeof(TouchEventMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(kTimeoutEvent), sizeof(TimeoutEventMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(static_cast<MsgType>(0xFF)), 0U);
}

TEST(EspNowProtocolTest, OnlyBeaconPingAndPongAreDiscoveryTraffic) {
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(kBeacon));
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(kPing));
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(kPong));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(kJoinGame));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(kArmTouch));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(kTouchEvent));
}

TEST(EspNowProtocolTest, SenderMustMatchRadioSource) {
    constexpr std::array<uint8_t, ESP_NOW_ETH_ALEN> kSource = {0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0};
    MsgHeader header = {};
    std::memcpy(header.senderMac, kSource.data(), kSource.size());

    EXPECT_TRUE(domes::espnow::senderMatchesSource(header, kSource.data()));

    auto differentSource = kSource;
    differentSource.back() ^= 0x01;
    EXPECT_FALSE(domes::espnow::senderMatchesSource(header, differentSource.data()));
    EXPECT_FALSE(domes::espnow::senderMatchesSource(header, nullptr));
}

TEST(EspNowProtocolTest, RoundTokenIsEchoedByEveryRoundScopedMessage) {
    constexpr uint32_t kRoundToken = 0xA1B2C3D4;
    ArmTouchMsg arm = {};
    arm.roundToken = kRoundToken;

    TouchEventMsg touch = {};
    touch.roundToken = arm.roundToken;
    TimeoutEventMsg timeout = {};
    timeout.roundToken = arm.roundToken;
    SimulateTouchMsg simulate = {};
    simulate.roundToken = arm.roundToken;

    EXPECT_EQ(touch.roundToken, kRoundToken);
    EXPECT_EQ(timeout.roundToken, kRoundToken);
    EXPECT_EQ(simulate.roundToken, kRoundToken);
    EXPECT_TRUE(domes::espnow::matchesActiveRound(kRoundToken, touch.roundToken));
    EXPECT_FALSE(domes::espnow::matchesActiveRound(kRoundToken, kRoundToken - 1));
    EXPECT_FALSE(domes::espnow::matchesActiveRound(0, 0));
}

Message makeValidMessage(MsgType type) {
    Message message = domes_peer_PeerMessage_init_zero;
    initializeMessage(message, type);
    message.header.sender_role = isDiscoveryMessage(type) ? kRoleUnspecified : kRoleMaster;
    if (type == kJoinGame)
        message.payload.join_game.assigned_role = kRoleSlave;
    if (type == kArmTouch)
        message.payload.arm_touch = {7, 3000, 3};
    if (type == kSetColor)
        message.payload.set_color = {1, 2, 3};
    if (type == kSimulateTouch)
        message.payload.simulate_touch = {7, 0};
    if (type == kTouchEvent) {
        message.header.sender_role = kRoleSlave;
        message.payload.touch_event = {7, 100, 0};
    }
    if (type == kTimeoutEvent) {
        message.header.sender_role = kRoleSlave;
        message.payload.timeout_event = {7};
    }
    return message;
}

TEST(EspNowProtocolTest, EveryVariantRoundTripsPortableGeneratedEncoding) {
    constexpr MsgType kTypes[] = {kBeacon,   kPing,    kPong,          kJoinGame,   kArmTouch,
                                  kSetColor, kStopAll, kSimulateTouch, kTouchEvent, kTimeoutEvent};
    for (const auto type : kTypes) {
        const auto input = makeValidMessage(type);
        std::array<uint8_t, domes_peer_PeerMessage_size> encoded{};
        size_t encodedSize = 0;
        ASSERT_TRUE(encodePortableMessage(input, encoded.data(), encoded.size(), encodedSize));
        Message decoded = domes_peer_PeerMessage_init_zero;
        ASSERT_TRUE(decodePortableMessage(encoded.data(), encodedSize, decoded));
        EXPECT_EQ(messageType(decoded), type);
        EXPECT_EQ(decoded.which_payload, input.which_payload);
    }
}

TEST(EspNowProtocolTest, PortableDecoderRejectsMalformedUnknownTruncatedAndOversized) {
    const auto input = makeValidMessage(kArmTouch);
    std::array<uint8_t, domes_peer_PeerMessage_size> encoded{};
    size_t encodedSize = 0;
    ASSERT_TRUE(encodePortableMessage(input, encoded.data(), encoded.size(), encodedSize));
    Message decoded = domes_peer_PeerMessage_init_zero;

    EXPECT_FALSE(decodePortableMessage(nullptr, encodedSize, decoded));
    EXPECT_FALSE(decodePortableMessage(encoded.data(), encodedSize - 1, decoded));
    std::array<uint8_t, domes_peer_PeerMessage_size + 1> oversized{};
    EXPECT_FALSE(decodePortableMessage(oversized.data(), oversized.size(), decoded));

    std::vector<uint8_t> unknown(encoded.begin(), encoded.begin() + encodedSize);
    unknown.insert(unknown.end(), {0xF8, 0x01, 0x01});
    EXPECT_FALSE(decodePortableMessage(unknown.data(), unknown.size(), decoded));
}

TEST(EspNowProtocolTest, SemanticValidationRejectsRoleAndStateViolations) {
    auto control = makeValidMessage(kArmTouch);
    control.header.sender_role = kRoleSlave;
    EXPECT_FALSE(hasValidRole(control));
    control.header.sender_role = kRoleMaster;
    EXPECT_FALSE(allowedInState(control, kRoleSlave,
                                domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_ARMED));
    EXPECT_TRUE(allowedInState(control, kRoleSlave,
                               domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_READY));

    control.payload.arm_touch.round_token = 0;
    EXPECT_FALSE(hasValidFields(control));
    control.payload.arm_touch = {1, 60001, 3};
    EXPECT_FALSE(hasValidFields(control));
}

TEST(EspNowProtocolTest, LegacyDecoderRejectsUnknownTruncatedOversizedAndInvalidPayloads) {
    ArmTouchMsg arm = {};
    arm.header.type = static_cast<uint8_t>(kArmTouch);
    arm.roundToken = 1;
    arm.timeoutMs = 3000;
    arm.feedbackMode = 3;
    Message decoded = domes_peer_PeerMessage_init_zero;
    EXPECT_TRUE(decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&arm), sizeof(arm), decoded));
    EXPECT_EQ(decoded.which_payload, domes_peer_PeerMessage_arm_touch_tag);
    EXPECT_FALSE(
        decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&arm), sizeof(arm) - 1, decoded));
    EXPECT_FALSE(
        decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&arm), sizeof(arm) + 1, decoded));
    arm.header.type = 0xFF;
    EXPECT_FALSE(decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&arm), sizeof(arm), decoded));
    arm.header.type = static_cast<uint8_t>(kArmTouch);
    arm.roundToken = 0;
    EXPECT_FALSE(decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&arm), sizeof(arm), decoded));
}

TEST(EspNowProtocolTest, EveryLegacyWireVariantMapsLosslesslyToGeneratedPayload) {
    Message decoded = domes_peer_PeerMessage_init_zero;
    auto expectDecoded = [&decoded](const auto& wire, MsgType expected) {
        ASSERT_TRUE(
            decodeLegacyMessage(reinterpret_cast<const uint8_t*>(&wire), sizeof(wire), decoded));
        EXPECT_EQ(messageType(decoded), expected);
    };

    MsgHeader beacon = {};
    beacon.type = static_cast<uint8_t>(kBeacon);
    MsgHeader ping = {};
    ping.type = static_cast<uint8_t>(kPing);
    MsgHeader pong = {};
    pong.type = static_cast<uint8_t>(kPong);
    JoinGameMsg join = {};
    join.header.type = static_cast<uint8_t>(kJoinGame);
    ArmTouchMsg arm = {};
    arm.header.type = static_cast<uint8_t>(kArmTouch);
    arm.roundToken = 1;
    arm.timeoutMs = 3000;
    arm.feedbackMode = 3;
    SetColorMsg color = {};
    color.header.type = static_cast<uint8_t>(kSetColor);
    color.r = 1;
    color.g = 2;
    color.b = 3;
    StopAllMsg stop = {};
    stop.header.type = static_cast<uint8_t>(kStopAll);
    SimulateTouchMsg simulate = {};
    simulate.header.type = static_cast<uint8_t>(kSimulateTouch);
    simulate.roundToken = 1;
    simulate.padIndex = 0;
    TouchEventMsg touch = {};
    touch.header.type = static_cast<uint8_t>(kTouchEvent);
    touch.roundToken = 1;
    touch.reactionTimeUs = 100;
    touch.padIndex = 0;
    TimeoutEventMsg timeout = {};
    timeout.header.type = static_cast<uint8_t>(kTimeoutEvent);
    timeout.roundToken = 1;

    expectDecoded(beacon, kBeacon);
    expectDecoded(ping, kPing);
    expectDecoded(pong, kPong);
    expectDecoded(join, kJoinGame);
    expectDecoded(arm, kArmTouch);
    expectDecoded(color, kSetColor);
    expectDecoded(stop, kStopAll);
    expectDecoded(simulate, kSimulateTouch);
    expectDecoded(touch, kTouchEvent);
    expectDecoded(timeout, kTimeoutEvent);
}

}  // namespace
