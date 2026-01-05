# 05 - Game Engine

## AI Agent Instructions

Load this file when:
- Implementing game state machine
- Creating drill types
- Handling timing and synchronization
- Managing pod coordination

Prerequisites: `03-driver-development.md`, `04-communication.md`

---

## State Machine

### Pod States

```
┌─────────────────────────────────────────────────────────────┐
│                      POD STATE MACHINE                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│    ┌────────────┐                                            │
│    │Initializing│                                            │
│    └─────┬──────┘                                            │
│          │ init complete                                     │
│          ▼                                                   │
│    ┌────────────┐  BLE connect   ┌────────────┐              │
│    │    Idle    │───────────────►│ Connected  │              │
│    └─────┬──────┘                └─────┬──────┘              │
│          │ long press                  │ START_DRILL         │
│          ▼                             ▼                     │
│    ┌────────────┐              ┌────────────┐                │
│    │ Standalone │              │   Armed    │◄───────┐       │
│    └────────────┘              └─────┬──────┘        │       │
│                                      │ touch         │       │
│                                      ▼               │       │
│                                ┌────────────┐        │       │
│                                │ Triggered  │        │       │
│                                └─────┬──────┘        │       │
│                                      │ send event    │       │
│                                      ▼               │       │
│                                ┌────────────┐        │       │
│                                │  Feedback  │────────┘       │
│                                └────────────┘  complete      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### State Implementation

```cpp
// game/state_machine.hpp
#pragma once

#include <cstdint>
#include <functional>

namespace domes {

enum class PodState : uint8_t {
    kInitializing,
    kIdle,
    kConnecting,
    kConnected,
    kArmed,
    kTriggered,
    kFeedback,
    kStandalone,
    kError,
};

const char* stateToString(PodState state);

struct StateContext {
    uint32_t enteredAtUs;    // When we entered this state
    uint32_t timeoutMs;      // State timeout (0 = no timeout)
    uint8_t retryCount;      // For retry logic
};

using StateHandler = std::function<void(StateContext&)>;
using TransitionCallback = std::function<void(PodState from, PodState to)>;

class StateMachine {
public:
    explicit StateMachine(PodState initialState = PodState::kInitializing);

    /**
     * @brief Transition to new state
     * @param newState Target state
     * @param timeoutMs Optional timeout for new state
     * @return true if transition allowed
     */
    bool transitionTo(PodState newState, uint32_t timeoutMs = 0);

    /**
     * @brief Get current state
     */
    PodState currentState() const { return state_; }

    /**
     * @brief Get time in current state (microseconds)
     */
    uint32_t timeInStateUs() const;

    /**
     * @brief Check if state has timed out
     */
    bool isTimedOut() const;

    /**
     * @brief Register state entry handler
     */
    void onEnter(PodState state, StateHandler handler);

    /**
     * @brief Register state exit handler
     */
    void onExit(PodState state, StateHandler handler);

    /**
     * @brief Register any transition callback
     */
    void onTransition(TransitionCallback callback);

    /**
     * @brief Process state (call in main loop)
     */
    void tick();

private:
    PodState state_;
    StateContext context_{};
    TransitionCallback transitionCb_;

    static constexpr size_t kStateCount = 9;
    StateHandler enterHandlers_[kStateCount]{};
    StateHandler exitHandlers_[kStateCount]{};

    bool isValidTransition(PodState from, PodState to) const;
};

}  // namespace domes
```

```cpp
// game/state_machine.cpp
#include "state_machine.hpp"
#include "esp_timer.h"
#include "esp_log.h"

namespace domes {

namespace {
    constexpr const char* kTag = "state";

