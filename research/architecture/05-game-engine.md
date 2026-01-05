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
┌──────────────────────────────────────────────────────────────────────────┐
│                           POD STATE MACHINE                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│    ┌──────────────┐                                                       │
│    │ Initializing │                                                       │
│    └──────┬───────┘                                                       │
│           │ init complete                                                 │
│           ▼                                                               │
│    ┌──────────────┐   BLE connect    ┌──────────────┐                     │
│    │     Idle     │─────────────────►│  Connected   │◄──────────────┐     │
│    └──────┬───────┘                  └──────┬───────┘               │     │
│           │                                 │                       │     │
│      long │                                 │ START_DRILL           │     │
│     press │                                 ▼                       │     │
│           │                          ┌──────────────┐               │     │
│           ▼                          │    Armed     │◄──────┐       │     │
│    ┌──────────────┐                  └──────┬───────┘       │       │     │
│    │  Standalone  │                         │ touch         │       │     │
│    └──────────────┘                         ▼               │       │     │
│                                      ┌──────────────┐       │       │     │
│                                      │  Triggered   │       │       │     │
│           ┌──────────────────────────┴──────┬───────┘       │       │     │
│           │ comm failure                    │ send event    │       │     │
│           ▼                                 ▼               │       │     │
│    ┌──────────────┐                  ┌──────────────┐       │       │     │
│    │    Error     │◄─────────────────│   Feedback   │───────┘       │     │
│    └──────┬───────┘                  └──────────────┘ complete      │     │
│           │ recovery                                                │     │
│           └─────────────────────────────────────────────────────────┘     │
│                                                                           │
│    ┌──────────────┐   (from any state except Initializing)               │
│    │  LowBattery  │◄────────────────────────────────────────             │
│    └──────────────┘   battery < threshold                                 │
│                                                                           │
│    ┌──────────────┐   (from Connected only)                              │
│    │OtaInProgress │◄────────────────────────────────────────             │
│    └──────────────┘   OTA_BEGIN received                                  │
│                                                                           │
│    ┌──────────────┐   (from Connected only)                              │
│    │ Calibrating  │◄────────────────────────────────────────             │
│    └──────────────┘   CALIBRATE command                                   │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
```

### State Descriptions

| State | Description | Entry Condition | Exit Condition |
|-------|-------------|-----------------|----------------|
| `kInitializing` | Hardware init, driver setup | Boot | Init complete |
| `kIdle` | No network connection, waiting | Init done, disconnected | BLE connect, long press |
| `kConnecting` | BLE/ESP-NOW connection in progress | Connection attempt | Connected, timeout |
| `kConnected` | Network connected, ready for commands | Connection established | Drill start, disconnect |
| `kArmed` | Waiting for touch event | Drill started | Touch, timeout |
| `kTriggered` | Touch detected, processing | Touch event | Event sent |
| `kFeedback` | Playing success/fail feedback | Event processed | Feedback complete |
| `kStandalone` | Offline mode, local drills only | Long press from idle | BLE connect |
| `kLowBattery` | Battery critical, limited operation | Battery < threshold | Charging, shutdown |
| `kOtaInProgress` | Receiving firmware update | OTA begin | OTA complete/fail |
| `kCalibrating` | Sensor calibration mode | Calibrate command | Calibration done |
| `kError` | Recoverable error state | Hardware/comm failure | Recovery action |

### State Implementation

```cpp
// game/state_machine.hpp
#pragma once

#include <cstdint>
#include <functional>
#include "esp_err.h"

