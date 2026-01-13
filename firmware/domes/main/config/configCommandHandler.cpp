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

ConfigCommandHandler::ConfigCommandHandler(ITransport& transport, FeatureManager& features)
    : transport_(transport)
    , features_(features) {
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

}  // namespace domes::config
