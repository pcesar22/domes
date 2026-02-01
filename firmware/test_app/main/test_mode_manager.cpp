/**
 * @file test_mode_manager.cpp
 * @brief Unit tests for ModeManager class
 *
 * Uses mock esp_timer_get_time() to control time for timeout tests.
 */

#include <gtest/gtest.h>
#include "esp_timer.h"  // Stub - provides test_stubs::mock_time_us
#include "config/modeManager.hpp"
#include "config/featureManager.hpp"

using namespace domes::config;

// =============================================================================
// Test Fixture
// =============================================================================

class ModeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_stubs::mock_time_us.store(0);
        features_ = std::make_unique<FeatureManager>();
        mgr_ = std::make_unique<ModeManager>(*features_);
    }

    void advanceTimeUs(int64_t us) {
        test_stubs::mock_time_us.fetch_add(us);
    }

    void advanceTimeMs(int64_t ms) {
        advanceTimeUs(ms * 1000);
    }

    void advanceTimeS(int64_t s) {
        advanceTimeUs(s * 1'000'000);
    }

    std::unique_ptr<FeatureManager> features_;
    std::unique_ptr<ModeManager> mgr_;
};

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(ModeManagerTest, StartsInBootingMode) {
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kBooting);
}

TEST_F(ModeManagerTest, BootingFeatureMaskIsZero) {
    EXPECT_EQ(ModeManager::featureMaskForMode(SystemMode::kBooting), 0u);
}

// =============================================================================
// Feature Mask Tests
// =============================================================================

TEST_F(ModeManagerTest, IdleMaskIsLedAndBle) {
    uint32_t mask = ModeManager::featureMaskForMode(SystemMode::kIdle);
    // bit(1) = LED, bit(2) = BLE
    EXPECT_EQ(mask, (1u << 1) | (1u << 2));
}

TEST_F(ModeManagerTest, TriageMaskExcludesEspNow) {
    uint32_t mask = ModeManager::featureMaskForMode(SystemMode::kTriage);
    // Should have LED(1), BLE(2), WiFi(3), Touch(5), Haptic(6), Audio(7)
    // Should NOT have ESP-NOW(4)
    EXPECT_NE(mask & (1u << 1), 0u);  // LED
    EXPECT_NE(mask & (1u << 2), 0u);  // BLE
    EXPECT_NE(mask & (1u << 3), 0u);  // WiFi
    EXPECT_EQ(mask & (1u << 4), 0u);  // NO ESP-NOW
    EXPECT_NE(mask & (1u << 5), 0u);  // Touch
    EXPECT_NE(mask & (1u << 6), 0u);  // Haptic
    EXPECT_NE(mask & (1u << 7), 0u);  // Audio
}

TEST_F(ModeManagerTest, ConnectedMaskExcludesWifi) {
    uint32_t mask = ModeManager::featureMaskForMode(SystemMode::kConnected);
    EXPECT_NE(mask & (1u << 1), 0u);  // LED
    EXPECT_NE(mask & (1u << 2), 0u);  // BLE
    EXPECT_EQ(mask & (1u << 3), 0u);  // NO WiFi
    EXPECT_NE(mask & (1u << 4), 0u);  // ESP-NOW
    EXPECT_NE(mask & (1u << 5), 0u);  // Touch
    EXPECT_NE(mask & (1u << 6), 0u);  // Haptic
    EXPECT_NE(mask & (1u << 7), 0u);  // Audio
}

TEST_F(ModeManagerTest, GameMaskSameAsConnected) {
    EXPECT_EQ(ModeManager::featureMaskForMode(SystemMode::kGame),
              ModeManager::featureMaskForMode(SystemMode::kConnected));
}

TEST_F(ModeManagerTest, ErrorMaskIsLedAndBle) {
    uint32_t mask = ModeManager::featureMaskForMode(SystemMode::kError);
    EXPECT_EQ(mask, (1u << 1) | (1u << 2));
}

// =============================================================================
// Valid Transition Tests
// =============================================================================

TEST_F(ModeManagerTest, BootingToIdle) {
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kIdle));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, IdleToTriage) {
    mgr_->transitionTo(SystemMode::kIdle);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kTriage));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kTriage);
}

TEST_F(ModeManagerTest, IdleToConnected) {
    mgr_->transitionTo(SystemMode::kIdle);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kConnected));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kConnected);
}

