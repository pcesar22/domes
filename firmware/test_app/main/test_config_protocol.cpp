/**
 * @file test_config_protocol.cpp
 * @brief Unit tests for config protocol serialization/deserialization
 */

#include <gtest/gtest.h>
#include "config/configProtocol.hpp"

#include <array>
#include <cstring>

using namespace domes::config;

// =============================================================================
// ConfigMsgType Tests
// =============================================================================

TEST(ConfigMsgType, IsConfigMessageInRange) {
    // All config message types should be recognized
    EXPECT_TRUE(isConfigMessage(0x20));  // ListFeaturesReq
    EXPECT_TRUE(isConfigMessage(0x21));  // ListFeaturesRsp
    EXPECT_TRUE(isConfigMessage(0x22));  // SetFeatureReq
    EXPECT_TRUE(isConfigMessage(0x23));  // SetFeatureRsp
    EXPECT_TRUE(isConfigMessage(0x24));  // GetFeatureReq
    EXPECT_TRUE(isConfigMessage(0x25));  // GetFeatureRsp
}

TEST(ConfigMsgType, IsConfigMessageOutOfRange) {
    // OTA and trace ranges should not be config messages
    EXPECT_FALSE(isConfigMessage(0x01));  // OTA_BEGIN
    EXPECT_FALSE(isConfigMessage(0x05));  // OTA_ABORT
    EXPECT_FALSE(isConfigMessage(0x10));  // TRACE_START
    EXPECT_FALSE(isConfigMessage(0x17));  // TRACE_ACK
    EXPECT_FALSE(isConfigMessage(0x00));  // Unknown
    EXPECT_FALSE(isConfigMessage(0xFF));  // Unknown
}

// =============================================================================
// Feature Tests
// =============================================================================

TEST(Feature, ValidFeatureIds) {
    EXPECT_EQ(static_cast<uint8_t>(Feature::kUnknown), 0);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kLedEffects), 1);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kBleAdvertising), 2);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kWifi), 3);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kEspNow), 4);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kTouch), 5);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kHaptic), 6);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kAudio), 7);
    EXPECT_EQ(static_cast<uint8_t>(Feature::kCount), 8);
}

TEST(Feature, FeatureToString) {
    EXPECT_STREQ(featureToString(Feature::kLedEffects), "led-effects");
    EXPECT_STREQ(featureToString(Feature::kBleAdvertising), "ble");
    EXPECT_STREQ(featureToString(Feature::kWifi), "wifi");
    EXPECT_STREQ(featureToString(Feature::kEspNow), "esp-now");
    EXPECT_STREQ(featureToString(Feature::kTouch), "touch");
    EXPECT_STREQ(featureToString(Feature::kHaptic), "haptic");
    EXPECT_STREQ(featureToString(Feature::kAudio), "audio");
    EXPECT_STREQ(featureToString(Feature::kUnknown), "unknown");
}

// =============================================================================
// ConfigStatus Tests
// =============================================================================

TEST(ConfigStatus, StatusValues) {
    EXPECT_EQ(static_cast<uint8_t>(ConfigStatus::kOk), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(ConfigStatus::kError), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(ConfigStatus::kInvalidFeature), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(ConfigStatus::kBusy), 0x03);
}

TEST(ConfigStatus, StatusToString) {
    EXPECT_STREQ(configStatusToString(ConfigStatus::kOk), "ok");
    EXPECT_STREQ(configStatusToString(ConfigStatus::kError), "error");
    EXPECT_STREQ(configStatusToString(ConfigStatus::kInvalidFeature), "invalid-feature");
    EXPECT_STREQ(configStatusToString(ConfigStatus::kBusy), "busy");
}

// =============================================================================
// Payload Structure Tests
// =============================================================================

TEST(FeatureState, StructSize) {
    EXPECT_EQ(sizeof(FeatureState), 2u);
}

TEST(ListFeaturesResponse, StructSize) {
    EXPECT_EQ(sizeof(ListFeaturesResponse), 2u);
}

TEST(SetFeatureRequest, StructSize) {
    EXPECT_EQ(sizeof(SetFeatureRequest), 2u);
}