    constexpr const char* kStateNames[] = {
        "Initializing", "Idle", "Connecting", "Connected",
        "Armed", "Triggered", "Feedback", "Standalone", "Error"
    };
}

const char* stateToString(PodState state) {
    return kStateNames[static_cast<int>(state)];
}

StateMachine::StateMachine(PodState initialState)
    : state_(initialState) {
    context_.enteredAtUs = esp_timer_get_time();
}

bool StateMachine::transitionTo(PodState newState, uint32_t timeoutMs) {
    if (!isValidTransition(state_, newState)) {
        ESP_LOGW(kTag, "Invalid transition: %s -> %s",
                 stateToString(state_), stateToString(newState));
        return false;
    }

    PodState oldState = state_;

    // Exit current state
    auto exitIdx = static_cast<size_t>(oldState);
    if (exitHandlers_[exitIdx]) {
        exitHandlers_[exitIdx](context_);
    }

    // Update state
    state_ = newState;
    context_.enteredAtUs = esp_timer_get_time();
    context_.timeoutMs = timeoutMs;
    context_.retryCount = 0;

    ESP_LOGI(kTag, "State: %s -> %s", stateToString(oldState), stateToString(newState));

    // Notify callback
    if (transitionCb_) {
        transitionCb_(oldState, newState);
    }

    // Enter new state
    auto enterIdx = static_cast<size_t>(newState);
    if (enterHandlers_[enterIdx]) {
        enterHandlers_[enterIdx](context_);
    }

    return true;
}

uint32_t StateMachine::timeInStateUs() const {
    return esp_timer_get_time() - context_.enteredAtUs;
}

bool StateMachine::isTimedOut() const {
    if (context_.timeoutMs == 0) return false;
    return timeInStateUs() > (context_.timeoutMs * 1000);
}

void StateMachine::onEnter(PodState state, StateHandler handler) {
    enterHandlers_[static_cast<size_t>(state)] = handler;
}

void StateMachine::onExit(PodState state, StateHandler handler) {
    exitHandlers_[static_cast<size_t>(state)] = handler;
}

void StateMachine::onTransition(TransitionCallback callback) {
    transitionCb_ = callback;
}

void StateMachine::tick() {
    if (isTimedOut()) {
        ESP_LOGW(kTag, "State %s timed out", stateToString(state_));
        // Handle timeout based on current state
        switch (state_) {
            case PodState::kConnecting:
                transitionTo(PodState::kIdle);
                break;
            case PodState::kArmed:
                transitionTo(PodState::kConnected);
                break;
            default:
                break;
        }
    }
}

bool StateMachine::isValidTransition(PodState from, PodState to) const {
    // Define valid transitions
    switch (from) {
        case PodState::kInitializing:
            return to == PodState::kIdle || to == PodState::kError;
        case PodState::kIdle:
            return to == PodState::kConnecting || to == PodState::kStandalone;
        case PodState::kConnecting:
            return to == PodState::kConnected || to == PodState::kIdle;
        case PodState::kConnected:
            return to == PodState::kArmed || to == PodState::kIdle;
        case PodState::kArmed:
            return to == PodState::kTriggered || to == PodState::kConnected;
        case PodState::kTriggered:
            return to == PodState::kFeedback;
        case PodState::kFeedback:
            return to == PodState::kArmed || to == PodState::kConnected;
        case PodState::kStandalone:
            return to == PodState::kArmed || to == PodState::kIdle;
        case PodState::kError:
            return to == PodState::kIdle;
    }
    return false;
}

}  // namespace domes
```

---

## Game Engine

```cpp
// game/game_engine.hpp
#pragma once

#include "state_machine.hpp"
#include "interfaces/i_touch_driver.hpp"
#include "services/feedback_service.hpp"
#include "services/comm_service.hpp"
#include <memory>

namespace domes {

struct DrillConfig {
    uint16_t timeoutMs = 5000;       // Max reaction time
    uint8_t feedbackMode = 3;        // 0=none, 1=audio, 2=haptic, 3=all
    bool randomDelay = true;         // Random delay before arming
    uint16_t minDelayMs = 500;       // Min random delay
    uint16_t maxDelayMs = 2000;      // Max random delay
};

class GameEngine {
public:
    GameEngine(ITouchDriver& touch,
               FeedbackService& feedback,
               CommService& comm);

    esp_err_t init();

    /**
     * @brief Start a drill sequence
     * @param config Drill configuration
     */
    esp_err_t startDrill(const DrillConfig& config);

    /**
     * @brief Stop current drill
     */
    esp_err_t stopDrill();

    /**
     * @brief Get current state
     */
    PodState currentState() const { return stateMachine_.currentState(); }

    /**
     * @brief Process game logic (call from game task)
     */
    void tick();

    /**
     * @brief Handle touch event
     */
    void onTouch(const TouchEvent& event);

    /**
     * @brief Get last reaction time (microseconds)
     */
    uint32_t lastReactionTimeUs() const { return lastReactionTimeUs_; }

private:
    ITouchDriver& touch_;
    FeedbackService& feedback_;
    CommService& comm_;
    StateMachine stateMachine_;
    DrillConfig currentConfig_;

    uint32_t armedAtUs_ = 0;
    uint32_t lastReactionTimeUs_ = 0;

    void setupStateHandlers();
    void sendTouchEvent(uint32_t reactionTimeUs, uint8_t strength);
};

}  // namespace domes
```

---

## Timing Synchronization

### Clock Sync Protocol

```cpp
// services/timing_service.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

