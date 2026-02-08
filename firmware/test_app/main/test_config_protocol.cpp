/**
 * @file test_config_protocol.cpp
 * @brief Unit tests for config protocol serialization/deserialization
 *
 * Tests protobuf encoding/decoding using nanopb.
 */

#include <gtest/gtest.h>
#include "config/configProtocol.hpp"
#include "config.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

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

TEST(ConfigMsgType, IsConfigMessageSystemModeRange) {
    // System mode commands (0x30-0x35) should be config messages
    EXPECT_TRUE(isConfigMessage(0x30));  // GetModeReq
    EXPECT_TRUE(isConfigMessage(0x31));  // GetModeRsp
    EXPECT_TRUE(isConfigMessage(0x32));  // SetModeReq
    EXPECT_TRUE(isConfigMessage(0x33));  // SetModeRsp
    EXPECT_TRUE(isConfigMessage(0x34));  // GetSystemInfoReq
    EXPECT_TRUE(isConfigMessage(0x35));  // GetSystemInfoRsp
}

TEST(ConfigMsgType, IsConfigMessagePodIdRange) {
    // Pod ID commands (0x36-0x37) should be config messages
    EXPECT_TRUE(isConfigMessage(0x36));  // SetPodIdReq
    EXPECT_TRUE(isConfigMessage(0x37));  // SetPodIdRsp
}

TEST(ConfigMsgType, IsConfigMessageOutOfRange) {
    // OTA and trace ranges should not be config messages
    EXPECT_FALSE(isConfigMessage(0x01));  // OTA_BEGIN
    EXPECT_FALSE(isConfigMessage(0x05));  // OTA_ABORT
    EXPECT_FALSE(isConfigMessage(0x10));  // TRACE_START
    EXPECT_FALSE(isConfigMessage(0x17));  // TRACE_ACK
    EXPECT_FALSE(isConfigMessage(0x00));  // Unknown
    EXPECT_FALSE(isConfigMessage(0xFF));  // Unknown
    EXPECT_FALSE(isConfigMessage(0x1F));  // Just before config range
    EXPECT_FALSE(isConfigMessage(0x38));  // Just past pod ID range
}

TEST(ConfigMsgType, GapValuesAreInRange) {
    // Gap values 0x2C-0x2F fall within the simple range check.
    // They're routed to the config handler but safely ignored by the switch.
    EXPECT_TRUE(isConfigMessage(0x2C));
    EXPECT_TRUE(isConfigMessage(0x2F));
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
// Status Tests
// =============================================================================

TEST(Status, StatusValues) {
    EXPECT_EQ(static_cast<uint8_t>(Status::kOk), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(Status::kError), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(Status::kInvalidFeature), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(Status::kBusy), 0x03);
}

TEST(Status, StatusToString) {
    EXPECT_STREQ(statusToString(Status::kOk), "ok");
    EXPECT_STREQ(statusToString(Status::kError), "error");
    EXPECT_STREQ(statusToString(Status::kInvalidFeature), "invalid-feature");
    EXPECT_STREQ(statusToString(Status::kBusy), "busy");
}

// =============================================================================
// Protobuf Serialization Tests
// =============================================================================

TEST(Protobuf, SetFeatureRequestEncodeDecode) {
    // Create a SetFeatureRequest
    domes_config_SetFeatureRequest req = domes_config_SetFeatureRequest_init_zero;
    req.feature = domes_config_Feature_FEATURE_LED_EFFECTS;
    req.enabled = true;

    // Encode
    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_SetFeatureRequest_fields, &req));
    EXPECT_GT(ostream.bytes_written, 0u);

    // Decode
    domes_config_SetFeatureRequest decoded = domes_config_SetFeatureRequest_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_SetFeatureRequest_fields, &decoded));

    EXPECT_EQ(decoded.feature, domes_config_Feature_FEATURE_LED_EFFECTS);
    EXPECT_TRUE(decoded.enabled);
}

TEST(Protobuf, SetFeatureResponseEncodeDecode) {
    // Create a SetFeatureResponse with a FeatureState
    domes_config_SetFeatureResponse resp = domes_config_SetFeatureResponse_init_zero;
    resp.has_feature = true;
    resp.feature.feature = domes_config_Feature_FEATURE_WIFI;
    resp.feature.enabled = false;

    // Encode
    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_SetFeatureResponse_fields, &resp));
    EXPECT_GT(ostream.bytes_written, 0u);

    // Decode
    domes_config_SetFeatureResponse decoded = domes_config_SetFeatureResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_SetFeatureResponse_fields, &decoded));

    EXPECT_TRUE(decoded.has_feature);
    EXPECT_EQ(decoded.feature.feature, domes_config_Feature_FEATURE_WIFI);
    EXPECT_FALSE(decoded.feature.enabled);
}

