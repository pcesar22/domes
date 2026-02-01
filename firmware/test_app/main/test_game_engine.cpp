/**
 * @file test_game_engine.cpp
 * @brief Unit tests for GameEngine class
 *
 * Uses a MockTouchDriver and mock feedback callbacks to test the
 * game FSM: Ready -> Armed -> Triggered -> Feedback -> Ready.
 */

#include <gtest/gtest.h>
#include "esp_timer.h"  // Stub - provides test_stubs::mock_time_us
#include "game/gameEngine.hpp"

#include <string>
#include <vector>

using namespace domes::game;
using namespace domes;

// =============================================================================
// Mock Touch Driver
// =============================================================================

class MockTouchDriver : public ITouchDriver {
public:
    esp_err_t init() override { return ESP_OK; }

    esp_err_t update() override {
        updateCount_++;
        return ESP_OK;
    }

    bool isTouched(uint8_t padIndex) const override {
        if (padIndex < kPadCount) {
            return touchState_[padIndex];
        }
        return false;
    }

    TouchPadState getPadState(uint8_t padIndex) const override {
        TouchPadState state;
        if (padIndex < kPadCount) {
            state.touched = touchState_[padIndex];
        }
        return state;
    }

    uint8_t getPadCount() const override { return kPadCount; }

    esp_err_t calibrate() override { return ESP_OK; }

    // Test helpers
    void setTouched(uint8_t padIndex, bool touched) {
        if (padIndex < kPadCount) {
            touchState_[padIndex] = touched;
        }
    }

    void clearAll() {
        for (auto& s : touchState_) s = false;
    }

    int updateCount() const { return updateCount_; }

private:
    static constexpr uint8_t kPadCount = 4;
    bool touchState_[kPadCount] = {};
    int updateCount_ = 0;
};

// =============================================================================
// Test Fixture
// =============================================================================

class GameEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_stubs::mock_time_us.store(0);
        touch_ = std::make_unique<MockTouchDriver>();
        engine_ = std::make_unique<GameEngine>(*touch_);

        // Set up recording feedback callbacks
        flashWhiteCount_ = 0;
        flashColorCount_ = 0;
        playSoundCount_ = 0;
        lastFlashWhiteDurationMs_ = 0;
        lastFlashColor_ = Color::off();
        lastSoundName_.clear();
        events_.clear();

        engine_->setFeedbackCallbacks({
            .flashWhite = [this](uint32_t ms) {
                flashWhiteCount_++;
                lastFlashWhiteDurationMs_ = ms;
            },
            .flashColor = [this](Color c, uint32_t ms) {
                flashColorCount_++;
                lastFlashColor_ = c;
            },
            .playSound = [this](const char* name) {
                playSoundCount_++;
                lastSoundName_ = name;
            },
        });

        engine_->setEventCallback([this](const GameEvent& e) {
            events_.push_back(e);
        });
    }

    void advanceTimeUs(int64_t us) {
        test_stubs::mock_time_us.fetch_add(us);
    }

    void advanceTimeMs(int64_t ms) {
        advanceTimeUs(ms * 1000);
    }

    std::unique_ptr<MockTouchDriver> touch_;
    std::unique_ptr<GameEngine> engine_;

    // Feedback recording
    int flashWhiteCount_;
    int flashColorCount_;
    int playSoundCount_;
    uint32_t lastFlashWhiteDurationMs_;
    Color lastFlashColor_;
    std::string lastSoundName_;
    std::vector<GameEvent> events_;
};

// =============================================================================
// State Machine Basics
// =============================================================================

TEST_F(GameEngineTest, StartsInReadyState) {
    EXPECT_EQ(engine_->currentState(), GameState::kReady);
}

TEST_F(GameEngineTest, ArmTransitionsToArmed) {
    EXPECT_TRUE(engine_->arm());
    EXPECT_EQ(engine_->currentState(), GameState::kArmed);
}

TEST_F(GameEngineTest, ArmFailsWhenNotReady) {
    engine_->arm();
    EXPECT_EQ(engine_->currentState(), GameState::kArmed);

    // Try to arm again while already armed
    EXPECT_FALSE(engine_->arm());
    EXPECT_EQ(engine_->currentState(), GameState::kArmed);
}

TEST_F(GameEngineTest, DisarmFromArmed) {
    engine_->arm();
    engine_->disarm();
    EXPECT_EQ(engine_->currentState(), GameState::kReady);
}

