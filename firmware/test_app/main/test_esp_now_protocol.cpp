#include "services/espNowProtocol.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace {

using domes::espnow::ArmTouchMsg;
using domes::espnow::JoinGameMsg;
using domes::espnow::MsgHeader;
using domes::espnow::MsgType;
using domes::espnow::SetColorMsg;
using domes::espnow::SimulateTouchMsg;
using domes::espnow::StopAllMsg;
using domes::espnow::TimeoutEventMsg;
using domes::espnow::TouchEventMsg;

TEST(EspNowProtocolTest, DefinesExactWireSizeForEveryMessage) {
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kBeacon), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kPing), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kPong), sizeof(MsgHeader));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kJoinGame), sizeof(JoinGameMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kArmTouch), sizeof(ArmTouchMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kSetColor), sizeof(SetColorMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kStopAll), sizeof(StopAllMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kSimulateTouch),
              sizeof(SimulateTouchMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kTouchEvent), sizeof(TouchEventMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(MsgType::kTimeoutEvent), sizeof(TimeoutEventMsg));
    EXPECT_EQ(domes::espnow::expectedMessageSize(static_cast<MsgType>(0xFF)), 0U);
}

TEST(EspNowProtocolTest, OnlyBeaconPingAndPongAreDiscoveryTraffic) {
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(MsgType::kBeacon));
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(MsgType::kPing));
    EXPECT_TRUE(domes::espnow::isDiscoveryMessage(MsgType::kPong));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(MsgType::kJoinGame));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(MsgType::kArmTouch));
    EXPECT_FALSE(domes::espnow::isDiscoveryMessage(MsgType::kTouchEvent));
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

}  // namespace
