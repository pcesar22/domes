/**
 * @file iImuDriver.hpp
 * @brief Abstract interface for IMU (accelerometer) driver implementations
 */

#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @brief 3-axis acceleration data
 */
struct AccelData {
    int16_t x;  ///< X-axis acceleration (mg)
    int16_t y;  ///< Y-axis acceleration (mg)
    int16_t z;  ///< Z-axis acceleration (mg)
};

/**
 * @brief Tap detection event types
 */
enum class TapEventType : uint8_t {
    None,           ///< No tap detected
    SingleTap,      ///< Single tap detected
    DoubleTap       ///< Double tap detected
};

/**
 * @brief IMU event data
 */
struct ImuEvent {
    TapEventType tapType;       ///< Type of tap event
    uint8_t axis;               ///< Axis of tap (0=X, 1=Y, 2=Z)
    int8_t direction;           ///< Direction of tap (+1 or -1)
    uint32_t timestampUs;       ///< Event timestamp in microseconds
};

/**
 * @brief IMU event callback function type
 */
using ImuCallback = void (*)(const ImuEvent& event, void* userData);

/**
 * @brief Data rate selection for accelerometer
 */
enum class ImuDataRate : uint8_t {
    PowerDown = 0,
    Hz_1_6 = 1,     ///< 1.6 Hz (low power)
    Hz_12_5 = 2,    ///< 12.5 Hz
    Hz_25 = 3,      ///< 25 Hz
    Hz_50 = 4,      ///< 50 Hz
    Hz_100 = 5,     ///< 100 Hz
    Hz_200 = 6,     ///< 200 Hz
    Hz_400 = 7,     ///< 400 Hz
    Hz_800 = 8,     ///< 800 Hz
    Hz_1600 = 9     ///< 1600 Hz
};

/**
 * @brief Full scale range selection
 */
enum class ImuRange : uint8_t {
    G_2 = 0,    ///< ±2g
    G_4 = 1,    ///< ±4g
    G_8 = 2,    ///< ±8g
    G_16 = 3    ///< ±16g
};

/**
 * @brief Abstract IMU driver interface
 *
 * Provides a hardware-independent interface for accelerometer
 * reading and tap detection (LIS2DW12).
 */
class IImuDriver {
public:
    virtual ~IImuDriver() = default;

    /**
     * @brief Initialize the IMU hardware
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Read current acceleration values
     * @param data Output acceleration data (in mg)
     * @return ESP_OK on success
     */
    virtual esp_err_t readAccel(AccelData& data) = 0;

    /**
     * @brief Enable tap detection
     * @param singleTap Enable single tap detection
     * @param doubleTap Enable double tap detection
     * @return ESP_OK on success
     */
    virtual esp_err_t enableTapDetection(bool singleTap, bool doubleTap) = 0;

    /**
     * @brief Disable tap detection
     * @return ESP_OK on success
     */
    virtual esp_err_t disableTapDetection() = 0;

    /**
     * @brief Set tap detection threshold
     * @param threshold Tap threshold (0-31, higher = less sensitive)
     * @return ESP_OK on success
     */
    virtual esp_err_t setTapThreshold(uint8_t threshold) = 0;

    /**
     * @brief Register callback for IMU events (tap detection)
     * @param callback Function to call on IMU events
     * @param userData User data pointer passed to callback
     */
    virtual void setCallback(ImuCallback callback, void* userData) = 0;

    /**
     * @brief Set accelerometer data rate
     * @param rate Data rate selection
     * @return ESP_OK on success
     */
    virtual esp_err_t setDataRate(ImuDataRate rate) = 0;

    /**
     * @brief Set accelerometer full scale range
     * @param range Full scale range selection
     * @return ESP_OK on success
     */
    virtual esp_err_t setRange(ImuRange range) = 0;

    /**
     * @brief Enter low power mode
     * @return ESP_OK on success
     */
    virtual esp_err_t enterLowPowerMode() = 0;

    /**
     * @brief Exit low power mode
     * @return ESP_OK on success
     */
    virtual esp_err_t exitLowPowerMode() = 0;

    /**
     * @brief Check if new data is available
     * @return true if new accelerometer data ready
     */
    virtual bool isDataReady() const = 0;

    /**
     * @brief Enable wake from sleep on motion
     * @param enable true to enable wake on motion
     * @return ESP_OK on success
     */
    virtual esp_err_t enableWakeOnMotion(bool enable) = 0;

    /**
     * @brief Read device ID for verification
     * @return Device ID (should be 0x44 for LIS2DW12)
     */
    virtual uint8_t getDeviceId() const = 0;
};
