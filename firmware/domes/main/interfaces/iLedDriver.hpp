#pragma once

#include "esp_err.h"

#include <cstdint>

namespace domes {

/**
 * @brief RGBW color value for addressable LEDs
 *
 * Supports both RGB (WS2812) and RGBW (SK6812) LED types.
 * The white channel is ignored for RGB-only LEDs.
 */
struct Color {
    uint8_t r = 0;  ///< Red channel (0-255)
    uint8_t g = 0;  ///< Green channel (0-255)
    uint8_t b = 0;  ///< Blue channel (0-255)
    uint8_t w = 0;  ///< White channel (0-255, ignored for RGB-only LEDs)

    /**
     * @brief Create color from RGB values
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @return Color instance
     */
    static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {r, g, b, 0};
    }

    /**
     * @brief Create color from RGBW values
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param w White value (0-255)
     * @return Color instance
     */
    static constexpr Color rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        return {r, g, b, w};
    }

    /// @name Predefined Colors
    /// @{
    static constexpr Color red()       { return {255, 0, 0, 0}; }
    static constexpr Color green()     { return {0, 255, 0, 0}; }
    static constexpr Color blue()      { return {0, 0, 255, 0}; }
    static constexpr Color white()     { return {255, 255, 255, 0}; }
    static constexpr Color warmWhite() { return {0, 0, 0, 255}; }  ///< RGBW only
    static constexpr Color yellow()    { return {255, 255, 0, 0}; }
    static constexpr Color cyan()      { return {0, 255, 255, 0}; }
    static constexpr Color magenta()   { return {255, 0, 255, 0}; }
    static constexpr Color orange()    { return {255, 128, 0, 0}; }
    static constexpr Color off()       { return {0, 0, 0, 0}; }
    /// @}
};

/**
 * @brief Abstract interface for LED strip/ring drivers
 *
 * Provides a hardware-agnostic interface for controlling addressable LEDs.
 * Supports both WS2812 (RGB) and SK6812 (RGBW) LED types.
 *
 * @note All color-setting methods buffer changes locally. Call refresh()
 *       to send the buffered colors to the LED hardware.
 *
 * @warning Must be initialized by calling init() before any other methods.
 */
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    /**
     * @brief Initialize the LED driver hardware
     *
     * Configures the underlying hardware (RMT, SPI, etc.) for LED control.
     * Must be called once before any other methods.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already initialized
     * @return ESP_FAIL on hardware initialization failure
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Set color of a single LED
     *
     * Buffers the color locally. Call refresh() to apply.
     *
     * @param index LED index (0-based)
     * @param color RGBW color value
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if index >= getLedCount()
     */
    virtual esp_err_t setPixel(uint8_t index, Color color) = 0;

    /**
     * @brief Set all LEDs to the same color
     *
     * Buffers the color locally. Call refresh() to apply.
     *
     * @param color RGBW color value
     * @return ESP_OK on success
     */
    virtual esp_err_t setAll(Color color) = 0;

    /**
     * @brief Clear all LEDs (set to off)
     *
     * Buffers the change locally. Call refresh() to apply.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t clear() = 0;

    /**
     * @brief Send buffered colors to LED hardware
     *
     * Applies brightness scaling and transmits color data to LEDs.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not initialized
     */
    virtual esp_err_t refresh() = 0;

    /**
     * @brief Set global brightness scaling
     *
     * Applied during refresh() to all LED colors.
     *
     * @param brightness Brightness level (0=off, 255=full)
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Get the number of LEDs in the strip
     * @return LED count
     */
    virtual uint8_t getLedCount() const = 0;
};

} // namespace domes
