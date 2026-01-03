/**
 * @file pinsDevkit.hpp
 * @brief Pin definitions for ESP32-S3 DevKit development board
 *
 * This configuration is for initial development using the ESP32-S3-DevKitC-1
 * or similar development board before custom PCB is available.
 */

#pragma once

#include "driver/gpio.h"

namespace pins {

// ============================================
// I2C Bus (Shared: IMU, Haptic Driver)
// ============================================
constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kI2cScl = GPIO_NUM_9;
constexpr uint32_t kI2cFreqHz = 400000;  // 400kHz Fast Mode

// ============================================
// I2S Audio (MAX98357A)
// ============================================
constexpr gpio_num_t kI2sBclk = GPIO_NUM_12;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_11;
constexpr gpio_num_t kI2sDout = GPIO_NUM_13;
constexpr gpio_num_t kAudioEnable = GPIO_NUM_7;  // MAX98357A SD pin

// ============================================
// LED Strip (SK6812 RGBW)
// ============================================
constexpr gpio_num_t kLedData = GPIO_NUM_14;

// ============================================
// Touch Sensing (ESP32-S3 Native Touch)
// ============================================
constexpr gpio_num_t kTouch0 = GPIO_NUM_1;
constexpr gpio_num_t kTouch1 = GPIO_NUM_2;
constexpr gpio_num_t kTouch2 = GPIO_NUM_3;
constexpr gpio_num_t kTouch3 = GPIO_NUM_4;

// ============================================
// IMU Interrupt (LIS2DW12)
// ============================================
constexpr gpio_num_t kImuInterrupt = GPIO_NUM_5;

// ============================================
// Haptic Motor (DRV2605L)
// ============================================
constexpr gpio_num_t kHapticEnable = GPIO_NUM_6;

// ============================================
// Battery Monitoring
// ============================================
constexpr gpio_num_t kBatteryAdc = GPIO_NUM_10;

// ============================================
// USB (Native USB-JTAG)
// ============================================
constexpr gpio_num_t kUsbDp = GPIO_NUM_19;
constexpr gpio_num_t kUsbDm = GPIO_NUM_20;

// ============================================
// JTAG Debug (Optional external)
// ============================================
constexpr gpio_num_t kJtagTms = GPIO_NUM_42;
constexpr gpio_num_t kJtagTdi = GPIO_NUM_41;
constexpr gpio_num_t kJtagTdo = GPIO_NUM_40;
constexpr gpio_num_t kJtagTck = GPIO_NUM_39;

// ============================================
// Status LED (DevKit onboard LED)
// ============================================
constexpr gpio_num_t kStatusLed = GPIO_NUM_48;

}  // namespace pins
