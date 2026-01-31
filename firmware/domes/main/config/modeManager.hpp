#pragma once

/**
 * @file modeManager.hpp
 * @brief System mode manager for device lifecycle states
 *
 * Controls which features are active based on the current system mode.
 * Transitions between modes apply feature masks atomically via FeatureManager.
 *
 * Modes: BOOTING -> IDLE -> TRIAGE/CONNECTED -> GAME
 *        Any mode -> ERROR -> IDLE (recovery)
 */

#include "featureManager.hpp"
#include "config.pb.h"

#include <atomic>
#include <cstdint>

namespace domes::config {

/**
 * @brief System operating modes (C++ wrapper for proto enum)
 */
enum class SystemMode : uint8_t {
    kBooting   = domes_config_SystemMode_SYSTEM_MODE_BOOTING,
    kIdle      = domes_config_SystemMode_SYSTEM_MODE_IDLE,
    kTriage    = domes_config_SystemMode_SYSTEM_MODE_TRIAGE,
    kConnected = domes_config_SystemMode_SYSTEM_MODE_CONNECTED,
    kGame      = domes_config_SystemMode_SYSTEM_MODE_GAME,
    kError     = domes_config_SystemMode_SYSTEM_MODE_ERROR,
};

/**
 * @brief Get human-readable name for a system mode
 */
const char* systemModeToString(SystemMode mode);

/**
 * @brief System mode manager
 *
 * Thread-safe mode transitions with automatic feature mask application.
 * Call tick() at ~10Hz from a dedicated FreeRTOS task for timeout monitoring.
 */
class ModeManager {
public:
    /**
     * @brief Construct mode manager in BOOTING state
     *
     * @param features Feature manager for mask application
     */
    explicit ModeManager(FeatureManager& features);

    /**
     * @brief Get current system mode (atomic read)
     */
    SystemMode currentMode() const;

    /**
     * @brief Attempt to transition to a new mode
     *
     * Validates the transition, applies the feature mask, and logs the change.
     * Thread-safe.
     *
     * @param newMode Target mode
     * @return true if transition succeeded, false if invalid
     */
    bool transitionTo(SystemMode newMode);

    /**
     * @brief Get time spent in current mode (milliseconds)
     */
    uint32_t timeInModeMs() const;

    /**
     * @brief Reset the activity timer
     *
     * Call this on every config command to prevent TRIAGE timeout.
     */
    void resetActivityTimer();

    /**
     * @brief Periodic tick for timeout monitoring
     *
     * Checks for TRIAGE inactivity timeout and ERROR recovery timeout.
     * Call at ~10Hz from a dedicated task.
     */
    void tick();

    /**
     * @brief Get the feature mask for a given mode
     */
    static uint32_t featureMaskForMode(SystemMode mode);

private:
    bool isValidTransition(SystemMode from, SystemMode to) const;
    void applyFeatureMask(SystemMode mode);

    FeatureManager& features_;
    std::atomic<uint8_t> currentMode_;    // Stored as uint8_t for atomic compatibility
    std::atomic<int64_t> modeEnteredAt_;  // esp_timer_get_time() value
    std::atomic<int64_t> lastActivityAt_; // esp_timer_get_time() value
};

// Timeout constants
constexpr int64_t kTriageTimeoutUs  = 30'000'000;   // 30s inactivity -> IDLE
constexpr int64_t kErrorRecoveryUs  = 10'000'000;   // 10s in ERROR -> IDLE
constexpr int64_t kGameTimeoutUs    = 300'000'000;   // 5min game safety -> CONNECTED

}  // namespace domes::config
