#pragma once

/**
 * @file iImuDriver.hpp
 * @brief Abstract interface for IMU (accelerometer) drivers
 *
 * Provides a hardware-agnostic interface for IMU drivers supporting
 * tap detection and acceleration reading.
 */

#include "esp_err.h"

#include <cstdint>

namespace domes {

/**
 * @brief 3-axis acceleration data
 */
struct AccelData {
    float x = 0.0f;  ///< X-axis acceleration in g
    float y = 0.0f;  ///< Y-axis acceleration in g
    float z = 0.0f;  ///< Z-axis acceleration in g
};

/**
 * @brief Abstract interface for IMU drivers
 *
 * Supports initialization, tap detection, and acceleration reading.
 * Implementations should configure hardware-specific interrupt routing.
 */
class IImuDriver {
public:
    virtual ~IImuDriver() = default;

    /**
     * @brief Initialize the IMU hardware
     *
     * Configures I2C communication and sets default parameters.
     * Must be called before any other methods.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_NOT_FOUND if device not detected on I2C bus
     * @return ESP_FAIL on initialization failure
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Enable tap detection
     *
     * Configures the IMU to detect taps and route interrupt to INT1 pin.
     *
     * @param singleTap Enable single tap detection
     * @param doubleTap Enable double tap detection (not yet implemented)
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not initialized
     */
    virtual esp_err_t enableTapDetection(bool singleTap, bool doubleTap) = 0;

    /**
     * @brief Read current acceleration values
     *
     * @param data Output acceleration data in g
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not initialized
     */
    virtual esp_err_t readAccel(AccelData& data) = 0;

    /**
     * @brief Check if a tap was detected
     *
     * Reads the interrupt status register. If a tap was detected,
     * returns true and clears the interrupt.
     *
     * @return true if tap was detected (and interrupt cleared)
     * @return false if no tap pending
     */
    virtual bool isTapDetected() = 0;

    /**
     * @brief Clear any pending interrupts
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t clearInterrupt() = 0;
};

}  // namespace domes
