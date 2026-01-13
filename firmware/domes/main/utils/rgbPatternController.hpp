#pragma once

/**
 * @file rgbPatternController.hpp
 * @brief RGB LED pattern controller with animated effects
 *
 * Provides multiple animated LED patterns controlled via protocol commands.
 * Must call update() periodically (recommended: every 20ms for 50fps).
 */

#include "config/configProtocol.hpp"
#include "interfaces/iLedDriver.hpp"

#include "esp_random.h"
#include "esp_timer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace domes {

/**
 * @brief RGB pattern configuration
 */
struct RgbPatternConfig {
    config::RgbPattern pattern = config::RgbPattern::kOff;
    Color primaryColor = Color::rgb(255, 0, 0);  ///< Primary color for solid/comet
    uint32_t speedMs = 50;                        ///< Animation step time in ms
    uint8_t brightness = 128;                     ///< Global brightness (0-255)
};

/**
 * @brief RGB LED pattern controller
 *
 * Animates LED patterns based on the configured pattern type.
 * Supports multiple patterns including rainbow chase, comet tail, and sparkle fire.
 */
class RgbPatternController {
public:
    explicit RgbPatternController(ILedDriver& driver)
        : driver_(driver),
          config_(),
          lastUpdateMs_(0),
          animationStep_(0) {}

    /**
     * @brief Set the active pattern configuration
     * @param config Pattern configuration
     */
    void setConfig(const RgbPatternConfig& config) {
        config_ = config;
        driver_.setBrightness(config_.brightness);
        animationStep_ = 0;
        lastUpdateMs_ = nowMs();

        // Initialize pattern state
        if (config_.pattern == config::RgbPattern::kSparkleFire) {
            initSparkleState();
        }
    }

    /**
     * @brief Get current pattern configuration
     * @return Current configuration
     */
    const RgbPatternConfig& getConfig() const { return config_; }

    /**
     * @brief Update animation state and refresh LEDs
     *
     * Call this periodically (every 20ms recommended).
     * Does nothing if not enough time has passed since last update.
     */
    void update() {
        uint32_t now = nowMs();
        uint32_t elapsed = now - lastUpdateMs_;

        // Rate limit updates based on speed setting
        if (elapsed < config_.speedMs) {
            return;
        }
        lastUpdateMs_ = now;

        switch (config_.pattern) {
            case config::RgbPattern::kOff:
                updateOff();
                break;
            case config::RgbPattern::kSolid:
                updateSolid();
                break;
            case config::RgbPattern::kRainbowChase:
                updateRainbowChase();
                break;
            case config::RgbPattern::kCometTail:
                updateCometTail();
                break;
            case config::RgbPattern::kSparkleFire:
                updateSparkleFire();
                break;
            default:
                updateOff();
                break;
        }

        driver_.refresh();
        animationStep_++;
    }

    /**
     * @brief Stop the current pattern and turn off LEDs
     */
    void stop() {
        config_.pattern = config::RgbPattern::kOff;
        driver_.clear();
        driver_.refresh();
    }

private:
    static constexpr uint8_t kMaxLeds = 32;  ///< Maximum supported LEDs

    static uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

    // =========================================================================
    // Pattern: Off
    // =========================================================================
    void updateOff() { driver_.clear(); }

    // =========================================================================
    // Pattern: Solid Color
    // =========================================================================
    void updateSolid() { driver_.setAll(config_.primaryColor); }

    // =========================================================================
    // Pattern: Rainbow Chase
    // Rainbow colors rotating around the LED ring
    // =========================================================================
    void updateRainbowChase() {
        uint8_t ledCount = driver_.getLedCount();
        uint8_t offset = animationStep_ % ledCount;

        for (uint8_t i = 0; i < ledCount; i++) {
            // Calculate hue based on position and animation step
            // Each LED gets a different hue, creating a rainbow spread
            uint8_t hue = static_cast<uint8_t>(((i + offset) * 256) / ledCount);
            Color color = hsvToRgb(hue, 255, 255);
            driver_.setPixel(i, color);
        }
    }