TEST_F(ModeManagerTest, TriageToConnected) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kConnected));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kConnected);
}

TEST_F(ModeManagerTest, ConnectedToGame) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kGame));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kGame);
}

TEST_F(ModeManagerTest, GameToConnected) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    mgr_->transitionTo(SystemMode::kGame);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kConnected));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kConnected);
}

TEST_F(ModeManagerTest, ConnectedToTriage) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kTriage));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kTriage);
}

TEST_F(ModeManagerTest, AnyModeToError) {
    // From BOOTING
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kError));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kError);

    // Reset to IDLE
    mgr_->transitionTo(SystemMode::kIdle);

    // From IDLE
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kError));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kError);
}

TEST_F(ModeManagerTest, AnyModeToIdle) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);

    // From TRIAGE to IDLE
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kIdle));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, ErrorToIdle) {
    mgr_->transitionTo(SystemMode::kError);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kIdle));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, SameModeTransitionSucceeds) {
    mgr_->transitionTo(SystemMode::kIdle);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kIdle));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

// =============================================================================
// Invalid Transition Tests
// =============================================================================

TEST_F(ModeManagerTest, BootingToTriageInvalid) {
    EXPECT_FALSE(mgr_->transitionTo(SystemMode::kTriage));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kBooting);
}

TEST_F(ModeManagerTest, BootingToConnectedInvalid) {
    EXPECT_FALSE(mgr_->transitionTo(SystemMode::kConnected));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kBooting);
}

TEST_F(ModeManagerTest, BootingToGameInvalid) {
    EXPECT_FALSE(mgr_->transitionTo(SystemMode::kGame));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kBooting);
}

TEST_F(ModeManagerTest, SoloDrillIdleToGame) {
    mgr_->transitionTo(SystemMode::kIdle);
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kGame));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kGame);
}

TEST_F(ModeManagerTest, TriageToGameInvalid) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);
    EXPECT_FALSE(mgr_->transitionTo(SystemMode::kGame));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kTriage);
}

TEST_F(ModeManagerTest, GameToTriageInvalid) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    mgr_->transitionTo(SystemMode::kGame);
    EXPECT_FALSE(mgr_->transitionTo(SystemMode::kTriage));
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kGame);
}

// =============================================================================
// Feature Mask Application Tests
// =============================================================================

TEST_F(ModeManagerTest, TransitionAppliesFeatureMask) {
    mgr_->transitionTo(SystemMode::kIdle);

    // IDLE mask: LED(1) + BLE(2)
    EXPECT_TRUE(features_->isEnabled(Feature::kLedEffects));
    EXPECT_TRUE(features_->isEnabled(Feature::kBleAdvertising));
    EXPECT_FALSE(features_->isEnabled(Feature::kWifi));
    EXPECT_FALSE(features_->isEnabled(Feature::kEspNow));
}

TEST_F(ModeManagerTest, TriageMaskEnablesWifi) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);

    EXPECT_TRUE(features_->isEnabled(Feature::kWifi));
    EXPECT_FALSE(features_->isEnabled(Feature::kEspNow));
}

TEST_F(ModeManagerTest, ConnectedMaskEnablesEspNow) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);

    EXPECT_FALSE(features_->isEnabled(Feature::kWifi));
    EXPECT_TRUE(features_->isEnabled(Feature::kEspNow));
}

// =============================================================================
// Timeout Tests
// =============================================================================

TEST_F(ModeManagerTest, TriageTimeoutToIdle) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);

    // Advance past the 30s timeout
    advanceTimeS(31);
    mgr_->tick();

    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, TriageActivityResetsTimeout) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kTriage);

    // Advance 20s
    advanceTimeS(20);
    mgr_->tick();
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kTriage);

    // Reset activity timer
    mgr_->resetActivityTimer();

    // Advance another 20s (40s total, but only 20s since last activity)
    advanceTimeS(20);
    mgr_->tick();
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kTriage);

    // Advance past timeout from last activity
    advanceTimeS(11);
    mgr_->tick();
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, ErrorRecoveryTimeout) {
    mgr_->transitionTo(SystemMode::kError);

    // Advance past the 10s error recovery timeout
    advanceTimeS(11);
    mgr_->tick();

    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, PeerDrillTimeoutToConnected) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    mgr_->transitionTo(SystemMode::kGame);

    // Advance past the 5 min (300s) timeout
    advanceTimeS(301);
    mgr_->tick();

    // Entered GAME from CONNECTED, so timeout returns to CONNECTED
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kConnected);
}