TEST_F(GameEngineTest, DisarmFromReady) {
    // Disarm from Ready should be a no-op (stays Ready)
    engine_->disarm();
    EXPECT_EQ(engine_->currentState(), GameState::kReady);
}

TEST_F(GameEngineTest, DisarmFromFeedback) {
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();  // Armed -> Feedback (miss)
    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);

    engine_->disarm();
    EXPECT_EQ(engine_->currentState(), GameState::kReady);
}

// =============================================================================
// Hit Path (Ready -> Armed -> Triggered -> Feedback -> Ready)
// =============================================================================

TEST_F(GameEngineTest, TouchInArmedStateRecordsHit) {
    engine_->arm();
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();  // Armed -> Triggered

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);
}

TEST_F(GameEngineTest, HitReactionTimeIsCorrect) {
    engine_->arm();
    advanceTimeMs(150);  // 150ms = 150000us
    touch_->setTouched(2, true);
    engine_->tick();  // Detects touch, records reaction time
    engine_->tick();  // Triggered -> Feedback (if not already)

    EXPECT_EQ(engine_->lastReactionTimeUs(), 150'000u);
}

TEST_F(GameEngineTest, HitTriggersWhiteFlashAndSound) {
    engine_->arm();
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();  // Armed -> Triggered -> Feedback

    EXPECT_EQ(flashWhiteCount_, 1);
    EXPECT_EQ(lastFlashWhiteDurationMs_, kFeedbackDurationMs);
    EXPECT_EQ(playSoundCount_, 1);
    EXPECT_EQ(lastSoundName_, "beep");
}

TEST_F(GameEngineTest, HitEmitsGameEvent) {
    engine_->arm();
    advanceTimeMs(100);
    touch_->setTouched(1, true);
    engine_->tick();

    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].type, GameEvent::Type::kHit);
    EXPECT_EQ(events_[0].reactionTimeUs, 100'000u);
    EXPECT_EQ(events_[0].padIndex, 1);
}

TEST_F(GameEngineTest, FeedbackCompletesAfterDuration) {
    engine_->arm();
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();  // -> Feedback

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);

    // Advance past feedback duration
    advanceTimeMs(kFeedbackDurationMs + 1);
    engine_->tick();

    EXPECT_EQ(engine_->currentState(), GameState::kReady);
}

TEST_F(GameEngineTest, MultiPadDetectionFirstWins) {
    engine_->arm();
    advanceTimeMs(50);

    // Multiple pads touched simultaneously - first one (index 0) wins
    touch_->setTouched(0, true);
    touch_->setTouched(2, true);
    engine_->tick();

    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].padIndex, 0);  // First pad wins
}

// =============================================================================
// Miss Path (Ready -> Armed -> Feedback -> Ready)
// =============================================================================

TEST_F(GameEngineTest, TimeoutInArmedStateRecordsMiss) {
    engine_->arm({.timeoutMs = 500});
    advanceTimeMs(501);
    engine_->tick();

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);
}

TEST_F(GameEngineTest, MissTriggersRedFlash) {
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();

    EXPECT_EQ(flashColorCount_, 1);
    EXPECT_EQ(lastFlashColor_.r, 255);
    EXPECT_EQ(lastFlashColor_.g, 0);
    EXPECT_EQ(lastFlashColor_.b, 0);
}

TEST_F(GameEngineTest, MissEmitsGameEvent) {
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();

    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].type, GameEvent::Type::kMiss);
    EXPECT_EQ(events_[0].reactionTimeUs, 0u);
    EXPECT_EQ(events_[0].padIndex, 0);
}

TEST_F(GameEngineTest, MissNoSound) {
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();

    EXPECT_EQ(playSoundCount_, 0);
}

// =============================================================================
// Feedback Modes
// =============================================================================

TEST_F(GameEngineTest, FeedbackModeNoneSkipsFlashAndSound) {
    engine_->arm({.timeoutMs = 3000, .feedbackMode = 0x00});
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();

    EXPECT_EQ(flashWhiteCount_, 0);
    EXPECT_EQ(playSoundCount_, 0);
    // Event should still be emitted
    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].type, GameEvent::Type::kHit);
}

TEST_F(GameEngineTest, FeedbackModeAudioOnlySkipsLed) {
    engine_->arm({.timeoutMs = 3000, .feedbackMode = kFeedbackAudio});
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();

    EXPECT_EQ(flashWhiteCount_, 0);
    EXPECT_EQ(playSoundCount_, 1);
}

