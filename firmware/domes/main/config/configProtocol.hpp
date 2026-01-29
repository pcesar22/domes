#pragma once

/**
 * @file configProtocol.hpp
 * @brief Wire protocol definitions for runtime configuration commands
 *
 * ALL TYPE DEFINITIONS ARE SOURCED FROM config.proto via nanopb-generated config.pb.h.
 * This file provides C++ enum class wrappers for type safety only.
 * DO NOT add new message types or enums here - add them to config.proto instead.
 */

#include <cstdint>
#include <cstddef>

// Include the nanopb-generated protobuf definitions (source of truth)
#include "config.pb.h"

namespace domes::config {

/**
 * @brief Config protocol message types (sourced from config.proto)
 */
enum class MsgType : uint8_t {
    kUnknown         = domes_config_MsgType_MSG_TYPE_UNKNOWN,
    kListFeaturesReq = domes_config_MsgType_MSG_TYPE_LIST_FEATURES_REQ,
    kListFeaturesRsp = domes_config_MsgType_MSG_TYPE_LIST_FEATURES_RSP,
    kSetFeatureReq   = domes_config_MsgType_MSG_TYPE_SET_FEATURE_REQ,
    kSetFeatureRsp   = domes_config_MsgType_MSG_TYPE_SET_FEATURE_RSP,
    kGetFeatureReq   = domes_config_MsgType_MSG_TYPE_GET_FEATURE_REQ,
    kGetFeatureRsp   = domes_config_MsgType_MSG_TYPE_GET_FEATURE_RSP,
    kSetLedPatternReq = domes_config_MsgType_MSG_TYPE_SET_LED_PATTERN_REQ,
    kSetLedPatternRsp = domes_config_MsgType_MSG_TYPE_SET_LED_PATTERN_RSP,
    kGetLedPatternReq = domes_config_MsgType_MSG_TYPE_GET_LED_PATTERN_REQ,
    kGetLedPatternRsp = domes_config_MsgType_MSG_TYPE_GET_LED_PATTERN_RSP,
    kSetImuTriageReq  = domes_config_MsgType_MSG_TYPE_SET_IMU_TRIAGE_REQ,
    kSetImuTriageRsp  = domes_config_MsgType_MSG_TYPE_SET_IMU_TRIAGE_RSP,
};

/**
 * @brief Runtime-toggleable features (sourced from config.proto)
 */
enum class Feature : uint8_t {
    kUnknown        = domes_config_Feature_FEATURE_UNKNOWN,
    kLedEffects     = domes_config_Feature_FEATURE_LED_EFFECTS,
    kBleAdvertising = domes_config_Feature_FEATURE_BLE_ADVERTISING,
    kWifi           = domes_config_Feature_FEATURE_WIFI,
    kEspNow         = domes_config_Feature_FEATURE_ESP_NOW,
    kTouch          = domes_config_Feature_FEATURE_TOUCH,
    kHaptic         = domes_config_Feature_FEATURE_HAPTIC,
    kAudio          = domes_config_Feature_FEATURE_AUDIO,
    kCount          = _domes_config_Feature_ARRAYSIZE,
};

/**
 * @brief Config command status codes (sourced from config.proto)
 */
enum class Status : uint8_t {
    kOk             = domes_config_Status_STATUS_OK,
    kError          = domes_config_Status_STATUS_ERROR,
    kInvalidFeature = domes_config_Status_STATUS_INVALID_FEATURE,
    kBusy           = domes_config_Status_STATUS_BUSY,
    kInvalidPattern = domes_config_Status_STATUS_INVALID_PATTERN,
};

/**
 * @brief Check if a message type is a config command (0x20-0x2F range)
 */
inline bool isConfigMessage(uint8_t type) {
    return type >= static_cast<uint8_t>(MsgType::kListFeaturesReq) &&
           type <= static_cast<uint8_t>(MsgType::kSetImuTriageRsp);
}

/**
 * @brief Get human-readable name for a feature
 */
inline const char* featureToString(Feature feature) {
    switch (feature) {
        case Feature::kLedEffects:     return "led-effects";
        case Feature::kBleAdvertising: return "ble";
        case Feature::kWifi:           return "wifi";
        case Feature::kEspNow:         return "esp-now";
        case Feature::kTouch:          return "touch";
        case Feature::kHaptic:         return "haptic";
        case Feature::kAudio:          return "audio";
        default:                       return "unknown";
    }
}

/**
 * @brief Get human-readable name for a config status
 */
inline const char* statusToString(Status status) {
    switch (status) {
        case Status::kOk:             return "ok";
        case Status::kError:          return "error";
        case Status::kInvalidFeature: return "invalid-feature";
        case Status::kBusy:           return "busy";
        case Status::kInvalidPattern: return "invalid-pattern";
        default:                      return "unknown";
    }
}

/// Maximum features supported
constexpr size_t kMaxFeatures = static_cast<size_t>(Feature::kCount);

/// Maximum frame size for config messages
constexpr size_t kMaxFrameSize = 256;

}  // namespace domes::config