TEST_F(ModeManagerTest, IdleNoTimeout) {
    mgr_->transitionTo(SystemMode::kIdle);

    // Even after a long time, IDLE should remain
    advanceTimeS(3600);
    mgr_->tick();

    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, ConnectedNoTimeout) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);

    // CONNECTED should not time out
    advanceTimeS(3600);
    mgr_->tick();

    EXPECT_EQ(mgr_->currentMode(), SystemMode::kConnected);
}

// =============================================================================
// systemModeToString Tests
// =============================================================================

TEST_F(ModeManagerTest, ModeToString) {
    EXPECT_STREQ(systemModeToString(SystemMode::kBooting), "BOOTING");
    EXPECT_STREQ(systemModeToString(SystemMode::kIdle), "IDLE");
    EXPECT_STREQ(systemModeToString(SystemMode::kTriage), "TRIAGE");
    EXPECT_STREQ(systemModeToString(SystemMode::kConnected), "CONNECTED");
    EXPECT_STREQ(systemModeToString(SystemMode::kGame), "GAME");
    EXPECT_STREQ(systemModeToString(SystemMode::kError), "ERROR");
}

// =============================================================================
// Game Entry Tracking Tests (gameEnteredFrom_)
// =============================================================================

TEST_F(ModeManagerTest, GameEnteredFromTracksConnected) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    mgr_->transitionTo(SystemMode::kGame);

    EXPECT_EQ(mgr_->gameEnteredFrom(), SystemMode::kConnected);
}

TEST_F(ModeManagerTest, GameEnteredFromTracksIdle) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kGame);  // solo drill

    EXPECT_EQ(mgr_->gameEnteredFrom(), SystemMode::kIdle);
}

TEST_F(ModeManagerTest, SoloDrillTimeoutToIdle) {
    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kGame);  // solo drill from IDLE

    // Advance past the 5 min (300s) timeout
    advanceTimeS(301);
    mgr_->tick();

    // Entered GAME from IDLE, so timeout returns to IDLE
    EXPECT_EQ(mgr_->currentMode(), SystemMode::kIdle);
}

// =============================================================================
// Mode Transition Callback Tests
// =============================================================================

TEST_F(ModeManagerTest, TransitionCallbackInvoked) {
    SystemMode cbFrom = SystemMode::kError;
    SystemMode cbTo = SystemMode::kError;
    int callCount = 0;

    mgr_->onTransition([&](SystemMode from, SystemMode to) {
        cbFrom = from;
        cbTo = to;
        callCount++;
    });

    mgr_->transitionTo(SystemMode::kIdle);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(cbFrom, SystemMode::kBooting);
    EXPECT_EQ(cbTo, SystemMode::kIdle);
}

TEST_F(ModeManagerTest, TransitionCallbackCalledOnEachTransition) {
    int callCount = 0;
    mgr_->onTransition([&](SystemMode, SystemMode) {
        callCount++;
    });

    mgr_->transitionTo(SystemMode::kIdle);
    mgr_->transitionTo(SystemMode::kConnected);
    mgr_->transitionTo(SystemMode::kGame);

    EXPECT_EQ(callCount, 3);
}

TEST_F(ModeManagerTest, TransitionCallbackNotCalledOnInvalid) {
    int callCount = 0;
    mgr_->onTransition([&](SystemMode, SystemMode) {
        callCount++;
    });

    // BOOTING -> GAME is invalid
    mgr_->transitionTo(SystemMode::kGame);

    EXPECT_EQ(callCount, 0);
}

TEST_F(ModeManagerTest, TransitionCallbackNotCalledOnSameMode) {
    mgr_->transitionTo(SystemMode::kIdle);

    int callCount = 0;
    mgr_->onTransition([&](SystemMode, SystemMode) {
        callCount++;
    });

    // Same-mode transition returns true but doesn't fire callback
    EXPECT_TRUE(mgr_->transitionTo(SystemMode::kIdle));
    EXPECT_EQ(callCount, 0);
}
