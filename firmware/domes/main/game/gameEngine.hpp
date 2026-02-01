#pragma once

/**
 * @file gameEngine.hpp
 * @brief Per-pod game logic FSM for the arm-touch-feedback cycle
 *
 * GameEngine manages the state machine within SystemMode::GAME:
 *   Ready -> Armed -> Triggered -> Feedback -> Ready
 *
 * Pure logic class with no FreeRTOS dependencies. Uses polling-based
 * touch detection and std::function callbacks for feedback.
 */

#include "interfaces/iLedDriver.hpp"
#include "interfaces/iTouchDriver.hpp"

#include "esp_timer.h"

#include <cstdint>
#include <functional>

namespace domes::game {

/**
 * @brief Game FSM states
 */
enum class GameState : uint8_t {
    kReady,      ///< Idle, waiting for arm() command
    kArmed,      ///< Armed and polling touch, timing out
    kTriggered,  ///< Touch detected, transitioning to feedback
    kFeedback,   ///< Playing feedback (flash/sound), waiting for duration
};

/**
 * @brief Get human-readable name for a game state
 */
const char* gameStateToString(GameState state);

/**
 * @brief Configuration for an arm cycle
 */
struct ArmConfig {
    uint32_t timeoutMs = 3000;    ///< Time before miss (0 = immediate miss)
    uint8_t feedbackMode = 0x03;  ///< Bitmask: 1=LED, 2=audio
};

/// Feedback mode bitmask constants
constexpr uint8_t kFeedbackLed   = 0x01;
constexpr uint8_t kFeedbackAudio = 0x02;

/**
 * @brief Game event emitted on hit or miss
 */
struct GameEvent {
    enum class Type : uint8_t {
        kHit,
        kMiss,
    };

    Type type;
    uint32_t reactionTimeUs;  ///< Microseconds from arm to touch (0 for miss)
    uint8_t padIndex;         ///< Which pad was touched (0 for miss)
};

/**
 * @brief Feedback action callbacks (set by main.cpp, recorded by tests)
 */
struct FeedbackCallbacks {
    std::function<void(uint32_t durationMs)> flashWhite;
    std::function<void(Color color, uint32_t durationMs)> flashColor;
    std::function<void(const char* name)> playSound;
};

using GameEventCallback = std::function<void(const GameEvent&)>;

/// Duration of feedback state before returning to Ready
constexpr uint32_t kFeedbackDurationMs = 200;

/**
 * @brief Per-pod game logic FSM
 *
 * Manages the arm-touch-feedback cycle. Call tick() at ~10ms intervals
 * from a game task. Pure logic — no FreeRTOS, no hardware dependencies
 * beyond ITouchDriver.
 *
 * @code
 * GameEngine engine(touchDriver);
 * engine.setFeedbackCallbacks({...});
 * engine.setEventCallback([](const GameEvent& e) { ... });
 * engine.arm({.timeoutMs = 3000});
 * // In game loop:
 * engine.tick();
 * @endcode
 */
class GameEngine {
public:
    /**
     * @brief Construct game engine
     * @param touch Touch driver for polling pad state
     */
    explicit GameEngine(ITouchDriver& touch);

    /**
     * @brief Arm the game engine (Ready -> Armed)
     *
     * Records arm timestamp and begins polling touch.
     *
     * @param config Arm configuration (timeout, feedback mode)
     * @return true if armed successfully (was in Ready state)
     */
    bool arm(const ArmConfig& config = {});

    /**
     * @brief Force disarm from any state -> Ready
     */
    void disarm();

    /**
     * @brief Advance the state machine
     *
     * Call at ~10ms intervals. Polls touch, checks timeouts,
     * and tracks feedback duration.
     */
    void tick();

    /**
     * @brief Get current FSM state
     */
    GameState currentState() const { return state_; }

    /**
     * @brief Get reaction time of last hit (microseconds)
     *
     * Only valid after a hit event. Returns 0 if no hit recorded.
     */
    uint32_t lastReactionTimeUs() const { return lastReactionTimeUs_; }

    /**
     * @brief Set feedback action callbacks
     */
    void setFeedbackCallbacks(FeedbackCallbacks callbacks);

    /**
     * @brief Set game event callback (hit/miss notifications)
     */
    void setEventCallback(GameEventCallback callback);

private:
    void handleArmed();
    void handleTriggered();
    void handleFeedback();
    void enterFeedback(GameEvent::Type type, uint32_t reactionTimeUs, uint8_t padIndex);

    ITouchDriver& touch_;
    GameState state_ = GameState::kReady;
    ArmConfig config_;

    int64_t armedAtUs_ = 0;       ///< esp_timer_get_time() when arm() called
    int64_t feedbackAtUs_ = 0;    ///< esp_timer_get_time() when feedback started
    uint32_t lastReactionTimeUs_ = 0;

    // Pending triggered event (from kTriggered -> kFeedback)
    uint8_t triggeredPadIndex_ = 0;
    uint32_t triggeredReactionUs_ = 0;

    FeedbackCallbacks feedbackCbs_;
    GameEventCallback eventCb_;
};

}  // namespace domes::game
