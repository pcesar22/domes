#pragma once

/**
 * @file lis2dw12.hpp
 * @brief LIS2DW12 3-axis accelerometer driver
 *
 * Supports I2C communication, tap detection, and acceleration reading.
 * Designed for use with ESP-IDF's I2C master driver (v5.x API).
 *
 * Datasheet: https://www.st.com/resource/en/datasheet/lis2dw12.pdf
 */

#include "interfaces/iImuDriver.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"

#include <cstdint>

namespace domes {

/**
 * @brief LIS2DW12 register addresses
 */
namespace lis2dw12 {

// Device identification
constexpr uint8_t kRegWhoAmI = 0x0F;
constexpr uint8_t kWhoAmIValue = 0x44;  // Expected WHO_AM_I response

// Control registers
constexpr uint8_t kRegCtrl1 = 0x20;    // ODR, mode, LP mode
constexpr uint8_t kRegCtrl2 = 0x21;    // Boot, soft reset, CS options
constexpr uint8_t kRegCtrl3 = 0x22;    // Self-test, PP/OD, H_LACTIVE, LIR, SLP_MODE
constexpr uint8_t kRegCtrl4Int1 = 0x23; // INT1 routing
constexpr uint8_t kRegCtrl5Int2 = 0x24; // INT2 routing
constexpr uint8_t kRegCtrl6 = 0x25;    // BW, FS, low noise
constexpr uint8_t kRegCtrl7 = 0x3F;    // Interrupt enable, usr_off, HP

// Status and data registers
constexpr uint8_t kRegStatus = 0x27;
constexpr uint8_t kRegOutXL = 0x28;
constexpr uint8_t kRegOutXH = 0x29;
constexpr uint8_t kRegOutYL = 0x2A;
constexpr uint8_t kRegOutYH = 0x2B;
constexpr uint8_t kRegOutZL = 0x2C;
constexpr uint8_t kRegOutZH = 0x2D;

// Tap detection registers
constexpr uint8_t kRegTapThsX = 0x30;  // X-axis tap threshold
constexpr uint8_t kRegTapThsY = 0x31;  // Y-axis tap threshold
constexpr uint8_t kRegTapThsZ = 0x32;  // Z-axis tap threshold + priority
constexpr uint8_t kRegIntDur = 0x33;   // Tap duration, latency, quiet
constexpr uint8_t kRegWakeUpThs = 0x34; // Single/double tap, wake-up threshold
constexpr uint8_t kRegWakeUpDur = 0x35; // Wake-up duration, sleep duration
constexpr uint8_t kRegFreeFall = 0x36;  // Free-fall configuration

// Interrupt source registers
constexpr uint8_t kRegStatusDup = 0x37; // Status duplicate
constexpr uint8_t kRegWakeUpSrc = 0x38; // Wake-up interrupt source
constexpr uint8_t kRegTapSrc = 0x39;    // Tap interrupt source
constexpr uint8_t kReg6dSrc = 0x3A;     // 6D orientation source
constexpr uint8_t kRegAllIntSrc = 0x3B; // All interrupt sources

// Control register 1 bits
constexpr uint8_t kOdr100Hz = 0x04;     // ODR = 100 Hz
constexpr uint8_t kModeHighPerf = 0x04; // High-performance mode (in LP_MODE bits)
constexpr uint8_t kLpMode1 = 0x00;      // Low-power mode 1

// Control register 4 (INT1 routing)
constexpr uint8_t kInt1SingleTap = 0x40;  // Route single-tap to INT1
constexpr uint8_t kInt1DoubleTap = 0x08;  // Route double-tap to INT1

// Control register 7 bits
constexpr uint8_t kInterruptsEnable = 0x20; // Enable interrupts

// Tap threshold (0-31 range, ~62.5mg per LSB at FS=2g)
constexpr uint8_t kDefaultTapThs = 0x03;  // ~0.19g threshold (very sensitive)

// Tap configuration
constexpr uint8_t kTapXYZEnable = 0x0E;   // Enable tap on X, Y, Z (bits 1-3)
constexpr uint8_t kTapPriorityZYX = 0x00; // Priority: Z > Y > X

// Tap source bits
constexpr uint8_t kTapIa = 0x40;          // Tap event detected
constexpr uint8_t kSingleTap = 0x20;      // Single-tap detected
constexpr uint8_t kDoubleTap = 0x10;      // Double-tap detected

}  // namespace lis2dw12

/**
 * @brief LIS2DW12 accelerometer driver
 *
 * Implements IImuDriver for the LIS2DW12 3-axis accelerometer.
 * Uses ESP-IDF's new I2C master driver (v5.x).
 *
 * @code
 * Lis2dw12Driver imu(i2cBus, 0x19);
 * imu.init();
 * imu.enableTapDetection(true, false);
 *
 * if (imu.isTapDetected()) {
 *     // Handle tap
 * }
 * @endcode
 */
class Lis2dw12Driver : public IImuDriver {
public:
    /**
     * @brief Construct driver instance
     *
     * @param bus I2C master bus handle (must be initialized)
     * @param address I2C device address (0x18 if SA0=low, 0x19 if SA0=high)
     */
    Lis2dw12Driver(i2c_master_bus_handle_t bus, uint8_t address)
        : bus_(bus), address_(address), device_(nullptr), initialized_(false) {}

