/**
 * @file gameEngine.cpp
 * @brief Per-pod game logic FSM implementation
 */

#include "game/gameEngine.hpp"

// NVS is only available on device builds (not in host test builds)
#ifdef CONFIG_IDF_TARGET_ESP32S3
#include "infra/nvsConfig.hpp"
#endif
#include "esp_log.h"
#include "trace/traceApi.hpp"

namespace domes::game {

static constexpr const char* kTag = "game";

const char* gameStateToString(GameState state) {
    switch (state) {
        case GameState::kReady:
            return "READY";
        case GameState::kArmed:
            return "ARMED";
        case GameState::kTriggered:
            return "TRIGGERED";
        case GameState::kFeedback:
            return "FEEDBACK";
        default:
            return "UNKNOWN";
    }
}

GameEngine::GameEngine(ITouchDriver& touch) : touch_(touch) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    // Read pod ID from NVS for event tagging (device builds only)
    domes::infra::NvsConfig config;
    if (config.open(domes::infra::nvs_ns::kConfig) == ESP_OK) {
        podId_ = config.getOrDefault<uint8_t>(domes::infra::config_key::kPodId, 0);
        config.close();
    }
#endif
}

bool GameEngine::arm(const ArmConfig& config) {
    TRACE_SCOPE(TRACE_ID("Game.Arm"), domes::trace::Category::kGame);
    utils::MutexGuard guard(stateMutex_);
    if (state_ != GameState::kReady) {
        ESP_LOGW(kTag, "Cannot arm: state is %s", gameStateToString(state_));
        return false;
    }

    config_ = config;
    armedAtUs_ = esp_timer_get_time();
    state_ = GameState::kArmed;

    ESP_LOGI(kTag, "Armed (timeout: %lu ms, feedback: 0x%02x)",
             static_cast<unsigned long>(config_.timeoutMs), config_.feedbackMode);
    return true;
}

void GameEngine::disarm() {
    utils::MutexGuard guard(stateMutex_);
    if (state_ != GameState::kReady) {
        ESP_LOGI(kTag, "Disarm from %s", gameStateToString(state_));
    }
    state_ = GameState::kReady;
}

void GameEngine::tick() {
    PendingDispatch pending;
    {
        utils::MutexGuard guard(stateMutex_);
        switch (state_) {
            case GameState::kReady:
                break;
            case GameState::kArmed:
                handleArmed(pending);
                // Fall through to process kTriggered in same tick
                if (state_ != GameState::kTriggered) {
                    break;
                }
                [[fallthrough]];
            case GameState::kTriggered:
                handleTriggered(pending);
                break;
            case GameState::kFeedback:
                handleFeedback();
                break;
        }
    }

    dispatch(pending);
}

GameState GameEngine::currentState() const {
    utils::MutexGuard guard(stateMutex_);
    return state_;
}

uint32_t GameEngine::lastReactionTimeUs() const {
    utils::MutexGuard guard(stateMutex_);
    return lastReactionTimeUs_;
}

void GameEngine::setFeedbackCallbacks(FeedbackCallbacks callbacks) {
    utils::MutexGuard guard(callbackMutex_);
    feedbackCbs_ = std::move(callbacks);
}

void GameEngine::setEventCallback(GameEventCallback callback) {
    utils::MutexGuard guard(callbackMutex_);
    eventCb_ = std::move(callback);
}

void GameEngine::handleArmed(PendingDispatch& pending) {
    // Poll touch pads
    touch_.update();

    uint8_t padCount = touch_.getPadCount();
    for (uint8_t i = 0; i < padCount; ++i) {
        if (touch_.isTouched(i)) {
            int64_t now = esp_timer_get_time();
            uint32_t reactionUs = static_cast<uint32_t>(now - armedAtUs_);

            ESP_LOGI(kTag, "Touch on pad %u, reaction: %lu us", i,
                     static_cast<unsigned long>(reactionUs));

            TRACE_INSTANT(TRACE_ID("Game.TouchHit"), domes::trace::Category::kGame);
            // Store for handleTriggered to process
            triggeredPadIndex_ = i;
            triggeredReactionUs_ = reactionUs;
            lastReactionTimeUs_ = reactionUs;
            state_ = GameState::kTriggered;
            return;
        }
    }

    // Check timeout
    int64_t now = esp_timer_get_time();
    int64_t elapsedUs = now - armedAtUs_;
    int64_t timeoutUs = static_cast<int64_t>(config_.timeoutMs) * 1000;

    if (elapsedUs >= timeoutUs) {
        TRACE_INSTANT(TRACE_ID("Game.TouchMiss"), domes::trace::Category::kGame);
        ESP_LOGI(kTag, "Timeout — miss");
        enterFeedback(GameEvent::Type::kMiss, 0, 0, pending);
    }
}

void GameEngine::handleTriggered(PendingDispatch& pending) {
    TRACE_SCOPE(TRACE_ID("Game.HandleTriggered"), domes::trace::Category::kGame);
    // Auto-advance to feedback with hit
    enterFeedback(GameEvent::Type::kHit, triggeredReactionUs_, triggeredPadIndex_, pending);
}

void GameEngine::handleFeedback() {
    int64_t now = esp_timer_get_time();
    int64_t elapsedUs = now - feedbackAtUs_;

    if (elapsedUs >= static_cast<int64_t>(kFeedbackDurationMs) * 1000) {
        ESP_LOGI(kTag, "Feedback complete, returning to READY");
        state_ = GameState::kReady;
    }
}

void GameEngine::enterFeedback(GameEvent::Type type, uint32_t reactionTimeUs, uint8_t padIndex,
                               PendingDispatch& pending) {
    TRACE_SCOPE(TRACE_ID("Game.EnterFeedback"), domes::trace::Category::kGame);
    TRACE_COUNTER(TRACE_ID("Game.ReactionTimeUs"), reactionTimeUs, domes::trace::Category::kGame);
    feedbackAtUs_ = esp_timer_get_time();
    state_ = GameState::kFeedback;

    pending.active = true;
    pending.event = GameEvent{type, podId_, reactionTimeUs, padIndex};
    pending.feedbackMode = config_.feedbackMode;
}

void GameEngine::dispatch(const PendingDispatch& pending) {
    if (!pending.active) {
        return;
    }

    // State is unlocked so callbacks can safely query or disarm the engine.
    // Callback replacement waits for an in-flight callback to return.
    utils::MutexGuard guard(callbackMutex_);

    // Fire feedback callbacks based on type and mode
    if (pending.event.type == GameEvent::Type::kHit) {
        if ((pending.feedbackMode & kFeedbackLed) && feedbackCbs_.flashWhite) {
            feedbackCbs_.flashWhite(kFeedbackDurationMs);
        }
        if ((pending.feedbackMode & kFeedbackAudio) && feedbackCbs_.playSound) {
            feedbackCbs_.playSound("beep");
        }
    } else {
        // Miss: red flash, no sound
        if ((pending.feedbackMode & kFeedbackLed) && feedbackCbs_.flashColor) {
            feedbackCbs_.flashColor(Color::red(), kFeedbackDurationMs);
        }
    }

    // Emit game event
    if (eventCb_) {
        eventCb_(pending.event);
    }
}

}  // namespace domes::game
