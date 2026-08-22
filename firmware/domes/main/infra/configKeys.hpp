#pragma once

namespace domes::infra {

namespace nvs_ns {
constexpr const char* kConfig = "config";
constexpr const char* kStats = "stats";
constexpr const char* kCalibration = "calibration";
}  // namespace nvs_ns

namespace config_key {
constexpr const char* kBrightness = "brightness";
constexpr const char* kVolume = "volume";
constexpr const char* kTouchThreshold = "touch_thresh";
constexpr const char* kPodId = "pod_id";
constexpr const char* kAutoUpdate = "auto_update";
}  // namespace config_key

namespace stats_key {
constexpr const char* kBootCount = "boot_count";
constexpr const char* kTotalRuntime = "runtime_s";
constexpr const char* kTouchEvents = "touch_events";
}  // namespace stats_key

}  // namespace domes::infra