    // =========================================================================
    // Pattern: Comet Tail
    // A bright dot with a fading trail chasing around the ring
    // =========================================================================
    void updateCometTail() {
        uint8_t ledCount = driver_.getLedCount();
        uint8_t headPos = animationStep_ % ledCount;

        // Tail length is proportional to LED count (about 1/3 of ring)
        uint8_t tailLength = std::max(static_cast<uint8_t>(3), static_cast<uint8_t>(ledCount / 3));

        // Clear all LEDs first
        driver_.clear();

        // Draw the comet head (brightest)
        driver_.setPixel(headPos, config_.primaryColor);

        // Draw the fading tail behind the head
        for (uint8_t i = 1; i < tailLength; i++) {
            // Calculate tail position (wrapping around)
            int8_t tailPos = static_cast<int8_t>(headPos) - i;
            if (tailPos < 0) {
                tailPos += ledCount;
            }

            // Calculate fade factor (255 at head, fading to 0)
            uint8_t fade = 255 - static_cast<uint8_t>((i * 255) / tailLength);
            Color tailColor = Color::lerp(Color::off(), config_.primaryColor, fade);
            driver_.setPixel(static_cast<uint8_t>(tailPos), tailColor);
        }
    }

    // =========================================================================
    // Pattern: Sparkle Fire
    // Random sparkling with warm fire colors (red, orange, yellow)
    // =========================================================================
    void updateSparkleFire() {
        uint8_t ledCount = driver_.getLedCount();

        // Update each LED's intensity with flickering
        for (uint8_t i = 0; i < ledCount && i < kMaxLeds; i++) {
            // Random chance to spark or dim
            int8_t delta = static_cast<int8_t>((esp_random() % 60) - 30);  // -30 to +29
            int16_t newIntensity = sparkleIntensity_[i] + delta;

            // Clamp and apply cooling (fire naturally dims)
            newIntensity -= 5;  // Cooling effect
            sparkleIntensity_[i] =
                static_cast<uint8_t>(std::clamp(newIntensity, static_cast<int16_t>(0), static_cast<int16_t>(255)));

            // Random sparks (chance to suddenly brighten)
            if ((esp_random() % 100) < 3) {  // 3% chance per update
                sparkleIntensity_[i] = std::min(255, sparkleIntensity_[i] + 100);
            }

            // Convert intensity to fire color (heat map)
            Color fireColor = heatToColor(sparkleIntensity_[i]);
            driver_.setPixel(i, fireColor);
        }
    }

    void initSparkleState() {
        // Initialize with random low intensities
        for (uint8_t i = 0; i < kMaxLeds; i++) {
            sparkleIntensity_[i] = static_cast<uint8_t>(esp_random() % 100);
        }
    }

    // =========================================================================
    // Color Utilities
    // =========================================================================

    /**
     * @brief Convert HSV to RGB color
     * @param h Hue (0-255)
     * @param s Saturation (0-255)
     * @param v Value/brightness (0-255)
     * @return RGB color
     */
    static Color hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
        if (s == 0) {
            return Color::rgb(v, v, v);  // Grayscale
        }

        uint8_t region = h / 43;
        uint8_t remainder = (h - (region * 43)) * 6;

        uint8_t p = (v * (255 - s)) >> 8;
        uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

        switch (region) {
            case 0:
                return Color::rgb(v, t, p);
            case 1:
                return Color::rgb(q, v, p);
            case 2:
                return Color::rgb(p, v, t);
            case 3:
                return Color::rgb(p, q, v);
            case 4:
                return Color::rgb(t, p, v);
            default:
                return Color::rgb(v, p, q);
        }
    }

    /**
     * @brief Convert heat intensity to fire color
     *
     * Maps 0-255 intensity to black -> red -> orange -> yellow -> white
     *
     * @param heat Heat intensity (0-255)
     * @return Fire color
     */
    static Color heatToColor(uint8_t heat) {
        // Scale heat to 0-191 for better color mapping
        uint8_t t192 = static_cast<uint8_t>((static_cast<uint16_t>(heat) * 191) / 255);

        // Three heat zones: red, red+orange, red+orange+yellow
        uint8_t r, g, b;

        if (t192 < 64) {
            // Zone 1: black to red (0-63)
            r = t192 * 4;
            g = 0;
            b = 0;
        } else if (t192 < 128) {
            // Zone 2: red to orange (64-127)
            r = 255;
            g = (t192 - 64) * 4;
            b = 0;
        } else {
            // Zone 3: orange to yellow/white (128-191)
            r = 255;
            g = 255;
            b = (t192 - 128) * 4;
        }

        return Color::rgb(r, g, b);
    }

    ILedDriver& driver_;
    RgbPatternConfig config_;
    uint32_t lastUpdateMs_;
    uint32_t animationStep_;

    // Sparkle fire state (per-LED intensity)
    uint8_t sparkleIntensity_[kMaxLeds] = {0};
};

}  // namespace domes