    ~Lis2dw12Driver() override {
        if (device_) {
            i2c_master_bus_rm_device(device_);
        }
    }

    // Non-copyable
    Lis2dw12Driver(const Lis2dw12Driver&) = delete;
    Lis2dw12Driver& operator=(const Lis2dw12Driver&) = delete;

    esp_err_t init() override {
        if (initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Add device to I2C bus
        i2c_device_config_t devConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address_,
            .scl_speed_hz = 400000,  // 400 kHz
            .scl_wait_us = 0,
            .flags = {.disable_ack_check = 0},
        };

        esp_err_t err = i2c_master_bus_add_device(bus_, &devConfig, &device_);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to add I2C device: %s", esp_err_to_name(err));
            return err;
        }

        // Verify WHO_AM_I register
        uint8_t whoAmI = 0;
        err = readRegister(lis2dw12::kRegWhoAmI, whoAmI);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to read WHO_AM_I: %s", esp_err_to_name(err));
            return ESP_ERR_NOT_FOUND;
        }

        if (whoAmI != lis2dw12::kWhoAmIValue) {
            ESP_LOGE(kTag, "Unexpected WHO_AM_I: 0x%02X (expected 0x%02X)",
                     whoAmI, lis2dw12::kWhoAmIValue);
            return ESP_ERR_NOT_FOUND;
        }

        ESP_LOGI(kTag, "LIS2DW12 detected at address 0x%02X", address_);

