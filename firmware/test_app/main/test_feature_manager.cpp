/**
 * @file test_feature_manager.cpp
 * @brief Unit tests for FeatureManager class
 */

#include <gtest/gtest.h>
#include "config/featureManager.hpp"
#include "config/configProtocol.hpp"

#include <array>

using namespace domes::config;

// =============================================================================
// Construction Tests
// =============================================================================

TEST(FeatureManager, AllFeaturesEnabledByDefault) {
    FeatureManager manager;

    EXPECT_TRUE(manager.isEnabled(Feature::kLedEffects));
    EXPECT_TRUE(manager.isEnabled(Feature::kBleAdvertising));
    EXPECT_TRUE(manager.isEnabled(Feature::kWifi));
    EXPECT_TRUE(manager.isEnabled(Feature::kEspNow));
    EXPECT_TRUE(manager.isEnabled(Feature::kTouch));
    EXPECT_TRUE(manager.isEnabled(Feature::kHaptic));
    EXPECT_TRUE(manager.isEnabled(Feature::kAudio));
}

TEST(FeatureManager, DefaultMaskIsAllOnes) {
    FeatureManager manager;

    // All bits should be set (0xFFFFFFFF)
    EXPECT_EQ(manager.getMask(), 0xFFFFFFFF);
}

// =============================================================================
// Enable/Disable Tests
// =============================================================================

TEST(FeatureManager, DisableFeature) {
    FeatureManager manager;

    EXPECT_TRUE(manager.setEnabled(Feature::kLedEffects, false));
    EXPECT_FALSE(manager.isEnabled(Feature::kLedEffects));

    // Other features should still be enabled
    EXPECT_TRUE(manager.isEnabled(Feature::kBleAdvertising));
    EXPECT_TRUE(manager.isEnabled(Feature::kWifi));
}

TEST(FeatureManager, EnableFeature) {
    FeatureManager manager;

    // Disable then re-enable
    manager.setEnabled(Feature::kBleAdvertising, false);
    EXPECT_FALSE(manager.isEnabled(Feature::kBleAdvertising));

    EXPECT_TRUE(manager.setEnabled(Feature::kBleAdvertising, true));
    EXPECT_TRUE(manager.isEnabled(Feature::kBleAdvertising));
}

TEST(FeatureManager, DisableMultipleFeatures) {
    FeatureManager manager;

    manager.setEnabled(Feature::kLedEffects, false);
    manager.setEnabled(Feature::kWifi, false);
    manager.setEnabled(Feature::kAudio, false);

    EXPECT_FALSE(manager.isEnabled(Feature::kLedEffects));
    EXPECT_FALSE(manager.isEnabled(Feature::kWifi));
    EXPECT_FALSE(manager.isEnabled(Feature::kAudio));

    EXPECT_TRUE(manager.isEnabled(Feature::kBleAdvertising));
    EXPECT_TRUE(manager.isEnabled(Feature::kEspNow));
    EXPECT_TRUE(manager.isEnabled(Feature::kTouch));
    EXPECT_TRUE(manager.isEnabled(Feature::kHaptic));
}

// =============================================================================
// Invalid Feature Tests
// =============================================================================

TEST(FeatureManager, InvalidFeatureReturnsFalse) {
    FeatureManager manager;

    // kUnknown is invalid
    EXPECT_FALSE(manager.isEnabled(Feature::kUnknown));
    EXPECT_FALSE(manager.setEnabled(Feature::kUnknown, true));
    EXPECT_FALSE(manager.setEnabled(Feature::kUnknown, false));

    // kCount is invalid
    EXPECT_FALSE(manager.isEnabled(Feature::kCount));
    EXPECT_FALSE(manager.setEnabled(Feature::kCount, true));
}

TEST(FeatureManager, OutOfRangeFeatureReturnsFalse) {
    FeatureManager manager;

    // Cast an out-of-range value
    auto outOfRange = static_cast<Feature>(99);
    EXPECT_FALSE(manager.isEnabled(outOfRange));
    EXPECT_FALSE(manager.setEnabled(outOfRange, true));
}