namespace domes {

enum class PodState : uint8_t {
    kInitializing = 0,
    kIdle = 1,
    kConnecting = 2,
    kConnected = 3,
    kArmed = 4,
    kTriggered = 5,
    kFeedback = 6,
    kStandalone = 7,
    kLowBattery = 8,
    kOtaInProgress = 9,
    kCalibrating = 10,
    kError = 11,
    kCount  // For array sizing
};

const char* stateToString(PodState state);

/**
 * @brief Error type for error state
 */
enum class ErrorType : uint8_t {
    kNone = 0,
    kHardwareFailure,
    kCommunicationTimeout,
    kProtocolError,
    kResourceExhausted,
    kInvalidState,
};

/**
 * @brief Context data for current state
 */
struct StateContext {
    uint64_t enteredAtUs;    // When we entered this state (64-bit for long uptime)
    uint32_t timeoutMs;      // State timeout (0 = no timeout)
    uint8_t retryCount;      // For retry logic
    ErrorType lastError;     // Error type if in error state
    PodState previousState;  // State before current (for recovery)
};

using StateHandler = std::function<void(StateContext&)>;
using TransitionCallback = std::function<void(PodState from, PodState to)>;
using ErrorCallback = std::function<void(ErrorType error, PodState fromState)>;

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
     * @brief Force transition (bypass validation - use for error recovery)
     * @param newState Target state
     */
    void forceTransition(PodState newState);

    /**
     * @brief Transition to error state
     * @param error Error type
     */
    void enterError(ErrorType error);

    /**
     * @brief Attempt recovery from error state
     * @return true if recovery transition succeeded
     */
    bool attemptRecovery();

    /**
     * @brief Get current state
     */
    PodState currentState() const { return state_; }

    /**
     * @brief Get previous state (before current)
     */
    PodState previousState() const { return context_.previousState; }

    /**
     * @brief Get time in current state (microseconds)
     */
    uint64_t timeInStateUs() const;

    /**
     * @brief Check if state has timed out
     */
    bool isTimedOut() const;

    /**
     * @brief Check if currently in an error state
     */
    bool isInError() const { return state_ == PodState::kError; }

    /**
     * @brief Get last error type
     */
    ErrorType lastError() const { return context_.lastError; }

    /**
     * @brief Get retry count in current state
     */
    uint8_t retryCount() const { return context_.retryCount; }

    /**
     * @brief Increment retry count
     */
    void incrementRetry() { context_.retryCount++; }

    /**
     * @brief Reset retry count
     */
    void resetRetry() { context_.retryCount = 0; }

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
     * @brief Register error callback
     */
    void onError(ErrorCallback callback);

    /**
     * @brief Process state (call in main loop)
     * Handles timeouts and periodic state maintenance.
     */
    void tick();

    /**
     * @brief Check if transition is valid
     */
    bool isValidTransition(PodState from, PodState to) const;

    // Timeout constants
    static constexpr uint32_t kDefaultTimeoutMs = 0;  // No timeout
    static constexpr uint32_t kConnectingTimeoutMs = 10000;
    static constexpr uint32_t kArmedTimeoutMs = 5000;
    static constexpr uint32_t kTriggeredTimeoutMs = 1000;
    static constexpr uint32_t kFeedbackTimeoutMs = 500;
    static constexpr uint32_t kOtaTimeoutMs = 60000;
    static constexpr uint32_t kCalibrationTimeoutMs = 30000;
    static constexpr uint8_t kMaxRetries = 3;

private:
    PodState state_;
    StateContext context_{};
    TransitionCallback transitionCb_;
    ErrorCallback errorCb_;

    static constexpr size_t kStateCount = static_cast<size_t>(PodState::kCount);
    StateHandler enterHandlers_[kStateCount]{};
    StateHandler exitHandlers_[kStateCount]{};

    void handleTimeout();
    PodState getRecoveryState() const;
};

}  // namespace domes
```

```cpp
// game/state_machine.cpp (complete implementation)
#include "state_machine.hpp"
#include "esp_timer.h"
#include "esp_log.h"

namespace domes {

namespace {
    constexpr const char* kTag = "state";

    constexpr const char* kStateNames[] = {
        "Initializing", "Idle", "Connecting", "Connected",
        "Armed", "Triggered", "Feedback", "Standalone",
        "LowBattery", "OtaInProgress", "Calibrating", "Error"
    };