        // Configure for 100Hz high-performance mode
        // CTRL1: ODR=100Hz (0x40), MODE=High-perf (0x04)
        err = writeRegister(lis2dw12::kRegCtrl1, 0x44);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to write CTRL1: %s", esp_err_to_name(err));
            return err;
        }

        // CTRL6: Full-scale = 2g (default), low-noise enabled
        err = writeRegister(lis2dw12::kRegCtrl6, 0x04);  // LOW_NOISE=1
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to write CTRL6: %s", esp_err_to_name(err));
            return err;
        }

        initialized_ = true;
        ESP_LOGI(kTag, "LIS2DW12 initialized (100Hz, 2g, high-perf)");
        return ESP_OK;
    }

    esp_err_t enableTapDetection(bool singleTap, bool doubleTap) override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err;

        // CTRL3: Enable latched interrupt (LIR=1)
        err = writeRegister(lis2dw12::kRegCtrl3, 0x10);  // LIR=1
        if (err != ESP_OK) return err;

        // TAP_THS_X: Threshold + enable X
        err = writeRegister(lis2dw12::kRegTapThsX, 0x40 | lis2dw12::kDefaultTapThs);  // 4D=0, 6D_THS[1:0]=0, TAP_THSX
        if (err != ESP_OK) return err;

        // TAP_THS_Y: Threshold + enable Y
        err = writeRegister(lis2dw12::kRegTapThsY, 0x00 | lis2dw12::kDefaultTapThs);  // TAP_THSY
        if (err != ESP_OK) return err;

        // TAP_THS_Z: Threshold + enable Z + priority + enable axes
        // Bits 7:5 = TAP_X_EN, TAP_Y_EN, TAP_Z_EN
        // Bits 4:0 = TAP_THS_Z
        err = writeRegister(lis2dw12::kRegTapThsZ, 0xE0 | lis2dw12::kDefaultTapThs);  // Enable X,Y,Z + threshold
        if (err != ESP_OK) return err;

        // INT_DUR: Tap timing configuration
        // SHOCK[1:0] (bits 1:0) = 0b11 = 4/ODR = 40ms shock duration (more lenient)
        // QUIET[1:0] (bits 3:2) = 0b10 = 3/ODR = 30ms quiet duration
        // LATENCY[3:0] (bits 7:4) = 0b0011 = double-tap latency
        err = writeRegister(lis2dw12::kRegIntDur, 0x3B);  // More lenient timing
        if (err != ESP_OK) return err;

        // WAKE_UP_THS: SINGLE_DOUBLE_TAP bit + wake-up threshold
        uint8_t wakeUpThs = doubleTap ? 0x80 : 0x00;  // Bit 7 = SINGLE_DOUBLE_TAP (0=single only)
        err = writeRegister(lis2dw12::kRegWakeUpThs, wakeUpThs);
        if (err != ESP_OK) return err;

        // CTRL4_INT1: Route tap to INT1
        uint8_t int1Config = 0;
        if (singleTap) int1Config |= lis2dw12::kInt1SingleTap;
        if (doubleTap) int1Config |= lis2dw12::kInt1DoubleTap;
        err = writeRegister(lis2dw12::kRegCtrl4Int1, int1Config);
        if (err != ESP_OK) return err;

        // CTRL7: Enable interrupts
        err = writeRegister(lis2dw12::kRegCtrl7, lis2dw12::kInterruptsEnable);
        if (err != ESP_OK) return err;

        ESP_LOGI(kTag, "Tap detection enabled (single=%d, double=%d)", singleTap, doubleTap);

        // Debug: verify configuration
        debugTapRegisters();

        return ESP_OK;
    }

    esp_err_t readAccel(AccelData& data) override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Read 6 bytes of acceleration data
        uint8_t raw[6];
        esp_err_t err = readRegisters(lis2dw12::kRegOutXL, raw, sizeof(raw));
        if (err != ESP_OK) {
            return err;
        }

        // Convert to signed 16-bit values
        int16_t rawX = static_cast<int16_t>(raw[1] << 8 | raw[0]);
        int16_t rawY = static_cast<int16_t>(raw[3] << 8 | raw[2]);
        int16_t rawZ = static_cast<int16_t>(raw[5] << 8 | raw[4]);

        // Convert to g (at FS=2g, sensitivity = 0.244 mg/LSB for 14-bit mode)
        // Data is left-justified in 16 bits, so divide by 4 to get 14-bit value
        // Then: g = raw_14bit * 0.244 / 1000 = raw_16bit * 0.244 / 4 / 1000 = raw_16bit * 0.000061
        constexpr float kScale = 0.000061f;
        data.x = static_cast<float>(rawX) * kScale;
        data.y = static_cast<float>(rawY) * kScale;
        data.z = static_cast<float>(rawZ) * kScale;

        return ESP_OK;
    }

    bool isTapDetected() override {
        if (!initialized_) {
            return false;
        }

        // Read TAP_SRC register
        uint8_t tapSrc = 0;
        esp_err_t err = readRegister(lis2dw12::kRegTapSrc, tapSrc);
        if (err != ESP_OK) {
            return false;
        }

        // Check if tap event occurred
        bool tapDetected = (tapSrc & lis2dw12::kTapIa) != 0;

        if (tapDetected) {
            bool isSingle = (tapSrc & lis2dw12::kSingleTap) != 0;
            bool isDouble = (tapSrc & lis2dw12::kDoubleTap) != 0;
            ESP_LOGI(kTag, "TAP! single=%d, double=%d, src=0x%02X",
                     isSingle, isDouble, tapSrc);
        }

        return tapDetected;
    }

    /**
     * @brief Debug: read and log all tap-related registers
     */
    void debugTapRegisters() {
        uint8_t val = 0;
        readRegister(lis2dw12::kRegTapSrc, val);
        ESP_LOGI(kTag, "TAP_SRC=0x%02X", val);
        readRegister(lis2dw12::kRegAllIntSrc, val);
        ESP_LOGI(kTag, "ALL_INT_SRC=0x%02X", val);
        readRegister(lis2dw12::kRegCtrl4Int1, val);
        ESP_LOGI(kTag, "CTRL4_INT1=0x%02X", val);
        readRegister(lis2dw12::kRegCtrl7, val);
        ESP_LOGI(kTag, "CTRL7=0x%02X", val);
    }

    esp_err_t clearInterrupt() override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Read ALL_INT_SRC to clear latched interrupts
        uint8_t allIntSrc = 0;
        return readRegister(lis2dw12::kRegAllIntSrc, allIntSrc);
    }

private:
    static constexpr const char* kTag = "lis2dw12";

    /**
     * @brief Read a single register
     */
    esp_err_t readRegister(uint8_t reg, uint8_t& value) {
        return i2c_master_transmit_receive(device_, &reg, 1, &value, 1, kTimeout);
    }

    /**
     * @brief Read multiple consecutive registers
     */
    esp_err_t readRegisters(uint8_t reg, uint8_t* data, size_t len) {
        return i2c_master_transmit_receive(device_, &reg, 1, data, len, kTimeout);
    }

    /**
     * @brief Write a single register
     */
    esp_err_t writeRegister(uint8_t reg, uint8_t value) {
        uint8_t buf[2] = {reg, value};
        return i2c_master_transmit(device_, buf, sizeof(buf), kTimeout);
    }

    static constexpr int kTimeout = 100;  // I2C timeout in ms

    i2c_master_bus_handle_t bus_;
    uint8_t address_;
    i2c_master_dev_handle_t device_;
    bool initialized_;
};

}  // namespace domes
