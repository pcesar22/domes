/**
 * @file configCommandHandler.cpp
 * @brief Config command handler implementation
 *
 * Uses nanopb for protobuf encoding/decoding of config messages.
 */

#include "configCommandHandler.hpp"
#include "protocol/frameCodec.hpp"

#include "config.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#include "esp_log.h"

#include <array>
#include <cstring>

namespace {
constexpr const char* kTag = "config_cmd";
}

namespace domes::config {

ConfigCommandHandler::ConfigCommandHandler(ITransport& transport, FeatureManager& features,
                                           RgbPatternController* rgbController)
    : transport_(transport)
    , features_(features)
    , rgbController_(rgbController) {
}

bool ConfigCommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    auto msgType = static_cast<ConfigMsgType>(type);

    switch (msgType) {
        case ConfigMsgType::kListFeaturesReq:
            ESP_LOGD(kTag, "Received LIST_FEATURES");
            handleListFeatures();
            return true;

        case ConfigMsgType::kSetFeatureReq:
            ESP_LOGD(kTag, "Received SET_FEATURE");
            handleSetFeature(payload, len);
            return true;

        case ConfigMsgType::kGetFeatureReq:
            ESP_LOGD(kTag, "Received GET_FEATURE");
            handleGetFeature(payload, len);
            return true;

        // RGB Pattern commands
        case ConfigMsgType::kSetRgbPatternReq:
            ESP_LOGD(kTag, "Received SET_RGB_PATTERN");
            handleSetRgbPattern(payload, len);
            return true;

        case ConfigMsgType::kGetRgbPatternReq:
            ESP_LOGD(kTag, "Received GET_RGB_PATTERN");
            handleGetRgbPattern();
            return true;

        case ConfigMsgType::kListRgbPatternsReq:
            ESP_LOGD(kTag, "Received LIST_RGB_PATTERNS");
            handleListRgbPatterns();
            return true;

        default:
            ESP_LOGW(kTag, "Unknown config command: 0x%02X", type);
            return false;
    }
}

void ConfigCommandHandler::handleListFeatures() {
    sendListFeaturesResponse();
}

void ConfigCommandHandler::handleSetFeature(const uint8_t* payload, size_t len) {
    // Decode protobuf message
    domes_config_SetFeatureRequest req = domes_config_SetFeatureRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetFeatureRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_FEATURE: %s", PB_GET_ERROR(&stream));
        sendSetFeatureResponse(ConfigStatus::kError, Feature::kUnknown, false);
        return;
    }

    auto feature = static_cast<Feature>(req.feature);
    bool enabled = req.enabled;

    ESP_LOGI(kTag, "Setting feature %s (%d) to %s",
             featureToString(feature),
             static_cast<int>(req.feature),
             enabled ? "enabled" : "disabled");

    if (!features_.setEnabled(feature, enabled)) {
        ESP_LOGW(kTag, "Invalid feature ID: %d", static_cast<int>(req.feature));
        sendSetFeatureResponse(ConfigStatus::kInvalidFeature, feature, false);
        return;
    }

    sendSetFeatureResponse(ConfigStatus::kOk, feature, enabled);
}

void ConfigCommandHandler::handleGetFeature(const uint8_t* payload, size_t len) {
    // Decode using SetFeatureRequest (ignore enabled field for get)
    // TODO: Add GetFeatureRequest to proto if distinct message needed
    domes_config_SetFeatureRequest req = domes_config_SetFeatureRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetFeatureRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode GET_FEATURE: %s", PB_GET_ERROR(&stream));
        sendGetFeatureResponse(ConfigStatus::kError, Feature::kUnknown, false);
        return;
    }

    auto feature = static_cast<Feature>(req.feature);

    // Check if feature is valid
    if (feature == Feature::kUnknown ||
        static_cast<uint8_t>(feature) >= static_cast<uint8_t>(Feature::kCount)) {
        ESP_LOGW(kTag, "Invalid feature ID: %d", static_cast<int>(req.feature));
        sendGetFeatureResponse(ConfigStatus::kInvalidFeature, feature, false);
        return;
    }

    bool enabled = features_.isEnabled(feature);
    sendGetFeatureResponse(ConfigStatus::kOk, feature, enabled);
}

