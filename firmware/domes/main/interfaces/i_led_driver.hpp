#pragma once

#include <cstdint>
#include "esp_err.h"

namespace domes {

/// RGBW color value
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t w = 0;  // White channel (ignored for RGB-only LEDs like WS2812)

    /// Create from RGB values
    static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {r, g, b, 0};
    }

    /// Create from RGBW values
    static constexpr Color rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        return {r, g, b, w};
    }

    /// Predefined colors
    static constexpr Color red()     { return {255, 0, 0, 0}; }
    static constexpr Color green()   { return {0, 255, 0, 0}; }
    static constexpr Color blue()    { return {0, 0, 255, 0}; }
    static constexpr Color white()   { return {255, 255, 255, 0}; }
    static constexpr Color warmWhite() { return {0, 0, 0, 255}; }  // RGBW only
    static constexpr Color yellow()  { return {255, 255, 0, 0}; }
    static constexpr Color cyan()    { return {0, 255, 255, 0}; }
    static constexpr Color magenta() { return {255, 0, 255, 0}; }
    static constexpr Color orange()  { return {255, 128, 0, 0}; }
    static constexpr Color off()     { return {0, 0, 0, 0}; }
};

/// Abstract interface for LED strip/ring drivers
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    /// Initialize the LED driver hardware
    /// @return ESP_OK on success
    virtual esp_err_t init() = 0;

    /// Set color of a single LED (does not refresh immediately)
    /// @param index LED index (0-based)
    /// @param color RGBW color value
    /// @return ESP_OK on success, ESP_ERR_INVALID_ARG if index out of range
    virtual esp_err_t setPixel(uint8_t index, Color color) = 0;

    /// Set all LEDs to the same color (does not refresh immediately)
    /// @param color RGBW color value
    /// @return ESP_OK on success
    virtual esp_err_t setAll(Color color) = 0;

    /// Clear all LEDs (set to off, does not refresh immediately)
    /// @return ESP_OK on success
    virtual esp_err_t clear() = 0;

    /// Refresh the LED strip (send data to LEDs)
    /// @return ESP_OK on success
    virtual esp_err_t refresh() = 0;

    /// Set global brightness scaling (0-255)
    /// @param brightness Brightness level
    virtual void setBrightness(uint8_t brightness) = 0;

    /// Get the number of LEDs in the strip
    /// @return LED count
    virtual uint8_t getLedCount() const = 0;
};

} // namespace domes
