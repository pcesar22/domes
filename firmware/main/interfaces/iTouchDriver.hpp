/**
 * @file iTouchDriver.hpp
 * @brief Abstract interface for touch sensor driver implementations
 */

#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @brief Touch event types
 */
enum class TouchEventType : uint8_t {
    None,       ///< No touch event
    Press,      ///< Initial touch detected
    Release,    ///< Touch released
    Hold        ///< Touch held for extended duration
};

/**
 * @brief Touch event data
 */
struct TouchEvent {
    TouchEventType type;        ///< Type of touch event
    uint8_t channel;            ///< Touch channel that triggered event
    uint32_t timestampUs;       ///< Event timestamp in microseconds
    uint16_t rawValue;          ///< Raw sensor reading (for debugging)
};

/**
 * @brief Touch event callback function type
 */
using TouchCallback = void (*)(const TouchEvent& event, void* userData);

/**
 * @brief Abstract touch sensor driver interface
 *
 * Provides a hardware-independent interface for capacitive touch
 * sensing using ESP32-S3 native touch peripheral or external IC.
 */
class ITouchDriver {
public:
    virtual ~ITouchDriver() = default;

    /**
     * @brief Initialize the touch sensor hardware
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Calibrate touch sensors
     *
     * Should be called after init when no touch is present
     * to establish baseline readings.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t calibrate() = 0;

    /**
     * @brief Check if any touch is currently active
     * @return true if touch detected, false otherwise
     */
    virtual bool isTouched() const = 0;

    /**
     * @brief Check if specific channel is touched
     * @param channel Touch channel index
     * @return true if channel is touched
     */
    virtual bool isChannelTouched(uint8_t channel) const = 0;

    /**
     * @brief Get raw reading from touch channel
     * @param channel Touch channel index
     * @return Raw sensor value (lower = touch detected for ESP32)
     */
    virtual uint16_t getRawValue(uint8_t channel) const = 0;

    /**
     * @brief Register callback for touch events
     * @param callback Function to call on touch events
     * @param userData User data pointer passed to callback
     */
    virtual void setCallback(TouchCallback callback, void* userData) = 0;

    /**
     * @brief Set touch detection threshold
     * @param channel Touch channel index
     * @param threshold Detection threshold value
     * @return ESP_OK on success
     */
    virtual esp_err_t setThreshold(uint8_t channel, uint16_t threshold) = 0;

    /**
     * @brief Get number of touch channels
     * @return Number of available touch channels
     */
    virtual uint8_t getChannelCount() const = 0;

    /**
     * @brief Enable wake from sleep on touch
     * @param enable true to enable wake on touch
     * @return ESP_OK on success
     */
    virtual esp_err_t enableWakeOnTouch(bool enable) = 0;

    /**
     * @brief Set debounce time
     * @param debounceMs Debounce time in milliseconds
     */
    virtual void setDebounceMs(uint8_t debounceMs) = 0;
};