void ConfigCommandHandler::sendListFeaturesResponse() {
    // Build protobuf response
    domes_config_ListFeaturesResponse resp = domes_config_ListFeaturesResponse_init_zero;

    // Get all feature states
    for (uint8_t i = 1; i < static_cast<uint8_t>(Feature::kCount); ++i) {
        auto feature = static_cast<Feature>(i);
        resp.features[resp.features_count].feature = static_cast<domes_config_Feature>(i);
        resp.features[resp.features_count].enabled = features_.isEnabled(feature);
        resp.features_count++;
    }

    // Encode to buffer
    std::array<uint8_t, domes_config_ListFeaturesResponse_size + 10> payload;
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());

    if (!pb_encode(&stream, domes_config_ListFeaturesResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode ListFeaturesResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(ConfigMsgType::kListFeaturesRsp, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::sendSetFeatureResponse(
    ConfigStatus status,
    Feature feature,
    bool enabled
) {
    // Build protobuf response
    domes_config_SetFeatureResponse resp = domes_config_SetFeatureResponse_init_zero;
    resp.has_feature = true;
    resp.feature.feature = static_cast<domes_config_Feature>(feature);
    resp.feature.enabled = enabled;

    // Encode to buffer: [status_byte][SetFeatureResponse_proto]
    std::array<uint8_t, domes_config_SetFeatureResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t resp_stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&resp_stream, domes_config_SetFeatureResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetFeatureResponse: %s", PB_GET_ERROR(&resp_stream));
        return;
    }

    sendFrame(ConfigMsgType::kSetFeatureRsp, payload.data(), 1 + resp_stream.bytes_written);
}

void ConfigCommandHandler::sendGetFeatureResponse(
    ConfigStatus status,
    Feature feature,
    bool enabled
) {
    // Use same format as SetFeatureResponse: [status][SetFeatureResponse_proto]
    domes_config_SetFeatureResponse resp = domes_config_SetFeatureResponse_init_zero;
    resp.has_feature = true;
    resp.feature.feature = static_cast<domes_config_Feature>(feature);
    resp.feature.enabled = enabled;

    std::array<uint8_t, domes_config_SetFeatureResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_SetFeatureResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetFeatureResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(ConfigMsgType::kGetFeatureRsp, payload.data(), 1 + stream.bytes_written);
}

bool ConfigCommandHandler::sendFrame(ConfigMsgType type, const uint8_t* payload, size_t len) {
    std::array<uint8_t, kMaxFrameSize> frameBuf;
    size_t frameLen = 0;

    TransportError err = encodeFrame(
        static_cast<uint8_t>(type),
        payload,
        len,
        frameBuf.data(),
        frameBuf.size(),
        &frameLen
    );

    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to encode frame");
        return false;
    }

    err = transport_.send(frameBuf.data(), frameLen);
    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to send frame: %s", transportErrorToString(err));
        return false;
    }

    return true;
}

// =============================================================================
// RGB Pattern Handlers
// =============================================================================

