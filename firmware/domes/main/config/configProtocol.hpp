#pragma once

/**
 * @file configProtocol.hpp
 * @brief Wire protocol definitions for runtime configuration commands
 *
 * Type definitions are sourced from nanopb-generated config.pb.h (the single
 * source of truth from config.proto). This file provides C++ enum wrappers
 * for type safety.
 *
 * Message payloads use protobuf encoding via nanopb (firmware) and prost (CLI).
 *
 * Message types are in the 0x20-0x2F range to avoid conflicts with
 * OTA message types (0x01-0x05) and trace types (0x10-0x1F).
 */

#include <cstdint>
#include <cstddef>

// Include the nanopb-generated protobuf definitions (source of truth)
#include "config.pb.h"

namespace domes::config {

/**
 * @brief Runtime-toggleable features (sourced from config.proto)
 *
 * Feature IDs are defined in firmware/common/proto/config.proto.
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
    // NOTE: Add new features to config.proto, not here!
    kCount          = _domes_config_Feature_ARRAYSIZE,
};

/**
 * @brief Config command status codes (sourced from config.proto)
 */
enum class ConfigStatus : uint8_t {
    kOk             = domes_config_Status_STATUS_OK,
    kError          = domes_config_Status_STATUS_ERROR,
    kInvalidFeature = domes_config_Status_STATUS_INVALID_FEATURE,
    kBusy           = domes_config_Status_STATUS_BUSY,
};

/**
 * @brief Config protocol message types (frame-level, not protobuf)
 */
enum class ConfigMsgType : uint8_t {
    kListFeaturesReq = 0x20,  ///< List all features (host -> device)
    kListFeaturesRsp = 0x21,  ///< Feature list response (device -> host)
    kSetFeatureReq   = 0x22,  ///< Set feature state (host -> device)
    kSetFeatureRsp   = 0x23,  ///< Set feature response (device -> host)
    kGetFeatureReq   = 0x24,  ///< Get single feature state (host -> device)
    kGetFeatureRsp   = 0x25,  ///< Get feature response (device -> host)
};

/**
 * @brief Check if a message type is a config command
 */
inline bool isConfigMessage(uint8_t type) {
    return type >= static_cast<uint8_t>(ConfigMsgType::kListFeaturesReq) &&
           type <= static_cast<uint8_t>(ConfigMsgType::kGetFeatureRsp);
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
inline const char* configStatusToString(ConfigStatus status) {
    switch (status) {
        case ConfigStatus::kOk:             return "ok";
        case ConfigStatus::kError:          return "error";
        case ConfigStatus::kInvalidFeature: return "invalid-feature";
        case ConfigStatus::kBusy:           return "busy";
        default:                            return "unknown";
    }
}

/// Maximum features supported
constexpr size_t kMaxFeatures = static_cast<size_t>(Feature::kCount);

/// Maximum frame size for config messages
constexpr size_t kMaxFrameSize = 256;

}  // namespace domes::config
