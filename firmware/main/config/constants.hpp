/**
 * @file constants.hpp
 * @brief Global constants and configuration values
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace config {

// ============================================
// Firmware Version
// ============================================
constexpr uint8_t kVersionMajor = 0;
constexpr uint8_t kVersionMinor = 1;
constexpr uint8_t kVersionPatch = 0;

// ============================================
// Platform Identification
// ============================================
#if defined(CONFIG_DOMES_PLATFORM_DEVKIT)
    constexpr const char* kPlatformName = "DevKit";
#elif defined(CONFIG_DOMES_PLATFORM_PCB_V1)
    constexpr const char* kPlatformName = "PCB_V1";
#else
    constexpr const char* kPlatformName = "Unknown";
#endif

// ============================================
// LED Configuration
// ============================================
constexpr uint8_t kLedCount = 16;
constexpr uint8_t kLedDefaultBrightness = 128;
constexpr uint32_t kLedRefreshRateHz = 60;

// ============================================
// Audio Configuration
// ============================================
constexpr uint32_t kAudioSampleRate = 16000;
constexpr uint8_t kAudioBitsPerSample = 16;
constexpr size_t kAudioBufferSizeBytes = 1024;

// ============================================
// Haptic Configuration
// ============================================
constexpr uint8_t kHapticI2cAddress = 0x5A;
constexpr uint8_t kHapticDefaultIntensity = 200;
constexpr uint8_t kHapticMaxEffects = 123;

// ============================================
// IMU Configuration
// ============================================
constexpr uint8_t kImuI2cAddress = 0x18;
constexpr uint8_t kImuTapThreshold = 10;

// ============================================
// Communication Configuration
// ============================================
constexpr size_t kMaxPods = 24;
constexpr uint32_t kEspNowLatencyTargetUs = 2000;  // P95 < 2ms
constexpr uint32_t kClockSyncIntervalMs = 100;
constexpr size_t kMaxPacketPayload = 20;

// ============================================
// Power Management
// ============================================
constexpr uint32_t kBatteryAdcSampleIntervalMs = 5000;
constexpr uint16_t kBatteryFullMv = 4200;
constexpr uint16_t kBatteryEmptyMv = 3200;

// ============================================
// System Timing
// ============================================
constexpr uint32_t kDefaultTimeoutMs = 1000;
constexpr size_t kEventQueueSize = 16;
constexpr uint32_t kWatchdogTimeoutMs = 10000;

// ============================================
// Task Configuration
// ============================================
constexpr size_t kAudioTaskStackSize = 4096;
constexpr size_t kGameTaskStackSize = 4096;
constexpr size_t kCommTaskStackSize = 4096;
constexpr size_t kLedTaskStackSize = 2048;

constexpr uint8_t kAudioTaskPriority = 5;
constexpr uint8_t kGameTaskPriority = 4;
constexpr uint8_t kCommTaskPriority = 3;
constexpr uint8_t kLedTaskPriority = 2;

// Task core affinity
constexpr int kAudioTaskCore = 1;
constexpr int kGameTaskCore = 1;
constexpr int kCommTaskCore = 0;
constexpr int kLedTaskCore = -1;  // Any core

}  // namespace config
