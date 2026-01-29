/**
 * @file configCommandHandler.cpp
 * @brief Config command handler implementation
 *
 * Uses nanopb for protobuf encoding/decoding of config messages.
 */

#include "configCommandHandler.hpp"
#include "protocol/frameCodec.hpp"
#include "services/imuService.hpp"
#include "services/ledService.hpp"

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
    auto msgType = static_cast<MsgType>(type);

    switch (msgType) {
        case MsgType::kListFeaturesReq:
            ESP_LOGD(kTag, "Received LIST_FEATURES");
            handleListFeatures();
            return true;

        case MsgType::kSetFeatureReq:
            ESP_LOGD(kTag, "Received SET_FEATURE");
            handleSetFeature(payload, len);
            return true;

        case MsgType::kGetFeatureReq:
            ESP_LOGD(kTag, "Received GET_FEATURE");
            handleGetFeature(payload, len);
            return true;

        case MsgType::kSetLedPatternReq:
            ESP_LOGD(kTag, "Received SET_LED_PATTERN");
            handleSetLedPattern(payload, len);
            return true;

        case MsgType::kGetLedPatternReq:
            ESP_LOGD(kTag, "Received GET_LED_PATTERN");
            handleGetLedPattern();
            return true;

        case MsgType::kSetImuTriageReq:
            ESP_LOGD(kTag, "Received SET_IMU_TRIAGE");
            handleSetImuTriage(payload, len);
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
        sendSetFeatureResponse(Status::kError, Feature::kUnknown, false);
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
        sendSetFeatureResponse(Status::kInvalidFeature, feature, false);
        return;
    }

    sendSetFeatureResponse(Status::kOk, feature, enabled);
}

void ConfigCommandHandler::handleGetFeature(const uint8_t* payload, size_t len) {
    // Decode using SetFeatureRequest (ignore enabled field for get)
    // TODO: Add GetFeatureRequest to proto if distinct message needed
    domes_config_SetFeatureRequest req = domes_config_SetFeatureRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetFeatureRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode GET_FEATURE: %s", PB_GET_ERROR(&stream));
        sendGetFeatureResponse(Status::kError, Feature::kUnknown, false);
        return;
    }

    auto feature = static_cast<Feature>(req.feature);

    // Check if feature is valid
    if (feature == Feature::kUnknown ||
        static_cast<uint8_t>(feature) >= static_cast<uint8_t>(Feature::kCount)) {
        ESP_LOGW(kTag, "Invalid feature ID: %d", static_cast<int>(req.feature));
        sendGetFeatureResponse(Status::kInvalidFeature, feature, false);
        return;
    }

    bool enabled = features_.isEnabled(feature);
    sendGetFeatureResponse(Status::kOk, feature, enabled);
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

    sendFrame(MsgType::kListFeaturesRsp, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::sendSetFeatureResponse(
    Status status,
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

    sendFrame(MsgType::kSetFeatureRsp, payload.data(), 1 + resp_stream.bytes_written);
}

void ConfigCommandHandler::sendGetFeatureResponse(
    Status status,
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

    sendFrame(MsgType::kGetFeatureRsp, payload.data(), 1 + stream.bytes_written);
}

bool ConfigCommandHandler::sendFrame(MsgType type, const uint8_t* payload, size_t len) {
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

void ConfigCommandHandler::handleSetLedPattern(const uint8_t* payload, size_t len) {
    if (!ledService_) {
        ESP_LOGW(kTag, "LED service not available");
        sendLedPatternResponse(Status::kError);
        return;
    }

    // Decode protobuf message
    domes_config_SetLedPatternRequest req = domes_config_SetLedPatternRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetLedPatternRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_LED_PATTERN: %s", PB_GET_ERROR(&stream));
        sendLedPatternResponse(Status::kError);
        return;
    }

    ESP_LOGI(kTag, "Setting LED pattern: type=%d, period=%lu, brightness=%lu",
             req.pattern.type, req.pattern.period_ms, req.pattern.brightness);

    esp_err_t err = ledService_->setPattern(req.pattern);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to set LED pattern: %s", esp_err_to_name(err));
        sendLedPatternResponse(Status::kInvalidPattern);
        return;
    }

    sendLedPatternResponse(Status::kOk);
}

void ConfigCommandHandler::handleGetLedPattern() {
    if (!ledService_) {
        ESP_LOGW(kTag, "LED service not available");
        sendLedPatternResponse(Status::kError);
        return;
    }

    // Build response with current pattern
    domes_config_GetLedPatternResponse resp = domes_config_GetLedPatternResponse_init_zero;
    resp.has_pattern = true;
    ledService_->getPattern(resp.pattern);

    // Encode to buffer: [status_byte][GetLedPatternResponse_proto]
    std::array<uint8_t, domes_config_GetLedPatternResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_GetLedPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetLedPatternResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kGetLedPatternRsp, payload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::sendLedPatternResponse(Status status) {
    // Build response with current pattern
    domes_config_SetLedPatternResponse resp = domes_config_SetLedPatternResponse_init_zero;

    if (ledService_ && status == Status::kOk) {
        resp.has_pattern = true;
        ledService_->getPattern(resp.pattern);
    }

    // Encode to buffer: [status_byte][SetLedPatternResponse_proto]
    std::array<uint8_t, domes_config_SetLedPatternResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetLedPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetLedPatternResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetLedPatternRsp, payload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleSetImuTriage(const uint8_t* payload, size_t len) {
    if (!imuService_) {
        ESP_LOGW(kTag, "IMU service not available");
        sendImuTriageResponse(Status::kError, false);
        return;
    }

    // Decode protobuf message
    domes_config_SetImuTriageRequest req = domes_config_SetImuTriageRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetImuTriageRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_IMU_TRIAGE: %s", PB_GET_ERROR(&stream));
        sendImuTriageResponse(Status::kError, false);
        return;
    }

    ESP_LOGI(kTag, "Setting IMU triage mode to %s", req.enabled ? "enabled" : "disabled");

    imuService_->setTriageMode(req.enabled);
    sendImuTriageResponse(Status::kOk, req.enabled);
}

void ConfigCommandHandler::sendImuTriageResponse(Status status, bool enabled) {
    // Build protobuf response
    domes_config_SetImuTriageResponse resp = domes_config_SetImuTriageResponse_init_zero;
    resp.enabled = enabled;

    // Encode to buffer: [status_byte][SetImuTriageResponse_proto]
    std::array<uint8_t, domes_config_SetImuTriageResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetImuTriageResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetImuTriageResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetImuTriageRsp, payload.data(), 1 + ostream.bytes_written);
}

}  // namespace domes::config
