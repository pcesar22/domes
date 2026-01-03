/**
 * @file pinsPcbV1.hpp
 * @brief Pin definitions for DOMES custom PCB v1
 *
 * This configuration is for the first revision of the custom PCB.
 * Pin assignments may differ from DevKit based on PCB layout requirements.
 *
 * NOTE: Update these values once PCB layout is finalized.
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
constexpr gpio_num_t kAudioEnable = GPIO_NUM_7;

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
// Charging Status (TP4056)
// ============================================
constexpr gpio_num_t kChargeStatus = GPIO_NUM_15;
constexpr gpio_num_t kChargeStandby = GPIO_NUM_16;

// ============================================
// USB (Native USB-JTAG)
// ============================================
constexpr gpio_num_t kUsbDp = GPIO_NUM_19;
constexpr gpio_num_t kUsbDm = GPIO_NUM_20;

}  // namespace pins