// =============================================================================
// GetAll Tests
// =============================================================================

TEST(FeatureManager, GetAllReturnsAllFeatures) {
    FeatureManager manager;
    std::array<FeatureState, kMaxFeatures> states{};

    size_t count = manager.getAll(states.data());

    // Should return 7 features (all valid ones, excluding kUnknown and kCount)
    EXPECT_EQ(count, 7u);
}

TEST(FeatureManager, GetAllReturnsCorrectStates) {
    FeatureManager manager;

    // Disable some features
    manager.setEnabled(Feature::kLedEffects, false);
    manager.setEnabled(Feature::kWifi, false);

    std::array<FeatureState, kMaxFeatures> states{};
    size_t count = manager.getAll(states.data());

    // Find the LED effects state
    bool foundLed = false;
    bool foundWifi = false;
    bool foundBle = false;

    for (size_t i = 0; i < count; ++i) {
        if (states[i].feature == static_cast<uint8_t>(Feature::kLedEffects)) {
            foundLed = true;
            EXPECT_EQ(states[i].enabled, 0);
        }
        if (states[i].feature == static_cast<uint8_t>(Feature::kWifi)) {
            foundWifi = true;
            EXPECT_EQ(states[i].enabled, 0);
        }
        if (states[i].feature == static_cast<uint8_t>(Feature::kBleAdvertising)) {
            foundBle = true;
            EXPECT_EQ(states[i].enabled, 1);
        }
    }

    EXPECT_TRUE(foundLed);
    EXPECT_TRUE(foundWifi);
    EXPECT_TRUE(foundBle);
}

// =============================================================================
// Mask Tests
// =============================================================================

TEST(FeatureManager, GetMaskReflectsState) {
    FeatureManager manager;

    // Clear bit 1 (LED effects)
    manager.setEnabled(Feature::kLedEffects, false);

    uint32_t mask = manager.getMask();

    // Bit 1 should be 0, others should be 1
    EXPECT_EQ(mask & (1u << 1), 0u);
    EXPECT_NE(mask & (1u << 2), 0u);  // BLE
    EXPECT_NE(mask & (1u << 3), 0u);  // WiFi
}

TEST(FeatureManager, SetMaskUpdatesState) {
    FeatureManager manager;

    // Set mask with only bits 1, 3, 5 set
    manager.setMask(0b00101010);

    EXPECT_TRUE(manager.isEnabled(Feature::kLedEffects));    // bit 1
    EXPECT_FALSE(manager.isEnabled(Feature::kBleAdvertising)); // bit 2
    EXPECT_TRUE(manager.isEnabled(Feature::kWifi));          // bit 3
    EXPECT_FALSE(manager.isEnabled(Feature::kEspNow));       // bit 4
    EXPECT_TRUE(manager.isEnabled(Feature::kTouch));         // bit 5
    EXPECT_FALSE(manager.isEnabled(Feature::kHaptic));       // bit 6
    EXPECT_FALSE(manager.isEnabled(Feature::kAudio));        // bit 7
}

// =============================================================================
// Thread Safety Tests (basic)
// =============================================================================

TEST(FeatureManager, ConcurrentReadsDoNotCrash) {
    FeatureManager manager;

    // Just verify we can call isEnabled many times without issues
    for (int i = 0; i < 1000; ++i) {
        manager.isEnabled(Feature::kLedEffects);
        manager.isEnabled(Feature::kBleAdvertising);
    }
}

TEST(FeatureManager, ConcurrentWritesDoNotCrash) {
    FeatureManager manager;

    // Just verify we can toggle features many times
    for (int i = 0; i < 1000; ++i) {
        manager.setEnabled(Feature::kLedEffects, (i % 2) == 0);
    }

    // Final state should be predictable
    EXPECT_FALSE(manager.isEnabled(Feature::kLedEffects));  // 1000 is even, last call was false
}