void ConfigCommandHandler::handleSetRgbPattern(const uint8_t* payload, size_t len) {
    if (!rgbController_) {
        ESP_LOGW(kTag, "RGB controller not available");
        sendSetRgbPatternResponse(RgbPattern::kOff);
        return;
    }

    // Decode protobuf message
    domes_config_SetRgbPatternRequest req = domes_config_SetRgbPatternRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetRgbPatternRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_RGB_PATTERN: %s", PB_GET_ERROR(&stream));
        sendSetRgbPatternResponse(RgbPattern::kOff);
        return;
    }

    // Build pattern config
    RgbPatternConfig config;
    config.pattern = static_cast<RgbPattern>(req.pattern);

    // Set primary color if provided
    if (req.has_primary_color) {
        config.primaryColor = Color::rgb(
            static_cast<uint8_t>(req.primary_color.r),
            static_cast<uint8_t>(req.primary_color.g),
            static_cast<uint8_t>(req.primary_color.b)
        );
    }

    // Set speed if provided (default to 50ms if 0)
    config.speedMs = (req.speed_ms > 0) ? req.speed_ms : 50;

    // Set brightness if provided (default to 128 if 0)
    config.brightness = (req.brightness > 0) ? static_cast<uint8_t>(req.brightness) : 128;

    ESP_LOGI(kTag, "Setting RGB pattern: %s (speed=%lu ms, brightness=%u)",
             rgbPatternToString(config.pattern),
             static_cast<unsigned long>(config.speedMs),
             config.brightness);

    rgbController_->setConfig(config);
    sendSetRgbPatternResponse(config.pattern);
}

void ConfigCommandHandler::handleGetRgbPattern() {
    sendGetRgbPatternResponse();
}

void ConfigCommandHandler::handleListRgbPatterns() {
    sendListRgbPatternsResponse();
}

void ConfigCommandHandler::sendSetRgbPatternResponse(RgbPattern pattern) {
    domes_config_SetRgbPatternResponse resp = domes_config_SetRgbPatternResponse_init_zero;
    resp.active_pattern = static_cast<domes_config_RgbPattern>(pattern);

    std::array<uint8_t, domes_config_SetRgbPatternResponse_size + 10> payload;
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());

    if (!pb_encode(&stream, domes_config_SetRgbPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetRgbPatternResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(ConfigMsgType::kSetRgbPatternRsp, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::sendGetRgbPatternResponse() {
    domes_config_GetRgbPatternResponse resp = domes_config_GetRgbPatternResponse_init_zero;

    if (rgbController_) {
        const auto& config = rgbController_->getConfig();
        resp.active_pattern = static_cast<domes_config_RgbPattern>(config.pattern);
        resp.has_primary_color = true;
        resp.primary_color.r = config.primaryColor.r;
        resp.primary_color.g = config.primaryColor.g;
        resp.primary_color.b = config.primaryColor.b;
        resp.speed_ms = config.speedMs;
        resp.brightness = config.brightness;
    } else {
        resp.active_pattern = domes_config_RgbPattern_RGB_PATTERN_OFF;
    }

    std::array<uint8_t, domes_config_GetRgbPatternResponse_size + 10> payload;
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());

    if (!pb_encode(&stream, domes_config_GetRgbPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetRgbPatternResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(ConfigMsgType::kGetRgbPatternRsp, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::sendListRgbPatternsResponse() {
    domes_config_ListRgbPatternsResponse resp = domes_config_ListRgbPatternsResponse_init_zero;

    // Add all available patterns
    for (uint8_t i = 0; i < static_cast<uint8_t>(RgbPattern::kCount); ++i) {
        auto pattern = static_cast<RgbPattern>(i);
        auto& info = resp.patterns[resp.patterns_count];

        info.pattern = static_cast<domes_config_RgbPattern>(pattern);

        // Copy pattern name
        const char* name = rgbPatternToString(pattern);
        strncpy(info.name, name, sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';

        // Copy pattern description
        const char* desc = rgbPatternDescription(pattern);
        strncpy(info.description, desc, sizeof(info.description) - 1);
        info.description[sizeof(info.description) - 1] = '\0';

        resp.patterns_count++;
    }

    std::array<uint8_t, domes_config_ListRgbPatternsResponse_size + 10> payload;
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());

    if (!pb_encode(&stream, domes_config_ListRgbPatternsResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode ListRgbPatternsResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(ConfigMsgType::kListRgbPatternsRsp, payload.data(), stream.bytes_written);
}

}  // namespace domes::config
