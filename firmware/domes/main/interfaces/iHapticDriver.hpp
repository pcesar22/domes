#pragma once

/**
 * @file iHapticDriver.hpp
 * @brief Abstract interface for haptic drivers
 *
 * Provides hardware-agnostic interface for haptic feedback.
 * Implementations handle specific haptic driver ICs.
 */

#include "esp_err.h"

#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for haptic drivers
 *
 * Controls haptic feedback motors via driver ICs.
 * Supports playing built-in effects and custom waveforms.
 *
 * @note Call init() before any other methods.
 */
class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;

    /**
     * @brief Initialize the haptic driver
     *
     * Configures the driver IC and calibrates the motor if needed.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already initialized
     * @return ESP_FAIL on hardware failure
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Play a built-in haptic effect
     *
     * Triggers one of the driver's built-in waveform effects.
     * Non-blocking - returns immediately after starting playback.
     *
     * @param effectId Effect ID (1-123 for DRV2605L)
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if effect ID is invalid
     * @return ESP_ERR_INVALID_STATE if not initialized
     */
    virtual esp_err_t playEffect(uint8_t effectId) = 0;

    /**
     * @brief Play a sequence of effects
     *
     * Queues multiple effects to play in sequence.
     * Non-blocking - returns immediately after starting.
     *
     * @param effectIds Array of effect IDs
     * @param count Number of effects (max 8 for DRV2605L)
     * @return ESP_OK on success
     */
    virtual esp_err_t playSequence(const uint8_t* effectIds, size_t count) = 0;

    /**
     * @brief Stop any ongoing haptic playback
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Set motor intensity/strength
     *
     * @param intensity 0-100 (0=off, 100=max)
     */
    virtual void setIntensity(uint8_t intensity) = 0;

    /**
     * @brief Get current intensity setting
     * @return Intensity 0-100
     */
    virtual uint8_t getIntensity() const = 0;

    /**
     * @brief Check if driver is initialized
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Check if haptic playback is active
     */
    virtual bool isPlaying() const = 0;
};

}  // namespace domes