TEST(Protobuf, ListFeaturesResponseEncodeDecode) {
    // Create a ListFeaturesResponse with multiple features
    domes_config_ListFeaturesResponse resp = domes_config_ListFeaturesResponse_init_zero;
    resp.features_count = 3;
    resp.features[0].feature = domes_config_Feature_FEATURE_LED_EFFECTS;
    resp.features[0].enabled = true;
    resp.features[1].feature = domes_config_Feature_FEATURE_BLE_ADVERTISING;
    resp.features[1].enabled = false;
    resp.features[2].feature = domes_config_Feature_FEATURE_WIFI;
    resp.features[2].enabled = true;

    // Encode
    std::array<uint8_t, 128> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_ListFeaturesResponse_fields, &resp));
    EXPECT_GT(ostream.bytes_written, 0u);

    // Decode
    domes_config_ListFeaturesResponse decoded = domes_config_ListFeaturesResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_ListFeaturesResponse_fields, &decoded));

    EXPECT_EQ(decoded.features_count, 3u);
    EXPECT_EQ(decoded.features[0].feature, domes_config_Feature_FEATURE_LED_EFFECTS);
    EXPECT_TRUE(decoded.features[0].enabled);
    EXPECT_EQ(decoded.features[1].feature, domes_config_Feature_FEATURE_BLE_ADVERTISING);
    EXPECT_FALSE(decoded.features[1].enabled);
    EXPECT_EQ(decoded.features[2].feature, domes_config_Feature_FEATURE_WIFI);
    EXPECT_TRUE(decoded.features[2].enabled);
}

TEST(Protobuf, EmptyListFeaturesResponse) {
    // Empty response
    domes_config_ListFeaturesResponse resp = domes_config_ListFeaturesResponse_init_zero;
    resp.features_count = 0;

    // Encode
    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_ListFeaturesResponse_fields, &resp));

    // Decode
    domes_config_ListFeaturesResponse decoded = domes_config_ListFeaturesResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_ListFeaturesResponse_fields, &decoded));

    EXPECT_EQ(decoded.features_count, 0u);
}

// =============================================================================
// Constants Tests
// =============================================================================

TEST(Constants, MaxFeatures) {
    EXPECT_EQ(kMaxFeatures, static_cast<size_t>(Feature::kCount));
}

TEST(Constants, MaxFrameSize) {
    EXPECT_EQ(kMaxFrameSize, 256u);
}

// =============================================================================
// System Mode Protobuf Tests
// =============================================================================

TEST(Protobuf, GetModeResponseEncodeDecode) {
    domes_config_GetModeResponse resp = domes_config_GetModeResponse_init_zero;
    resp.mode = domes_config_SystemMode_SYSTEM_MODE_TRIAGE;
    resp.time_in_mode_ms = 12345;

    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_GetModeResponse_fields, &resp));
    EXPECT_GT(ostream.bytes_written, 0u);

    domes_config_GetModeResponse decoded = domes_config_GetModeResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_GetModeResponse_fields, &decoded));

    EXPECT_EQ(decoded.mode, domes_config_SystemMode_SYSTEM_MODE_TRIAGE);
    EXPECT_EQ(decoded.time_in_mode_ms, 12345u);
}

TEST(Protobuf, SetModeRequestEncodeDecode) {
    domes_config_SetModeRequest req = domes_config_SetModeRequest_init_zero;
    req.mode = domes_config_SystemMode_SYSTEM_MODE_CONNECTED;

    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_SetModeRequest_fields, &req));
    EXPECT_GT(ostream.bytes_written, 0u);

    domes_config_SetModeRequest decoded = domes_config_SetModeRequest_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_SetModeRequest_fields, &decoded));

    EXPECT_EQ(decoded.mode, domes_config_SystemMode_SYSTEM_MODE_CONNECTED);
}

TEST(Protobuf, SetModeResponseEncodeDecode) {
    domes_config_SetModeResponse resp = domes_config_SetModeResponse_init_zero;
    resp.mode = domes_config_SystemMode_SYSTEM_MODE_GAME;
    resp.transition_ok = true;

    std::array<uint8_t, 64> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_SetModeResponse_fields, &resp));
    EXPECT_GT(ostream.bytes_written, 0u);

    domes_config_SetModeResponse decoded = domes_config_SetModeResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_SetModeResponse_fields, &decoded));

    EXPECT_EQ(decoded.mode, domes_config_SystemMode_SYSTEM_MODE_GAME);
    EXPECT_TRUE(decoded.transition_ok);
}

TEST(Protobuf, GetSystemInfoResponseEncodeDecode) {
    domes_config_GetSystemInfoResponse resp = domes_config_GetSystemInfoResponse_init_zero;
    strncpy(resp.firmware_version, "v1.2.3", sizeof(resp.firmware_version) - 1);
    resp.uptime_s = 3600;
    resp.free_heap = 65536;
    resp.boot_count = 42;
    resp.mode = domes_config_SystemMode_SYSTEM_MODE_IDLE;
    resp.feature_mask = 0x000000EE;

    std::array<uint8_t, 128> buffer{};
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    ASSERT_TRUE(pb_encode(&ostream, domes_config_GetSystemInfoResponse_fields, &resp));
    EXPECT_GT(ostream.bytes_written, 0u);

    domes_config_GetSystemInfoResponse decoded = domes_config_GetSystemInfoResponse_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer.data(), ostream.bytes_written);
    ASSERT_TRUE(pb_decode(&istream, domes_config_GetSystemInfoResponse_fields, &decoded));

    EXPECT_STREQ(decoded.firmware_version, "v1.2.3");
    EXPECT_EQ(decoded.uptime_s, 3600u);
    EXPECT_EQ(decoded.free_heap, 65536u);
    EXPECT_EQ(decoded.boot_count, 42u);
    EXPECT_EQ(decoded.mode, domes_config_SystemMode_SYSTEM_MODE_IDLE);
    EXPECT_EQ(decoded.feature_mask, 0x000000EEu);
}
