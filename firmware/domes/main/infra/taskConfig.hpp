#pragma once

/**
 * @file taskConfig.hpp
 * @brief FreeRTOS task configuration constants
 *
 * Defines task priorities, core affinity, and stack sizes
 * following ESP-IDF conventions.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace domes::infra {

/**
 * @brief Task priority levels
 *
 * Higher values = higher priority (max configMAX_PRIORITIES-1 = 24)
 * Following ESP-IDF conventions for task priority assignment.
 */
namespace priority {
    constexpr UBaseType_t kCritical = configMAX_PRIORITIES - 1;  // 24 - Audio, time-critical
    constexpr UBaseType_t kHigh     = 15;                         // ESP-NOW, BLE handlers
    constexpr UBaseType_t kMedium   = 10;                         // Game logic, feedback
    constexpr UBaseType_t kLow      = 5;                          // LED updates
    constexpr UBaseType_t kIdle     = 0;                          // Background tasks
}

/**
 * @brief Core affinity constants
 *
 * Core 0 (PRO CPU): WiFi, BLE, ESP-NOW protocol stack
 * Core 1 (APP CPU): Audio, game logic (user-responsive)
 */
namespace core {
    constexpr BaseType_t kProtocol    = 0;             // WiFi, BLE, ESP-NOW
    constexpr BaseType_t kApplication = 1;             // Audio, game logic
    constexpr BaseType_t kAny         = tskNO_AFFINITY; // LED, touch (either core)
}

/**
 * @brief Default stack sizes (in bytes)
 *
 * ESP32 stack sizes are in bytes (not words like some FreeRTOS ports).
 * These are conservative defaults; tune based on actual usage via
 * uxTaskGetStackHighWaterMark().
 */
namespace stack {
    constexpr uint32_t kMinimal  = 2048;   // Simple tasks (LED demo)
    constexpr uint32_t kStandard = 4096;   // Most tasks
    constexpr uint32_t kLarge    = 8192;   // Complex tasks (game logic, JSON parsing)
}

/**
 * @brief Task configuration structure
 *
 * Used by TaskManager for creating managed tasks.
 */
struct TaskConfig {
    const char* name;              ///< Task name (max 16 chars)
    uint32_t stackSize;            ///< Stack size in bytes
    UBaseType_t priority;          ///< Task priority
    BaseType_t coreAffinity;       ///< Core to pin task to (or tskNO_AFFINITY)
    bool subscribeToWatchdog;      ///< Whether to subscribe to TWDT
};

} // namespace domes::infra