TEST(SetFeatureResponse, StructSize) {
    EXPECT_EQ(sizeof(SetFeatureResponse), 3u);
}

TEST(GetFeatureRequest, StructSize) {
    EXPECT_EQ(sizeof(GetFeatureRequest), 1u);
}

TEST(GetFeatureResponse, StructSize) {
    EXPECT_EQ(sizeof(GetFeatureResponse), 3u);
}

// =============================================================================
// Serialization Round-trip Tests
// =============================================================================

TEST(SetFeatureRequest, SerializationRoundTrip) {
    SetFeatureRequest req;
    req.feature = static_cast<uint8_t>(Feature::kLedEffects);
    req.enabled = 1;

    // Serialize (just casting to bytes)
    const auto* bytes = reinterpret_cast<const uint8_t*>(&req);

    // Deserialize
    const auto* parsed = reinterpret_cast<const SetFeatureRequest*>(bytes);

    EXPECT_EQ(parsed->feature, static_cast<uint8_t>(Feature::kLedEffects));
    EXPECT_EQ(parsed->enabled, 1);
}

TEST(SetFeatureResponse, SerializationRoundTrip) {
    SetFeatureResponse resp;
    resp.status = static_cast<uint8_t>(ConfigStatus::kOk);
    resp.feature = static_cast<uint8_t>(Feature::kBleAdvertising);
    resp.enabled = 0;

    // Serialize
    const auto* bytes = reinterpret_cast<const uint8_t*>(&resp);

    // Deserialize
    const auto* parsed = reinterpret_cast<const SetFeatureResponse*>(bytes);

    EXPECT_EQ(parsed->status, static_cast<uint8_t>(ConfigStatus::kOk));
    EXPECT_EQ(parsed->feature, static_cast<uint8_t>(Feature::kBleAdvertising));
    EXPECT_EQ(parsed->enabled, 0);
}

TEST(ListFeaturesResponse, MultipleFeatures) {
    // Simulate a response with multiple features
    std::array<uint8_t, 16> buffer{};

    // Header
    auto* header = reinterpret_cast<ListFeaturesResponse*>(buffer.data());
    header->status = static_cast<uint8_t>(ConfigStatus::kOk);
    header->count = 3;

    // Feature states
    auto* states = reinterpret_cast<FeatureState*>(buffer.data() + sizeof(ListFeaturesResponse));
    states[0].feature = static_cast<uint8_t>(Feature::kLedEffects);
    states[0].enabled = 1;
    states[1].feature = static_cast<uint8_t>(Feature::kBleAdvertising);
    states[1].enabled = 0;
    states[2].feature = static_cast<uint8_t>(Feature::kWifi);
    states[2].enabled = 1;

    // Parse back
    const auto* parsedHeader = reinterpret_cast<const ListFeaturesResponse*>(buffer.data());
    const auto* parsedStates = reinterpret_cast<const FeatureState*>(buffer.data() + sizeof(ListFeaturesResponse));

    EXPECT_EQ(parsedHeader->status, static_cast<uint8_t>(ConfigStatus::kOk));
    EXPECT_EQ(parsedHeader->count, 3);

    EXPECT_EQ(parsedStates[0].feature, static_cast<uint8_t>(Feature::kLedEffects));
    EXPECT_EQ(parsedStates[0].enabled, 1);
    EXPECT_EQ(parsedStates[1].feature, static_cast<uint8_t>(Feature::kBleAdvertising));
    EXPECT_EQ(parsedStates[1].enabled, 0);
    EXPECT_EQ(parsedStates[2].feature, static_cast<uint8_t>(Feature::kWifi));
    EXPECT_EQ(parsedStates[2].enabled, 1);
}

// =============================================================================
// Constants Tests
// =============================================================================

TEST(Constants, MaxFeatures) {
    EXPECT_EQ(kMaxFeatures, static_cast<size_t>(Feature::kCount));
}

TEST(Constants, MaxListFeaturesPayload) {
    size_t expected = sizeof(ListFeaturesResponse) + (kMaxFeatures * sizeof(FeatureState));
    EXPECT_EQ(kMaxListFeaturesPayload, expected);
}