TEST_F(GameEngineTest, FeedbackModeLedOnlySkipsAudio) {
    engine_->arm({.timeoutMs = 3000, .feedbackMode = kFeedbackLed});
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();

    EXPECT_EQ(flashWhiteCount_, 1);
    EXPECT_EQ(playSoundCount_, 0);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(GameEngineTest, MultipleArmDisarmCycles) {
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(engine_->arm());
        EXPECT_EQ(engine_->currentState(), GameState::kArmed);
        engine_->disarm();
        EXPECT_EQ(engine_->currentState(), GameState::kReady);
    }
}

TEST_F(GameEngineTest, TouchAfterTimeoutIgnored) {
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();  // Timeout -> Feedback (miss)

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);

    // Touch during feedback should be ignored
    touch_->setTouched(0, true);
    engine_->tick();

    // Should still be in feedback (no second event)
    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].type, GameEvent::Type::kMiss);
}

TEST_F(GameEngineTest, TouchDuringFeedbackIgnored) {
    engine_->arm();
    advanceTimeMs(50);
    touch_->setTouched(0, true);
    engine_->tick();  // Hit -> Feedback

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);
    size_t eventCount = events_.size();

    // Touch another pad during feedback
    touch_->setTouched(1, true);
    advanceTimeMs(10);
    engine_->tick();

    // No new events
    EXPECT_EQ(events_.size(), eventCount);
}

TEST_F(GameEngineTest, DisarmDuringArmed) {
    engine_->arm();
    advanceTimeMs(50);
    engine_->disarm();

    EXPECT_EQ(engine_->currentState(), GameState::kReady);
    EXPECT_EQ(events_.size(), 0u);  // No event emitted
}

TEST_F(GameEngineTest, ZeroTimeoutImmediateMiss) {
    engine_->arm({.timeoutMs = 0});
    engine_->tick();

    EXPECT_EQ(engine_->currentState(), GameState::kFeedback);
    ASSERT_EQ(events_.size(), 1u);
    EXPECT_EQ(events_[0].type, GameEvent::Type::kMiss);
}

TEST_F(GameEngineTest, FullCycleHitThenRearm) {
    // First cycle: hit
    engine_->arm();
    advanceTimeMs(100);
    touch_->setTouched(0, true);
    engine_->tick();  // -> Feedback

    // Complete feedback
    touch_->clearAll();
    advanceTimeMs(kFeedbackDurationMs + 1);
    engine_->tick();  // -> Ready

    EXPECT_EQ(engine_->currentState(), GameState::kReady);

    // Second cycle: arm again
    EXPECT_TRUE(engine_->arm());
    EXPECT_EQ(engine_->currentState(), GameState::kArmed);
}

TEST_F(GameEngineTest, FullCycleMissThenRearm) {
    // First cycle: miss
    engine_->arm({.timeoutMs = 100});
    advanceTimeMs(200);
    engine_->tick();  // -> Feedback (miss)

    // Complete feedback
    advanceTimeMs(kFeedbackDurationMs + 1);
    engine_->tick();  // -> Ready

    EXPECT_EQ(engine_->currentState(), GameState::kReady);

    // Second cycle
    EXPECT_TRUE(engine_->arm());
    EXPECT_EQ(engine_->currentState(), GameState::kArmed);
}

TEST_F(GameEngineTest, TickInReadyIsNoop) {
    // Ticking in Ready state should do nothing
    for (int i = 0; i < 10; ++i) {
        engine_->tick();
    }
    EXPECT_EQ(engine_->currentState(), GameState::kReady);
    EXPECT_EQ(events_.size(), 0u);
    EXPECT_EQ(touch_->updateCount(), 0);  // Touch not polled in Ready
}

TEST_F(GameEngineTest, TouchUpdateCalledInArmed) {
    engine_->arm();
    engine_->tick();
    engine_->tick();
    engine_->tick();

    EXPECT_EQ(touch_->updateCount(), 3);
}

// =============================================================================
// gameStateToString Tests
// =============================================================================

TEST_F(GameEngineTest, StateToString) {
    EXPECT_STREQ(gameStateToString(GameState::kReady), "READY");
    EXPECT_STREQ(gameStateToString(GameState::kArmed), "ARMED");
    EXPECT_STREQ(gameStateToString(GameState::kTriggered), "TRIGGERED");
    EXPECT_STREQ(gameStateToString(GameState::kFeedback), "FEEDBACK");
}
