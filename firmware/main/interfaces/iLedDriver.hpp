/**
 * @file iLedDriver.hpp
 * @brief Abstract interface for LED driver implementations
 */

#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @brief RGBW color structure for SK6812 LEDs
 */
struct Color {
    uint8_t r;  ///< Red component (0-255)
    uint8_t g;  ///< Green component (0-255)
    uint8_t b;  ///< Blue component (0-255)
    uint8_t w;  ///< White component (0-255)

    /// Create black (off) color
    static constexpr Color black() { return {0, 0, 0, 0}; }

    /// Create pure white using white LED
    static constexpr Color white(uint8_t brightness = 255) {
        return {0, 0, 0, brightness};
    }

    /// Create red color
    static constexpr Color red(uint8_t brightness = 255) {
        return {brightness, 0, 0, 0};
    }

    /// Create green color
    static constexpr Color green(uint8_t brightness = 255) {
        return {0, brightness, 0, 0};
    }

    /// Create blue color
    static constexpr Color blue(uint8_t brightness = 255) {
        return {0, 0, brightness, 0};
    }
};

/**
 * @brief Abstract LED driver interface
 *
 * Provides a hardware-independent interface for controlling
 * addressable LED strips (SK6812 RGBW).
 */
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    /**
     * @brief Initialize the LED driver hardware
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Set color of a single LED
     * @param index LED index (0-based)
     * @param color RGBW color value
     * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index out of range
     */
    virtual esp_err_t setPixel(uint8_t index, Color color) = 0;

    /**
     * @brief Set all LEDs to the same color
     * @param color RGBW color value
     * @return ESP_OK on success
     */
    virtual esp_err_t fill(Color color) = 0;

    /**
     * @brief Turn off all LEDs
     * @return ESP_OK on success
     */
    virtual esp_err_t clear() = 0;

    /**
     * @brief Update LEDs with current buffer values
     *
     * Call this after setPixel/fill to push changes to LEDs.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t show() = 0;

    /**
     * @brief Set global brightness scaling
     * @param brightness Brightness level (0-255)
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Get number of LEDs
     * @return Number of LEDs in the strip
     */
    virtual uint8_t getLedCount() const = 0;
};