class TimingService {
public:
    /**
     * @brief Get synchronized timestamp (microseconds)
     *
     * Returns local time adjusted by master offset.
     */
    uint32_t getSyncedTimeUs() const;

    /**
     * @brief Handle clock sync message from master
     * @param masterTimeUs Master's timestamp when message was sent
     */
    void onSyncMessage(uint32_t masterTimeUs);

    /**
     * @brief Get current offset from master (microseconds)
     *
     * Positive = we are ahead of master
     * Negative = we are behind master
     */
    int32_t getOffsetUs() const { return offsetUs_; }

    /**
     * @brief Check if synchronized with master
     */
    bool isSynced() const { return synced_; }

private:
    int32_t offsetUs_ = 0;
    uint32_t lastSyncUs_ = 0;
    bool synced_ = false;

    // Low-pass filter for offset
    static constexpr float kAlpha = 0.1f;
};

}  // namespace domes
```

```cpp
// services/timing_service.cpp
#include "timing_service.hpp"
#include "esp_timer.h"

namespace domes {

uint32_t TimingService::getSyncedTimeUs() const {
    return esp_timer_get_time() - offsetUs_;
}

void TimingService::onSyncMessage(uint32_t masterTimeUs) {
    uint32_t localTimeUs = esp_timer_get_time();

    // Estimate one-way delay as half of typical RTT (~500us for ESP-NOW)
    constexpr uint32_t kEstimatedDelayUs = 500;

    // Master's time when we received = masterTimeUs + delay
    uint32_t masterNowUs = masterTimeUs + kEstimatedDelayUs;

    // Our offset from master
    int32_t newOffset = static_cast<int32_t>(localTimeUs - masterNowUs);

    if (synced_) {
        // Low-pass filter to smooth jitter
        offsetUs_ = static_cast<int32_t>(
            kAlpha * newOffset + (1.0f - kAlpha) * offsetUs_);
    } else {
        // First sync - use directly
        offsetUs_ = newOffset;
        synced_ = true;
    }

    lastSyncUs_ = localTimeUs;
}

}  // namespace domes
```

---

## Task Architecture

```cpp
// In main.cpp

void gameTask(void* param) {
    auto* engine = static_cast<GameEngine*>(param);

    while (true) {
        // Process game state
        engine->tick();

        // Check for touch events
        auto event = touchDriver->getEvent();
        if (event) {
            engine->onTouch(*event);
        }

        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms tick
    }
}

void commTask(void* param) {
    while (true) {
        // Handle incoming ESP-NOW messages
        // (Callback-driven, this task handles BLE)

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Create tasks in app_main
xTaskCreatePinnedToCore(gameTask, "game", 8192, &gameEngine,
                        PRIORITY_MEDIUM, nullptr, 1);  // Core 1
xTaskCreatePinnedToCore(commTask, "comm", 4096, nullptr,
                        PRIORITY_HIGH, nullptr, 0);    // Core 0
```

---

## Drill Types

### Reaction Drill

```cpp
// Standard reaction drill: light up, wait for touch, measure time
DrillConfig reactionDrill = {
    .timeoutMs = 3000,
    .feedbackMode = 3,      // All feedback
    .randomDelay = true,
    .minDelayMs = 500,
    .maxDelayMs = 2000,
};
```

### Sequence Drill

```cpp
// Multiple pods light up in sequence
struct SequenceDrillConfig {
    uint8_t podOrder[24];   // Order of pod IDs
    uint8_t podCount;       // Number of pods in sequence
    uint16_t intervalMs;    // Time between pods
};
```

### Speed Drill

```cpp
// As fast as possible, count successful hits
struct SpeedDrillConfig {
    uint16_t durationMs;    // Total drill duration
    uint16_t targetCount;   // Target number of hits
};
```

---

## Verification

```bash
# Build and flash
idf.py build && idf.py flash monitor

# Expected log output:
# I (XXX) state: State: Initializing -> Idle
# I (XXX) game: Game engine initialized

# On BLE connect:
# I (XXX) state: State: Idle -> Connected

# On START_DRILL command:
# I (XXX) state: State: Connected -> Armed
# I (XXX) game: Armed, waiting for touch...

# On touch:
# I (XXX) state: State: Armed -> Triggered
# I (XXX) game: Touch! Reaction time: 245 ms
# I (XXX) state: State: Triggered -> Feedback
```

---

*Prerequisites: 03-driver-development.md, 04-communication.md*
*Related: 09-reference.md (timing constants)*