    constexpr const char* kErrorNames[] = {
        "None", "HardwareFailure", "CommunicationTimeout",
        "ProtocolError", "ResourceExhausted", "InvalidState"
    };
}

const char* stateToString(PodState state) {
    auto idx = static_cast<size_t>(state);
    if (idx < sizeof(kStateNames) / sizeof(kStateNames[0])) {
        return kStateNames[idx];
    }
    return "Unknown";
}

StateMachine::StateMachine(PodState initialState)
    : state_(initialState) {
    context_.enteredAtUs = esp_timer_get_time();
    context_.previousState = initialState;
}

bool StateMachine::transitionTo(PodState newState, uint32_t timeoutMs) {
    if (state_ == newState) {
        return true;  // Already in state
    }

    if (!isValidTransition(state_, newState)) {
        ESP_LOGW(kTag, "Invalid transition: %s -> %s",
                 stateToString(state_), stateToString(newState));
        return false;
    }

    PodState oldState = state_;

    // Exit current state
    auto exitIdx = static_cast<size_t>(oldState);
    if (exitIdx < kStateCount && exitHandlers_[exitIdx]) {
        exitHandlers_[exitIdx](context_);
    }

    // Update state
    context_.previousState = state_;
    state_ = newState;
    context_.enteredAtUs = esp_timer_get_time();
    context_.timeoutMs = timeoutMs;
    context_.retryCount = 0;
    if (newState != PodState::kError) {
        context_.lastError = ErrorType::kNone;
    }

    ESP_LOGI(kTag, "State: %s -> %s (timeout=%lums)",
             stateToString(oldState), stateToString(newState), timeoutMs);

    // Notify callback
    if (transitionCb_) {
        transitionCb_(oldState, newState);
    }

    // Enter new state
    auto enterIdx = static_cast<size_t>(newState);
    if (enterIdx < kStateCount && enterHandlers_[enterIdx]) {
        enterHandlers_[enterIdx](context_);
    }

    return true;
}

void StateMachine::forceTransition(PodState newState) {
    ESP_LOGW(kTag, "Force transition: %s -> %s",
             stateToString(state_), stateToString(newState));

    context_.previousState = state_;
    state_ = newState;
    context_.enteredAtUs = esp_timer_get_time();
    context_.timeoutMs = 0;
    context_.retryCount = 0;
}

void StateMachine::enterError(ErrorType error) {
    ESP_LOGE(kTag, "Entering error state: %s (from %s)",
             kErrorNames[static_cast<int>(error)], stateToString(state_));

    context_.lastError = error;

    if (errorCb_) {
        errorCb_(error, state_);
    }

    forceTransition(PodState::kError);
}

bool StateMachine::attemptRecovery() {
    if (state_ != PodState::kError) {
        return false;
    }

    PodState recovery = getRecoveryState();
    ESP_LOGI(kTag, "Attempting recovery to %s", stateToString(recovery));

    forceTransition(recovery);
    return true;
}

PodState StateMachine::getRecoveryState() const {
    // Determine best recovery state based on error type and previous state
    switch (context_.lastError) {
        case ErrorType::kHardwareFailure:
            // Hardware failed - try reinit via Idle
            return PodState::kIdle;

        case ErrorType::kCommunicationTimeout:
            // Comm failed - back to connected if possible
            return PodState::kConnected;

        case ErrorType::kProtocolError:
            // Protocol error - reset to connected
            return PodState::kConnected;

        default:
            // Default: go to idle
            return PodState::kIdle;
    }
}

uint64_t StateMachine::timeInStateUs() const {
    return esp_timer_get_time() - context_.enteredAtUs;
}

bool StateMachine::isTimedOut() const {
    if (context_.timeoutMs == 0) return false;
    return timeInStateUs() > (static_cast<uint64_t>(context_.timeoutMs) * 1000);
}

void StateMachine::onEnter(PodState state, StateHandler handler) {
    auto idx = static_cast<size_t>(state);
    if (idx < kStateCount) {
        enterHandlers_[idx] = handler;
    }
}

void StateMachine::onExit(PodState state, StateHandler handler) {
    auto idx = static_cast<size_t>(state);
    if (idx < kStateCount) {
        exitHandlers_[idx] = handler;
    }
}

void StateMachine::onTransition(TransitionCallback callback) {
    transitionCb_ = callback;
}

void StateMachine::onError(ErrorCallback callback) {
    errorCb_ = callback;
}

void StateMachine::tick() {
    if (isTimedOut()) {
        handleTimeout();
    }
}

void StateMachine::handleTimeout() {
    ESP_LOGW(kTag, "State %s timed out after %llums",
             stateToString(state_), timeInStateUs() / 1000);

    switch (state_) {
        case PodState::kConnecting:
            transitionTo(PodState::kIdle);
            break;

        case PodState::kArmed:
            // Touch timeout - notify master and return to connected
            ESP_LOGI(kTag, "Armed timeout - no touch detected");
            transitionTo(PodState::kConnected);
            break;

        case PodState::kTriggered:
            // Stuck in triggered - force to feedback or error
            if (context_.retryCount < kMaxRetries) {
                context_.retryCount++;
                // Retry sending event
            } else {
                enterError(ErrorType::kCommunicationTimeout);
            }
            break;

        case PodState::kFeedback:
            // Feedback too long - skip and continue
            transitionTo(PodState::kConnected);
            break;

        case PodState::kOtaInProgress:
            // OTA timeout - serious error
            enterError(ErrorType::kCommunicationTimeout);
            break;

        case PodState::kCalibrating:
            // Calibration timeout
            transitionTo(PodState::kConnected);
            break;

        case PodState::kError:
            // Try recovery after timeout
            attemptRecovery();
            break;

        default:
            // Other states don't typically timeout
            break;
    }
}

bool StateMachine::isValidTransition(PodState from, PodState to) const {
    // LowBattery can be entered from most states
    if (to == PodState::kLowBattery) {
        return from != PodState::kInitializing &&
               from != PodState::kLowBattery;
    }

    // Error can be entered from any state
    if (to == PodState::kError) {
        return true;
    }

    switch (from) {
        case PodState::kInitializing:
            return to == PodState::kIdle;

        case PodState::kIdle:
            return to == PodState::kConnecting ||
                   to == PodState::kStandalone;

        case PodState::kConnecting:
            return to == PodState::kConnected ||
                   to == PodState::kIdle;  // Timeout/failure

        case PodState::kConnected:
            return to == PodState::kArmed ||
                   to == PodState::kIdle ||          // Disconnect
                   to == PodState::kOtaInProgress ||
                   to == PodState::kCalibrating;

        case PodState::kArmed:
            return to == PodState::kTriggered ||
                   to == PodState::kConnected;  // Timeout/cancel

        case PodState::kTriggered:
            return to == PodState::kFeedback ||
                   to == PodState::kConnected;  // Error fallback

        case PodState::kFeedback:
            return to == PodState::kArmed ||    // Next round
                   to == PodState::kConnected;  // Drill complete

        case PodState::kStandalone:
            return to == PodState::kArmed ||    // Local drill
                   to == PodState::kIdle ||     // Exit standalone
                   to == PodState::kConnecting; // Network found

        case PodState::kLowBattery:
            return to == PodState::kIdle ||     // Battery recovered
                   to == PodState::kConnected;  // Charging + connected

        case PodState::kOtaInProgress:
            return to == PodState::kConnected || // OTA complete
                   to == PodState::kIdle;        // OTA failed, reboot

        case PodState::kCalibrating:
            return to == PodState::kConnected;

        case PodState::kError:
            return to == PodState::kIdle ||
                   to == PodState::kConnected;

        default:
            return false;
    }
}

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
