#pragma once

/**
 * @file logging.hpp
 * @brief Module-based logging utilities
 *
 * Provides convenience macros for module-scoped logging following
 * the ESP-IDF pattern with kTag constants.
 *
 * Usage:
 * @code
 * // In module .cpp file:
 * static constexpr const char* kTag = "myModule";
 *
 * void doSomething() {
 *     DOMES_LOGI("Starting operation");
 *     DOMES_LOGD("Value: %d", value);
 * }
 * @endcode
 */

#include "esp_log.h"

// Convenience macros that use kTag (must be defined in scope)
#define DOMES_LOGE(fmt, ...) ESP_LOGE(kTag, fmt, ##__VA_ARGS__)
#define DOMES_LOGW(fmt, ...) ESP_LOGW(kTag, fmt, ##__VA_ARGS__)
#define DOMES_LOGI(fmt, ...) ESP_LOGI(kTag, fmt, ##__VA_ARGS__)
#define DOMES_LOGD(fmt, ...) ESP_LOGD(kTag, fmt, ##__VA_ARGS__)
#define DOMES_LOGV(fmt, ...) ESP_LOGV(kTag, fmt, ##__VA_ARGS__)

// Buffer dump for debugging (hex format)
#define DOMES_LOG_BUFFER_HEX(tag, buffer, len) \
    ESP_LOG_BUFFER_HEX_LEVEL(tag, buffer, len, ESP_LOG_DEBUG)

namespace domes::infra {

/**
 * @brief Set log level for a specific module
 * @param tag Module tag (e.g., "touch", "espnow")
 * @param level ESP log level
 */
inline void setLogLevel(const char* tag, esp_log_level_t level) {
    esp_log_level_set(tag, level);
}

/**
 * @brief Set default log level for all modules
 * @param level ESP log level
 */
inline void setDefaultLogLevel(esp_log_level_t level) {
    esp_log_level_set("*", level);
}

/**
 * @brief Common module tags for consistency
 *
 * Use these constants for kTag definitions to ensure consistent
 * naming across the codebase.
 */
namespace tag {
    constexpr const char* kMain      = "domes";
    constexpr const char* kTask      = "task";
    constexpr const char* kNvs       = "nvs";
    constexpr const char* kWatchdog  = "wdt";
    constexpr const char* kLed       = "led";
    constexpr const char* kTouch     = "touch";
    constexpr const char* kAudio     = "audio";
    constexpr const char* kHaptic    = "haptic";
    constexpr const char* kEspNow    = "espnow";
    constexpr const char* kBle       = "ble";
    constexpr const char* kGame      = "game";
    constexpr const char* kTrace     = "trace";
}

} // namespace domes::infra
