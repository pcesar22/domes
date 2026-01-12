#pragma once

/**
 * @file featureManager.hpp
 * @brief Runtime feature toggle management
 *
 * Manages the enable/disable state of runtime-toggleable features.
 * Uses an atomic bitmask for thread-safe access from multiple tasks.
 */

#include "configProtocol.hpp"

#include <atomic>
#include <cstdint>

namespace domes::config {

/**
 * @brief Manages runtime feature toggles
 *
 * Thread-safe feature state management using atomic operations.
 * All features are enabled by default on startup.
 *
 * Usage:
 *   FeatureManager features;
 *   if (features.isEnabled(Feature::kLedEffects)) {
 *       // Run LED effects
 *   }
 *
 *   features.setEnabled(Feature::kBleAdvertising, false);  // Disable BLE
 */
class FeatureManager {
public:
    /**
     * @brief Construct feature manager with all features enabled
     */
    FeatureManager();

    /**
     * @brief Check if a feature is enabled
     *
     * Thread-safe, can be called from any task or ISR.
     *
     * @param feature Feature to check
     * @return true if enabled, false if disabled or invalid
     */
    bool isEnabled(Feature feature) const;

    /**
     * @brief Set feature enabled state
     *
     * Thread-safe, can be called from any task.
     *
     * @param feature Feature to modify
     * @param enabled true to enable, false to disable
     * @return true if set successfully, false if invalid feature
     */
    bool setEnabled(Feature feature, bool enabled);

    /**
     * @brief Get all feature states
     *
     * @param states Output array (must be at least kMaxFeatures elements)
     * @return Number of features written
     */
    size_t getAll(FeatureState* states) const;

    /**
     * @brief Get the raw enabled mask
     *
     * Each bit corresponds to a Feature enum value.
     * Bit N is set if Feature(N) is enabled.
     *
     * @return Bitmask of enabled features
     */
    uint32_t getMask() const;

    /**
     * @brief Set enabled mask directly
     *
     * Used for restoring state from storage.
     *
     * @param mask Bitmask of enabled features
     */
    void setMask(uint32_t mask);

private:
    /**
     * @brief Get bit position for a feature
     *
     * @param feature Feature enum
     * @return Bit position (0-31)
     */
    static uint8_t featureToBit(Feature feature);

    /**
     * @brief Check if feature is valid
     *
     * @param feature Feature to check
     * @return true if valid feature ID
     */
    static bool isValidFeature(Feature feature);

    /// Bitmask of enabled features (bit N = Feature(N))
    std::atomic<uint32_t> enabledMask_;
};

}  // namespace domes::config
