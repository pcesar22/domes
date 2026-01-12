#pragma once

/**
 * @file configProtocol.hpp
 * @brief Wire protocol definitions for runtime configuration commands
 *
 * Defines the message types and payload structures for runtime feature
 * configuration over the serial transport. Uses the same frame format as
 * OTA protocol (0xAA55 start, length, type, payload, CRC32).
 *
 * Message types are in the 0x20-0x2F range to avoid conflicts with
 * OTA message types (0x01-0x05) and trace types (0x10-0x1F).
 */

#include <cstdint>
#include <cstddef>

namespace domes::config {

/**
 * @brief Runtime-toggleable features
 *
 * Each feature can be enabled/disabled at runtime via the config protocol.
 * Feature IDs are stable and should not be reordered.
 */
enum class Feature : uint8_t {
    kUnknown        = 0,  ///< Invalid/unknown feature
    kLedEffects     = 1,  ///< LED animation effects
    kBleAdvertising = 2,  ///< BLE advertising and connections
    kWifi           = 3,  ///< WiFi stack
    kEspNow         = 4,  ///< ESP-NOW communication
    kTouch          = 5,  ///< Touch sensing
    kHaptic         = 6,  ///< Haptic feedback
    kAudio          = 7,  ///< Audio playback
    // Add new features here, don't reorder existing ones
    kCount          = 8,  ///< Number of features (for iteration)
};

/**
 * @brief Config protocol message types
 *
 * These extend the frame protocol with config-specific commands.
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
 *
 * @param type Message type byte
 * @return true if this is a config message
 */
inline bool isConfigMessage(uint8_t type) {
    return type >= static_cast<uint8_t>(ConfigMsgType::kListFeaturesReq) &&
           type <= static_cast<uint8_t>(ConfigMsgType::kGetFeatureRsp);
}

/**
 * @brief Config command status codes
 */
enum class ConfigStatus : uint8_t {
    kOk             = 0x00,  ///< Command succeeded
    kError          = 0x01,  ///< Generic error
    kInvalidFeature = 0x02,  ///< Unknown feature ID
    kBusy           = 0x03,  ///< Device busy, try again
};

/**
 * @brief Feature state entry (used in list response)
 */
#pragma pack(push, 1)
struct FeatureState {
    uint8_t feature;  ///< Feature ID (cast to Feature enum)
    uint8_t enabled;  ///< 1 if enabled, 0 if disabled
};
static_assert(sizeof(FeatureState) == 2, "FeatureState size mismatch");
#pragma pack(pop)

/**
 * @brief List features response payload
 */
#pragma pack(push, 1)
struct ListFeaturesResponse {
    uint8_t status;  ///< ConfigStatus value
    uint8_t count;   ///< Number of features
    // Followed by: FeatureState[count]
};
static_assert(sizeof(ListFeaturesResponse) == 2, "ListFeaturesResponse size mismatch");
#pragma pack(pop)

/**
 * @brief Set feature request payload
 */
#pragma pack(push, 1)
struct SetFeatureRequest {
    uint8_t feature;  ///< Feature ID to set
    uint8_t enabled;  ///< 1 to enable, 0 to disable
};
static_assert(sizeof(SetFeatureRequest) == 2, "SetFeatureRequest size mismatch");
#pragma pack(pop)

/**
 * @brief Set feature response payload
 */
#pragma pack(push, 1)
struct SetFeatureResponse {
    uint8_t status;   ///< ConfigStatus value
    uint8_t feature;  ///< Feature ID that was set
    uint8_t enabled;  ///< New state (1 enabled, 0 disabled)
};
static_assert(sizeof(SetFeatureResponse) == 3, "SetFeatureResponse size mismatch");
#pragma pack(pop)

/**
 * @brief Get feature request payload
 */
#pragma pack(push, 1)
struct GetFeatureRequest {
    uint8_t feature;  ///< Feature ID to query
};
static_assert(sizeof(GetFeatureRequest) == 1, "GetFeatureRequest size mismatch");
#pragma pack(pop)

/**
 * @brief Get feature response payload
 */
#pragma pack(push, 1)
struct GetFeatureResponse {
    uint8_t status;   ///< ConfigStatus value
    uint8_t feature;  ///< Feature ID queried
    uint8_t enabled;  ///< Current state (1 enabled, 0 disabled)
};
static_assert(sizeof(GetFeatureResponse) == 3, "GetFeatureResponse size mismatch");
#pragma pack(pop)

/**
 * @brief Get human-readable name for a feature
 *
 * @param feature Feature ID
 * @return Feature name string
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
 *
 * @param status Status code
 * @return Status name string
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

/// Maximum features in list response
constexpr size_t kMaxFeatures = static_cast<size_t>(Feature::kCount);

/// Maximum payload size for list features response
constexpr size_t kMaxListFeaturesPayload =
    sizeof(ListFeaturesResponse) + (kMaxFeatures * sizeof(FeatureState));

}  // namespace domes::config
