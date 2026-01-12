/**
 * @file configCommandHandler.cpp
 * @brief Config command handler implementation
 */

#include "configCommandHandler.hpp"
#include "protocol/frameCodec.hpp"

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
    if (len < sizeof(SetFeatureRequest)) {
        ESP_LOGW(kTag, "SET_FEATURE payload too short: %zu", len);
        sendSetFeatureResponse(ConfigStatus::kError, Feature::kUnknown, false);
        return;
    }

    const auto* req = reinterpret_cast<const SetFeatureRequest*>(payload);
    auto feature = static_cast<Feature>(req->feature);
    bool enabled = req->enabled != 0;

    ESP_LOGI(kTag, "Setting feature %s (%d) to %s",
             featureToString(feature),
             req->feature,
             enabled ? "enabled" : "disabled");

    if (!features_.setEnabled(feature, enabled)) {
        ESP_LOGW(kTag, "Invalid feature ID: %d", req->feature);
        sendSetFeatureResponse(ConfigStatus::kInvalidFeature, feature, false);
        return;
    }

    sendSetFeatureResponse(ConfigStatus::kOk, feature, enabled);
}

void ConfigCommandHandler::handleGetFeature(const uint8_t* payload, size_t len) {
    if (len < sizeof(GetFeatureRequest)) {
        ESP_LOGW(kTag, "GET_FEATURE payload too short: %zu", len);
        sendGetFeatureResponse(ConfigStatus::kError, Feature::kUnknown, false);
        return;
    }

    const auto* req = reinterpret_cast<const GetFeatureRequest*>(payload);
    auto feature = static_cast<Feature>(req->feature);

    // Check if feature is valid
    if (feature == Feature::kUnknown ||
        static_cast<uint8_t>(feature) >= static_cast<uint8_t>(Feature::kCount)) {
        ESP_LOGW(kTag, "Invalid feature ID: %d", req->feature);
        sendGetFeatureResponse(ConfigStatus::kInvalidFeature, feature, false);
        return;
    }

    bool enabled = features_.isEnabled(feature);
    sendGetFeatureResponse(ConfigStatus::kOk, feature, enabled);
}

void ConfigCommandHandler::sendListFeaturesResponse() {
    // Build response with all feature states
    std::array<uint8_t, kMaxListFeaturesPayload> payload;

    auto* header = reinterpret_cast<ListFeaturesResponse*>(payload.data());
    auto* states = reinterpret_cast<FeatureState*>(payload.data() + sizeof(ListFeaturesResponse));

    size_t count = features_.getAll(states);

    header->status = static_cast<uint8_t>(ConfigStatus::kOk);
    header->count = static_cast<uint8_t>(count);

    size_t payloadLen = sizeof(ListFeaturesResponse) + count * sizeof(FeatureState);

    sendFrame(ConfigMsgType::kListFeaturesRsp, payload.data(), payloadLen);
}

void ConfigCommandHandler::sendSetFeatureResponse(
    ConfigStatus status,
    Feature feature,
    bool enabled
) {
    SetFeatureResponse resp;
    resp.status = static_cast<uint8_t>(status);
    resp.feature = static_cast<uint8_t>(feature);
    resp.enabled = enabled ? 1 : 0;

    sendFrame(ConfigMsgType::kSetFeatureRsp,
              reinterpret_cast<const uint8_t*>(&resp),
              sizeof(resp));
}

void ConfigCommandHandler::sendGetFeatureResponse(
    ConfigStatus status,
    Feature feature,
    bool enabled
) {
    GetFeatureResponse resp;
    resp.status = static_cast<uint8_t>(status);
    resp.feature = static_cast<uint8_t>(feature);
    resp.enabled = enabled ? 1 : 0;

    sendFrame(ConfigMsgType::kGetFeatureRsp,
              reinterpret_cast<const uint8_t*>(&resp),
              sizeof(resp));
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
