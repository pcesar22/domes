#pragma once

#include "esp_err.h"

#include <cstdint>

namespace domes {

/**
 * @brief Touch pad state for a single pad
 */
struct TouchPadState {
    bool touched = false;      ///< True if pad is currently being touched
    uint32_t rawValue = 0;     ///< Raw capacitance reading
    uint32_t threshold = 0;    ///< Touch detection threshold
};

/**
 * @brief Abstract interface for capacitive touch pad drivers
 *
 * Provides a hardware-agnostic interface for reading touch pad state.
 * Supports up to 4 touch pads.
 */
class ITouchDriver {
public:
    virtual ~ITouchDriver() = default;

    /**
     * @brief Initialize the touch pad hardware
     *
     * Configures the touch peripheral and calibrates baseline readings.
     *
     * @return ESP_OK on success
     * @return ESP_FAIL on hardware initialization failure
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Read touch state for all pads
     *
     * Updates the internal state with current touch readings.
     * Must be called periodically (e.g., every 10ms).
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t update() = 0;

    /**
     * @brief Check if a specific pad is touched
     *
     * @param padIndex Pad index (0-3)
     * @return true if pad is currently being touched
     */
    virtual bool isTouched(uint8_t padIndex) const = 0;

    /**
     * @brief Get detailed state for a specific pad
     *
     * @param padIndex Pad index (0-3)
     * @return Touch pad state including raw value and threshold
     */
    virtual TouchPadState getPadState(uint8_t padIndex) const = 0;

    /**
     * @brief Get the number of configured touch pads
     * @return Number of touch pads (typically 4)
     */
    virtual uint8_t getPadCount() const = 0;

    /**
     * @brief Recalibrate touch pad baselines
     *
     * Call this when pads are not being touched to update baseline.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t calibrate() = 0;
};

}  // namespace domes
