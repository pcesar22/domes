/**
 * @file iHapticDriver.hpp
 * @brief Abstract interface for haptic driver implementations
 */

#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @brief Haptic effect library selection for DRV2605L
 */
enum class HapticLibrary : uint8_t {
    Empty = 0,          ///< No effects
    StrongClick = 1,    ///< Strong click library (ERM)
    SharpClick = 2,     ///< Sharp click library (ERM)
    SoftClick = 3,      ///< Soft click library (ERM)
    StrongBuzz = 4,     ///< Strong buzz library (ERM)
    Alert = 5,          ///< Alert library (ERM)
    LRA = 6             ///< LRA library (for LRA motors)
};

/**
 * @brief Common haptic effect IDs (DRV2605L built-in effects)
 */
namespace HapticEffect {
    constexpr uint8_t kStrongClick = 1;
    constexpr uint8_t kSharpClick = 4;
    constexpr uint8_t kSoftClick = 7;
    constexpr uint8_t kDoubleClick = 10;
    constexpr uint8_t kTripleClick = 12;
    constexpr uint8_t kSoftBump = 13;
    constexpr uint8_t kDoubleBump = 14;
    constexpr uint8_t kTripleBump = 16;
    constexpr uint8_t kBuzz = 47;
    constexpr uint8_t kShortBuzz = 49;
    constexpr uint8_t kPulsing = 52;
    constexpr uint8_t kHum = 58;
    constexpr uint8_t kRampUp = 64;
    constexpr uint8_t kRampDown = 70;
    constexpr uint8_t kTransition = 76;
    constexpr uint8_t kAlert = 82;
}

/**
 * @brief Abstract haptic driver interface
 *
 * Provides a hardware-independent interface for haptic feedback
 * control (DRV2605L with LRA motor).
 */
class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;

    /**
     * @brief Initialize the haptic driver hardware
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Play a built-in haptic effect
     * @param effectId Effect ID (1-123 for DRV2605L)
     * @return ESP_OK on success, ESP_ERR_INVALID_ARG if invalid effect
     */
    virtual esp_err_t playEffect(uint8_t effectId) = 0;

    /**
     * @brief Play a sequence of effects
     * @param effects Array of effect IDs (max 8)
     * @param count Number of effects in sequence
     * @return ESP_OK on success
     */
    virtual esp_err_t playSequence(const uint8_t* effects, uint8_t count) = 0;

    /**
     * @brief Play a simple vibration pulse
     * @param intensity Vibration intensity (0-255)
     * @param durationMs Duration in milliseconds
     * @return ESP_OK on success
     */
    virtual esp_err_t pulse(uint8_t intensity, uint16_t durationMs) = 0;

    /**
     * @brief Stop any current haptic output
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Set the effect library
     * @param library Library selection
     * @return ESP_OK on success
     */
    virtual esp_err_t setLibrary(HapticLibrary library) = 0;

    /**
     * @brief Set default intensity for effects
     * @param intensity Intensity level (0-255)
     */
    virtual void setIntensity(uint8_t intensity) = 0;

    /**
     * @brief Check if haptic output is currently active
     * @return true if active, false otherwise
     */
    virtual bool isActive() const = 0;

    /**
     * @brief Enable or disable the haptic driver
     * @param enable true to enable, false to disable (standby)
     * @return ESP_OK on success
     */
    virtual esp_err_t setEnabled(bool enable) = 0;
};
