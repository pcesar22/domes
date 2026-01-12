#pragma once

#include "esp_timer.h"
#include "interfaces/iLedDriver.hpp"

#include <cmath>
#include <cstdint>

namespace domes {

/**
 * @brief LED animation controller with smooth transitions
 *
 * Provides frame-based animation for LED color transitions and effects.
 * Must call update() periodically (recommended: every 16ms for 60fps).
 */
class LedAnimator {
public:
    /**
     * @brief Construct animator for an LED driver
     * @param driver Reference to LED driver (must outlive animator)
     */
    explicit LedAnimator(ILedDriver& driver)
        : driver_(driver),
          currentColor_(Color::off()),
          targetColor_(Color::off()),
          transitionStartMs_(0),
          transitionDurationMs_(0),
          breathingColor_(Color::off()),
          breathingPeriodMs_(0),
          breathingStartMs_(0),
          breathing_(false) {}

    /**
     * @brief Start transition to a new color
     * @param target Target color
     * @param durationMs Transition duration in milliseconds
     */
    void transitionTo(Color target, uint32_t durationMs = 500) {
        // Capture current interpolated color as start point
        if (isAnimating()) {
            currentColor_ = getInterpolatedColor();
        }
        targetColor_ = target;
        transitionStartMs_ = nowMs();
        transitionDurationMs_ = durationMs;
    }

    /**
     * @brief Start breathing effect (pulsing brightness)
     * @param color Base color for breathing
     * @param periodMs Full breath cycle period (inhale + exhale)
     */
    void startBreathing(Color color, uint32_t periodMs = 2000) {
        breathing_ = true;
        breathingColor_ = color;
        breathingPeriodMs_ = periodMs;
        breathingStartMs_ = nowMs();
    }

    /**
     * @brief Stop breathing effect
     */
    void stopBreathing() { breathing_ = false; }

    /**
     * @brief Update animation state and refresh LEDs
     *
     * Call this periodically (every 16ms for 60fps).
     * Handles both color transitions and breathing effects.
     */
    void update() {
        Color output;

        if (breathing_) {
            output = getBreathingColor();
        } else if (isAnimating()) {
            output = getInterpolatedColor();
        } else {
            output = targetColor_;
        }

        driver_.setAll(output);
        driver_.refresh();

        // Update current color when transition completes
        if (!breathing_ && !isAnimating()) {
            currentColor_ = targetColor_;
        }
    }

    /**
     * @brief Check if color transition is in progress
     * @return true if animating between colors
     */
    bool isAnimating() const {
        if (transitionDurationMs_ == 0)
            return false;
        uint32_t elapsed = nowMs() - transitionStartMs_;
        return elapsed < transitionDurationMs_;
    }

    /**
     * @brief Check if breathing effect is active
     * @return true if breathing
     */
    bool isBreathing() const { return breathing_; }

    /**
     * @brief Get current displayed color
     * @return Current color (interpolated if animating)
     */
    Color getCurrentColor() const {
        if (breathing_) {
            return getBreathingColor();
        }
        return isAnimating() ? getInterpolatedColor() : targetColor_;
    }

private:
    static uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

    Color getInterpolatedColor() const {
        if (transitionDurationMs_ == 0)
            return targetColor_;

        uint32_t elapsed = nowMs() - transitionStartMs_;
        if (elapsed >= transitionDurationMs_) {
            return targetColor_;
        }

        // Calculate progress (0-255)
        uint8_t t = static_cast<uint8_t>((elapsed * 255) / transitionDurationMs_);
        return Color::lerp(currentColor_, targetColor_, t);
    }

    Color getBreathingColor() const {
        if (breathingPeriodMs_ == 0)
            return breathingColor_;

        uint32_t elapsed = nowMs() - breathingStartMs_;
        uint32_t phase = elapsed % breathingPeriodMs_;

        // Use sine wave for smooth breathing (0 to 2*PI over period)
        // sin returns -1 to 1, we want 0.1 to 1.0 for brightness
        float angle = (static_cast<float>(phase) / breathingPeriodMs_) * 2.0f * 3.14159265f;
        float brightness = 0.55f + 0.45f * std::sin(angle);  // Range: 0.1 to 1.0

        uint8_t scale = static_cast<uint8_t>(brightness * 255.0f);
        return Color::lerp(Color::off(), breathingColor_, scale);
    }

    ILedDriver& driver_;

    // Color transition state
    Color currentColor_;
    Color targetColor_;
    uint32_t transitionStartMs_;
    uint32_t transitionDurationMs_;

    // Breathing state
    Color breathingColor_;
    uint32_t breathingPeriodMs_;
    uint32_t breathingStartMs_;
    bool breathing_;
};

}  // namespace domes
