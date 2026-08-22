#pragma once

/**
 * @file configProtocol.hpp
 * @brief Wire protocol definitions for runtime configuration commands
 *
 * ALL TYPE DEFINITIONS ARE SOURCED FROM config.proto via nanopb-generated config.pb.h.
 * This file provides C++ enum class wrappers for type safety only.
 * DO NOT add new message types or enums here - add them to config.proto instead.
 */

#include <cstddef>
#include <cstdint>

// Include the nanopb-generated protobuf definitions (source of truth)
#include "config.pb.h"

namespace domes::config {

/**
 * @brief Config protocol message types (sourced from config.proto)
 */
enum class MsgType : uint8_t {
    kUnknown = domes_config_MsgType_MSG_TYPE_UNKNOWN,
    // Config commands (0x20-0x2B)
    kListFeaturesReq = domes_config_MsgType_MSG_TYPE_LIST_FEATURES_REQ,
    kListFeaturesRsp = domes_config_MsgType_MSG_TYPE_LIST_FEATURES_RSP,
    kSetFeatureReq = domes_config_MsgType_MSG_TYPE_SET_FEATURE_REQ,
    kSetFeatureRsp = domes_config_MsgType_MSG_TYPE_SET_FEATURE_RSP,
    kGetFeatureReq = domes_config_MsgType_MSG_TYPE_GET_FEATURE_REQ,
    kGetFeatureRsp = domes_config_MsgType_MSG_TYPE_GET_FEATURE_RSP,
    kSetLedPatternReq = domes_config_MsgType_MSG_TYPE_SET_LED_PATTERN_REQ,
    kSetLedPatternRsp = domes_config_MsgType_MSG_TYPE_SET_LED_PATTERN_RSP,
    kGetLedPatternReq = domes_config_MsgType_MSG_TYPE_GET_LED_PATTERN_REQ,
    kGetLedPatternRsp = domes_config_MsgType_MSG_TYPE_GET_LED_PATTERN_RSP,
    kSetImuTriageReq = domes_config_MsgType_MSG_TYPE_SET_IMU_TRIAGE_REQ,
    kSetImuTriageRsp = domes_config_MsgType_MSG_TYPE_SET_IMU_TRIAGE_RSP,
    // System mode commands (0x30-0x35)
    kGetModeReq = domes_config_MsgType_MSG_TYPE_GET_MODE_REQ,
    kGetModeRsp = domes_config_MsgType_MSG_TYPE_GET_MODE_RSP,
    kSetModeReq = domes_config_MsgType_MSG_TYPE_SET_MODE_REQ,
    kSetModeRsp = domes_config_MsgType_MSG_TYPE_SET_MODE_RSP,
    kGetSystemInfoReq = domes_config_MsgType_MSG_TYPE_GET_SYSTEM_INFO_REQ,
    kGetSystemInfoRsp = domes_config_MsgType_MSG_TYPE_GET_SYSTEM_INFO_RSP,
    kSetPodIdReq = domes_config_MsgType_MSG_TYPE_SET_POD_ID_REQ,
    kSetPodIdRsp = domes_config_MsgType_MSG_TYPE_SET_POD_ID_RSP,
    // Observability commands (0x38-0x43)
    kGetHealthReq = domes_config_MsgType_MSG_TYPE_GET_HEALTH_REQ,
    kGetHealthRsp = domes_config_MsgType_MSG_TYPE_GET_HEALTH_RSP,
    kGetEspNowStatusReq = domes_config_MsgType_MSG_TYPE_GET_ESPNOW_STATUS_REQ,
    kGetEspNowStatusRsp = domes_config_MsgType_MSG_TYPE_GET_ESPNOW_STATUS_RSP,
    kEspNowBenchReq = domes_config_MsgType_MSG_TYPE_ESPNOW_BENCH_REQ,
    kEspNowBenchRsp = domes_config_MsgType_MSG_TYPE_ESPNOW_BENCH_RSP,
    // Crash dump commands (0x3E-0x41)
    kGetCrashDumpReq = domes_config_MsgType_MSG_TYPE_GET_CRASH_DUMP_REQ,
    kGetCrashDumpRsp = domes_config_MsgType_MSG_TYPE_GET_CRASH_DUMP_RSP,
    kClearCrashDumpReq = domes_config_MsgType_MSG_TYPE_CLEAR_CRASH_DUMP_REQ,
    kClearCrashDumpRsp = domes_config_MsgType_MSG_TYPE_CLEAR_CRASH_DUMP_RSP,
    // Memory profiler commands (0x42-0x43)
    kGetMemoryProfileReq = domes_config_MsgType_MSG_TYPE_GET_MEMORY_PROFILE_REQ,
    kGetMemoryProfileRsp = domes_config_MsgType_MSG_TYPE_GET_MEMORY_PROFILE_RSP,
    // Self-test / smoke test commands (0x44-0x45)
    kSelfTestReq = domes_config_MsgType_MSG_TYPE_SELF_TEST_REQ,
    kSelfTestRsp = domes_config_MsgType_MSG_TYPE_SELF_TEST_RSP,
    // GitHub OTA commands (0x46-0x49)
    kCheckUpdateReq = domes_config_MsgType_MSG_TYPE_CHECK_UPDATE_REQ,
    kCheckUpdateRsp = domes_config_MsgType_MSG_TYPE_CHECK_UPDATE_RSP,
    kSetAutoUpdateReq = domes_config_MsgType_MSG_TYPE_SET_AUTO_UPDATE_REQ,
    kSetAutoUpdateRsp = domes_config_MsgType_MSG_TYPE_SET_AUTO_UPDATE_RSP,
    // Touch injection commands (0x4C-0x4D)
    kSimulateTouchReq = domes_config_MsgType_MSG_TYPE_SIMULATE_TOUCH_REQ,
    kSimulateTouchRsp = domes_config_MsgType_MSG_TYPE_SIMULATE_TOUCH_RSP,
    // Sim drill mode commands (0x4E-0x4F)
    kSetSimModeReq = domes_config_MsgType_MSG_TYPE_SET_SIM_MODE_REQ,
    kSetSimModeRsp = domes_config_MsgType_MSG_TYPE_SET_SIM_MODE_RSP,
    // Unsolicited device-originated touch notification (0x50)
    kTouchEventNtf = domes_config_MsgType_MSG_TYPE_TOUCH_EVENT_NTF,
    kGetAudioVolumeReq = domes_config_MsgType_MSG_TYPE_GET_AUDIO_VOLUME_REQ,
    kGetAudioVolumeRsp = domes_config_MsgType_MSG_TYPE_GET_AUDIO_VOLUME_RSP,
    kSetAudioVolumeReq = domes_config_MsgType_MSG_TYPE_SET_AUDIO_VOLUME_REQ,
    kSetAudioVolumeRsp = domes_config_MsgType_MSG_TYPE_SET_AUDIO_VOLUME_RSP,
    kTriggerFeedbackReq = domes_config_MsgType_MSG_TYPE_TRIGGER_FEEDBACK_REQ,
    kTriggerFeedbackRsp = domes_config_MsgType_MSG_TYPE_TRIGGER_FEEDBACK_RSP,
};

enum class FeedbackProbe : uint8_t {
    kUnknown = domes_config_FeedbackProbe_FEEDBACK_PROBE_UNKNOWN,
    kEmbeddedBeep = domes_config_FeedbackProbe_FEEDBACK_PROBE_EMBEDDED_BEEP,
    kFixedHaptic = domes_config_FeedbackProbe_FEEDBACK_PROBE_FIXED_HAPTIC,
};

/**
 * @brief Runtime-toggleable features (sourced from config.proto)
 */
enum class Feature : uint8_t {
    kUnknown = domes_config_Feature_FEATURE_UNKNOWN,
    kLedEffects = domes_config_Feature_FEATURE_LED_EFFECTS,
    kBleAdvertising = domes_config_Feature_FEATURE_BLE_ADVERTISING,
    kWifi = domes_config_Feature_FEATURE_WIFI,
    kEspNow = domes_config_Feature_FEATURE_ESP_NOW,
    kTouch = domes_config_Feature_FEATURE_TOUCH,
    kHaptic = domes_config_Feature_FEATURE_HAPTIC,
    kAudio = domes_config_Feature_FEATURE_AUDIO,
    kCount = _domes_config_Feature_ARRAYSIZE,
};

/**
 * @brief Config command status codes (sourced from config.proto)
 */
enum class Status : uint8_t {
    kOk = domes_config_Status_STATUS_OK,
    kError = domes_config_Status_STATUS_ERROR,
    kInvalidFeature = domes_config_Status_STATUS_INVALID_FEATURE,
    kBusy = domes_config_Status_STATUS_BUSY,
    kInvalidPattern = domes_config_Status_STATUS_INVALID_PATTERN,
    kNoData = domes_config_Status_STATUS_NO_DATA,
    kInvalidValue = domes_config_Status_STATUS_INVALID_VALUE,
    kDisabled = domes_config_Status_STATUS_DISABLED,
    kRejected = domes_config_Status_STATUS_REJECTED,
    kStorageError = domes_config_Status_STATUS_STORAGE_ERROR,
};

/**
 * @brief Check if a message type is a valid host-to-device config request.
 */
constexpr bool isConfigRequest(uint8_t type) {
    switch (static_cast<MsgType>(type)) {
        case MsgType::kListFeaturesReq:
        case MsgType::kSetFeatureReq:
        case MsgType::kGetFeatureReq:
        case MsgType::kSetLedPatternReq:
        case MsgType::kGetLedPatternReq:
        case MsgType::kSetImuTriageReq:
        case MsgType::kGetModeReq:
        case MsgType::kSetModeReq:
        case MsgType::kGetSystemInfoReq:
        case MsgType::kSetPodIdReq:
        case MsgType::kGetHealthReq:
        case MsgType::kGetEspNowStatusReq:
        case MsgType::kEspNowBenchReq:
        case MsgType::kGetCrashDumpReq:
        case MsgType::kClearCrashDumpReq:
        case MsgType::kGetMemoryProfileReq:
        case MsgType::kSelfTestReq:
        case MsgType::kCheckUpdateReq:
        case MsgType::kSetAutoUpdateReq:
        case MsgType::kSimulateTouchReq:
        case MsgType::kSetSimModeReq:
        case MsgType::kGetAudioVolumeReq:
        case MsgType::kSetAudioVolumeReq:
        case MsgType::kTriggerFeedbackReq:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Whether a request represents user activity rather than observation.
 *
 * Passive reads must not change system mode or extend the TRIAGE timeout.
 */
constexpr bool commandRecordsActivity(MsgType type) {
    switch (type) {
        case MsgType::kSetFeatureReq:
        case MsgType::kSetLedPatternReq:
        case MsgType::kSetImuTriageReq:
        case MsgType::kSetModeReq:
        case MsgType::kSetPodIdReq:
        case MsgType::kEspNowBenchReq:
        case MsgType::kClearCrashDumpReq:
        case MsgType::kSelfTestReq:
        case MsgType::kCheckUpdateReq:
        case MsgType::kSetAutoUpdateReq:
        case MsgType::kSimulateTouchReq:
        case MsgType::kSetSimModeReq:
        case MsgType::kSetAudioVolumeReq:
        case MsgType::kTriggerFeedbackReq:
            return true;

        default:
            return false;
    }
}

/**
 * @brief Validate bounded LED fields before narrowing protobuf integers.
 */
inline bool isValidLedPattern(const domes_config_LedPattern& pattern) {
    const auto colorFits = [](const domes_config_Color& color) {
        return color.r <= UINT8_MAX && color.g <= UINT8_MAX && color.b <= UINT8_MAX &&
               color.w <= UINT8_MAX;
    };

    if (pattern.brightness > UINT8_MAX || (pattern.has_color && !colorFits(pattern.color))) {
        return false;
    }
    for (pb_size_t i = 0; i < pattern.colors_count; ++i) {
        if (!colorFits(pattern.colors[i])) {
            return false;
        }
    }

    switch (pattern.type) {
        case domes_config_LedPatternType_LED_PATTERN_OFF:
            return true;
        case domes_config_LedPatternType_LED_PATTERN_SOLID:
            return pattern.has_color;
        case domes_config_LedPatternType_LED_PATTERN_BREATHING:
            return pattern.has_color && pattern.period_ms > 0;
        case domes_config_LedPatternType_LED_PATTERN_COLOR_CYCLE:
            return pattern.period_ms > 0;
        default:
            return false;
    }
}

/**
 * @brief Get human-readable name for a feature
 */
inline const char* featureToString(Feature feature) {
    switch (feature) {
        case Feature::kLedEffects:
            return "led-effects";
        case Feature::kBleAdvertising:
            return "ble";
        case Feature::kWifi:
            return "wifi";
        case Feature::kEspNow:
            return "esp-now";
        case Feature::kTouch:
            return "touch";
        case Feature::kHaptic:
            return "haptic";
        case Feature::kAudio:
            return "audio";
        default:
            return "unknown";
    }
}

/**
 * @brief Get human-readable name for a config status
 */
inline const char* statusToString(Status status) {
    switch (status) {
        case Status::kOk:
            return "ok";
        case Status::kError:
            return "error";
        case Status::kInvalidFeature:
            return "invalid-feature";
        case Status::kBusy:
            return "busy";
        case Status::kInvalidPattern:
            return "invalid-pattern";
        case Status::kNoData:
            return "no-data";
        default:
            return "unknown";
    }
}

/// Maximum features supported
constexpr size_t kMaxFeatures = static_cast<size_t>(Feature::kCount);

/// Maximum frame size for config messages (increased for memory profile + self-test responses)
constexpr size_t kMaxFrameSize = 1200;

}  // namespace domes::config
