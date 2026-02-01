# 05 - Game Engine

## AI Agent Instructions

Load this file when:
- Implementing per-pod game logic (arm, touch detection, feedback cycle)
- Adding or modifying pod primitives (LED, touch, audio, haptic control)
- Handling touch detection and reaction time measurement
- Working on clock synchronization between pods

Prerequisites: `03-driver-development.md`, `04-communication.md`
Companions: `11-system-modes.md` (device lifecycle), `12-multi-pod-orchestration.md` (multi-pod coordination)

---

## How This Document Fits

Three documents define the DOMES runtime architecture. Each operates at a different scope:

```
+---------------------------------------------------------------+
|  Doc 12: Multi-Pod Orchestration                              |
|  Coordinates pods: discovery, roles, drill routing, results   |
|                                                               |
|  +----------------------------------------------------------+|
|  |  Doc 11: System Modes                                     ||
|  |  Per-pod lifecycle: BOOTING > IDLE > CONNECTED > GAME     ||
|  |                                                           ||
|  |  +------------------------------------------------------+||
|  |  |  Doc 05: Game Engine                   << THIS DOC   |||
|  |  |  Per-pod game logic within GAME mode                  |||
|  |  |  Ready > Armed > Triggered > Feedback                 |||
|  |  +------------------------------------------------------+||
|  +----------------------------------------------------------+|
+---------------------------------------------------------------+
```

| Document | Scope | Defines |
|----------|-------|---------|
| **05 - Game Engine (this doc)** | Single pod, single round | GameState FSM, primitives, reaction timing |
| **11 - System Modes** | Single pod, full lifecycle | SystemMode FSM, feature masks, power management |
| **12 - Multi-Pod Orchestration** | Multiple pods, full session | Roles, discovery, drill programs, result collection |

**This document** defines what a pod does as a **game target**: receive an arm command, detect a touch (or timeout), play feedback, and report the result. This logic runs identically on every pod — master and slave alike. The GameState FSM defined here operates exclusively within `SystemMode::GAME` (see doc 11).

Who triggers these actions depends on the pod's role (see doc 12):
- On the **master pod**: the `DrillInterpreter` (see doc 12, §Drill Execution) calls `GameEngine.arm()` directly
- On **slave pods**: a `PodCommandHandler` translates incoming ESP-NOW `ARM_TOUCH` messages into `GameEngine.arm()` calls

### Shared Terminology

These terms are used consistently across docs 05, 11, and 12:

| Term | Definition | Authoritative Doc |
|------|-----------|-------------------|
| **SystemMode** | Device lifecycle state (BOOTING, IDLE, TRIAGE, CONNECTED, GAME, ERROR) | Doc 11 |
| **GameState** | Per-pod, per-round game state (Ready, Armed, Triggered, Feedback) | Doc 05 (this doc) |
| **ModeManager** | Class that manages SystemMode transitions and feature masks | Doc 11 |
| **GameEngine** | Class that manages GameState transitions and touch detection on a single pod | Doc 05 (this doc) |
| **DrillInterpreter** | Master-only class that steps through a drill program and sends commands to pods | Doc 12 |
| **Primitive** | Atomic pod action: SET_COLOR, ARM_TOUCH, PLAY_SOUND, FIRE_HAPTIC | Doc 05 (this doc) / Doc 04 |
| **CommService** | ESP-NOW send/receive singleton for inter-pod communication | Doc 04 |
| **Master** | Pod connected to phone via BLE; runs DrillInterpreter | Doc 12 |
| **Slave** | Pod receiving commands from master via ESP-NOW | Doc 12 |

---

## Game State Machine (GameState)

### Overview

When a pod enters `SystemMode::GAME` (see doc 11, §System Modes), its GameState FSM activates. The FSM tracks the pod's behavior during a **single round** of a drill — one arm-touch-feedback cycle. The DrillInterpreter (see doc 12) or PodCommandHandler drives transitions by calling `arm()`. Touch detection and feedback happen locally.

### States

```
+----------------------------------------------------------+
|               SystemMode::GAME (doc 11)                  |
|                                                          |
|  +--------+  arm()    +--------+                         |
|  | Ready  |---------->| Armed  |                         |
|  +--------+           +---+----+                         |
|      ^                    |                              |
|      |           +--------+--------+                     |
|      |           |                 |                     |
|      |        touch            timeout                   |
|      |           |                 |                     |
|      |           v                 |                     |
|      |    +-----------+            |                     |
|      |    | Triggered |            |                     |
|      |    +-----+-----+            |                     |
|      |          | auto             |                     |
|      |          v                  v                     |
|      |    +---------------------------+                  |
|      |    |         Feedback          |                  |
|      |    |  hit: white flash + beep  |                  |
|      |    |  miss: red flash          |                  |
|      |    +-------------+-------------+                  |
|      |                  | complete                       |
|      +------------------+                                |
|                                                          |
+----------------------------------------------------------+
```

| State | Purpose | Entry | Exit |
|-------|---------|-------|------|
| **Ready** | Between rounds, waiting for arm command | Feedback complete, or GAME mode entered | `arm()` called |
| **Armed** | Touch detection active, measuring reaction time | `arm()` with config | Touch detected or timeout |
| **Triggered** | Touch was detected, reaction time recorded | Touch event from driver | Auto-advance to Feedback |
| **Feedback** | Playing hit/miss feedback (LED flash, sound, haptic) | After Triggered (hit) or after timeout (miss) | Feedback animation complete |

**Hit path:** Ready → Armed → Triggered → Feedback → Ready
**Miss path:** Ready → Armed → Feedback (skips Triggered) → Ready

### GameState Implementation

```cpp
// game/game_state.hpp
#pragma once

#include <cstdint>

namespace domes {

enum class GameState : uint8_t {
    kReady,      // Between rounds, awaiting arm command
    kArmed,      // Touch detection active, measuring time
    kTriggered,  // Touch detected (transient, auto-advances)
    kFeedback,   // Playing feedback animation/sound/haptic
};

const char* gameStateToString(GameState state);

}  // namespace domes
```

### Valid Transitions

```cpp
bool isValidGameTransition(GameState from, GameState to) {
    switch (from) {
        case GameState::kReady:
            return to == GameState::kArmed;
        case GameState::kArmed:
            return to == GameState::kTriggered   // touch (hit)
                || to == GameState::kFeedback;   // timeout (miss)
        case GameState::kTriggered:
            return to == GameState::kFeedback;   // auto-advance
        case GameState::kFeedback:
            return to == GameState::kReady;      // round complete
    }
    return false;
}
```

**Note:** Any state can also return to `kReady` via `disarm()` — a forced reset used when the drill is stopped or the pod exits `SystemMode::GAME`.

---

## Pod Primitives

Primitives are the atomic actions a pod can execute. The DrillInterpreter (see doc 12) composes primitives into drills. Each primitive maps to an ESP-NOW message type defined in doc 04 (§Protocol Messages).

| Primitive | ESP-NOW Message | What It Does | Response |
|-----------|----------------|-------------|----------|
| **SET_COLOR** | `kSetColor` (0x02) | Set LED color, duration, transition | None (fire-and-forget) |
| **ARM_TOUCH** | `kArmTouch` (0x03) | Start touch detection with timeout | `kTouchEvent` or `kTimeoutEvent` |
| **PLAY_SOUND** | `kPlaySound` (0x04) | Play audio clip by ID | None (fire-and-forget) |
| **FIRE_HAPTIC** | `kHapticPulse` (0x05) | Trigger haptic effect | None (fire-and-forget) |
| **STOP_ALL** | `kStopAll` (0x06) | Cancel all active primitives | None |

### Primitive Execution

Primitives execute **locally on the target pod**. The master sends an ESP-NOW command; the target pod's `PodCommandHandler` dispatches it to the appropriate service. For `ARM_TOUCH`, the handler calls `GameEngine.arm()`, which starts the GameState FSM.

On the master pod targeting itself, primitives are local function calls — no ESP-NOW round-trip.

```
Master targeting Pod 3 (remote):
  DrillInterpreter → CommService.send(pod3, ARM_TOUCH) → ESP-NOW → PodCommandHandler → GameEngine.arm()

Master targeting itself (local):
  DrillInterpreter → GameEngine.arm()
```

### SET_COLOR

```cpp
// Set LED color on target pod
struct SetColorMsg {
    MessageHeader header;
    uint8_t r, g, b, w;
    uint16_t durationMs;    // 0 = indefinite
    uint8_t transition;     // 0 = instant, 1 = fade
};
```

### ARM_TOUCH

```cpp
// Arm touch detection on target pod — triggers GameState: Ready → Armed
struct ArmTouchMsg {
    MessageHeader header;
    uint16_t timeoutMs;     // Max wait for touch (e.g. 3000)
    uint8_t feedbackMode;   // 0 = none, 1 = audio, 2 = haptic, 3 = all
};
```

The target pod responds with either `kTouchEvent` (hit) or `kTimeoutEvent` (miss).

### Response Events

```cpp
// Pod → Master: touch detected (hit)
struct TouchEventMsg {
    MessageHeader header;
    uint8_t podId;
    uint32_t reactionTimeUs;  // Locally measured: touch_at - armed_at
    uint8_t touchStrength;    // Capacitive reading intensity
};

// Pod → Master: timeout expired (miss)
struct TimeoutEventMsg {
    MessageHeader header;
    uint8_t podId;
};
```

---

## GameEngine Class

The GameEngine manages one pod's GameState FSM. It handles a single arm-touch-feedback cycle per invocation. It does not manage drill-level logic (round sequencing, pod selection, scoring) — that is the DrillInterpreter's job (see doc 12, §Drill Execution).

```cpp
// game/game_engine.hpp
#pragma once

#include "game_state.hpp"
#include "interfaces/i_touch_driver.hpp"
#include "services/feedback_service.hpp"
#include <cstdint>
#include <functional>

namespace domes {

struct ArmConfig {
    uint16_t timeoutMs = 3000;    // Max wait for touch
    uint8_t feedbackMode = 3;     // 0=none, 1=audio, 2=haptic, 3=all
};

struct GameEvent {
    enum class Type : uint8_t {
        kHit,     // Touch detected within timeout
        kMiss,    // Timeout expired, no touch
    };

    Type type;
    uint32_t reactionTimeUs;  // Valid for kHit only
    uint8_t touchStrength;    // Valid for kHit only
};

using GameEventCallback = std::function<void(const GameEvent&)>;

class GameEngine {
public:
    GameEngine(ITouchDriver& touch, FeedbackService& feedback);

    esp_err_t init();

    /**
     * @brief Arm touch detection — transitions Ready → Armed
     *
     * Called by DrillInterpreter (on master) or PodCommandHandler (on slave).
     */
    esp_err_t arm(const ArmConfig& config);

    /**
     * @brief Cancel arming — force back to Ready from any state
     *
     * Called on STOP_ALL or when exiting SystemMode::GAME.
     */
    esp_err_t disarm();

    /**
     * @brief Process game logic (call from game task at 1ms)
     *
     * Checks for timeouts in Armed state, advances through Triggered
     * and Feedback states automatically.
     */
    void tick();

    /**
     * @brief Handle touch event from driver (ISR-safe: enqueues)
     */
    void onTouch(const TouchEvent& event);

    GameState currentState() const { return state_; }
    uint32_t lastReactionTimeUs() const { return lastReactionTimeUs_; }

    /**
     * @brief Register callback for game events (hit/miss)
     *
     * On master: callback feeds DrillInterpreter.onEvent()
     * On slave: callback sends ESP-NOW event to master
     */
    void setEventCallback(GameEventCallback cb);

private:
    ITouchDriver& touch_;
    FeedbackService& feedback_;

    GameState state_ = GameState::kReady;
    ArmConfig currentArm_{};
    uint32_t armedAtUs_ = 0;
    uint32_t lastReactionTimeUs_ = 0;
    GameEventCallback eventCb_;

    void enterFeedback(GameEvent::Type type, uint32_t reactionUs, uint8_t strength);
};

}  // namespace domes
```

### GameEngine Implementation (Key Methods)

```cpp
// game/game_engine.cpp
#include "game_engine.hpp"
#include "esp_timer.h"
#include "esp_log.h"

namespace domes {

namespace {
    constexpr const char* kTag = "game";
    constexpr uint32_t kFeedbackDurationMs = 200;
}

esp_err_t GameEngine::arm(const ArmConfig& config) {
    if (state_ != GameState::kReady) {
        ESP_LOGW(kTag, "Cannot arm: state is %s", gameStateToString(state_));
        return ESP_ERR_INVALID_STATE;
    }

    currentArm_ = config;
    armedAtUs_ = esp_timer_get_time();
    state_ = GameState::kArmed;

    // Enable touch detection on driver
    touch_.enable();

    ESP_LOGI(kTag, "Armed (timeout: %u ms, feedback: %u)",
             config.timeoutMs, config.feedbackMode);
    return ESP_OK;
}

esp_err_t GameEngine::disarm() {
    touch_.disable();
    state_ = GameState::kReady;
    ESP_LOGI(kTag, "Disarmed → Ready");
    return ESP_OK;
}

void GameEngine::onTouch(const TouchEvent& event) {
    if (state_ != GameState::kArmed) return;

    uint32_t reactionUs = esp_timer_get_time() - armedAtUs_;
    lastReactionTimeUs_ = reactionUs;

    touch_.disable();
    state_ = GameState::kTriggered;

    ESP_LOGI(kTag, "Touch! Reaction: %lu us (%lu ms)",
             reactionUs, reactionUs / 1000);

    // Auto-advance: Triggered → Feedback (hit)
    enterFeedback(GameEvent::Type::kHit, reactionUs, event.strength);
}

void GameEngine::tick() {
    switch (state_) {
        case GameState::kArmed: {
            // Check timeout
            uint32_t elapsed = esp_timer_get_time() - armedAtUs_;
            if (elapsed > currentArm_.timeoutMs * 1000u) {
                ESP_LOGI(kTag, "Timeout — miss");
                touch_.disable();
                enterFeedback(GameEvent::Type::kMiss, 0, 0);
            }
            break;
        }
        case GameState::kFeedback: {
            // Check if feedback animation is complete
            if (feedback_.isComplete()) {
                state_ = GameState::kReady;
            }
            break;
        }
        default:
            break;
    }
}

void GameEngine::enterFeedback(GameEvent::Type type, uint32_t reactionUs, uint8_t strength) {
    state_ = GameState::kFeedback;

    if (type == GameEvent::Type::kHit) {
        // White flash + success beep + haptic
        if (currentArm_.feedbackMode & 0x01) feedback_.playSound("hit_success");
        if (currentArm_.feedbackMode & 0x02) feedback_.fireHaptic(0x01);
        feedback_.flashLed(255, 255, 255, kFeedbackDurationMs);  // white
    } else {
        // Red flash, no sound
        feedback_.flashLed(255, 0, 0, kFeedbackDurationMs);  // red
    }

    // Report event to whoever armed us
    if (eventCb_) {
        eventCb_(GameEvent{
            .type = type,
            .reactionTimeUs = reactionUs,
            .touchStrength = strength,
        });
    }
}

}  // namespace domes
```

---

## Timing and Measurement

### Local Reaction Timing

Reaction time is measured **locally on the target pod** using a single clock source:

```
reaction_time = touch_at - armed_at
```

Both `touch_at` and `armed_at` come from `esp_timer_get_time()` on the same ESP32. This means:

- ESP-NOW transmission latency (~500us) does **not** affect the measurement
- No clock synchronization is needed for reaction drills
- Accuracy is limited only by the touch driver's sampling rate (~10ms = 100 Hz)

This is true regardless of role. On the master targeting itself, the same local clock is used. On a slave pod, arm and touch timestamps are both local.

### Clock Synchronization (for Sequence Drills)

Sequence drills (see doc 12, §Clock Synchronization) require multiple pods to activate at precise relative times. For this, the master periodically broadcasts `SYNC_CLOCK` messages and each slave estimates its offset using a low-pass filter.

```cpp
// services/timing_service.hpp
#pragma once

#include <cstdint>

namespace domes {

class TimingService {
public:
    /**
     * @brief Get time adjusted to master's clock (microseconds)
     */
    uint32_t getSyncedTimeUs() const;

    /**
     * @brief Process a SYNC_CLOCK message from master
     * @param masterTimeUs Master's timestamp when the message was sent
     */
    void onSyncMessage(uint32_t masterTimeUs);

    int32_t getOffsetUs() const { return offsetUs_; }
    bool isSynced() const { return synced_; }

private:
    int32_t offsetUs_ = 0;
    uint32_t lastSyncUs_ = 0;
    bool synced_ = false;
    static constexpr float kAlpha = 0.1f;  // Low-pass filter coefficient
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

    // Estimate one-way delay as half of typical ESP-NOW RTT (~500us)
    constexpr uint32_t kEstimatedDelayUs = 500;
    uint32_t masterNowUs = masterTimeUs + kEstimatedDelayUs;

    int32_t newOffset = static_cast<int32_t>(localTimeUs - masterNowUs);

    if (synced_) {
        // Low-pass filter to smooth jitter
        offsetUs_ = static_cast<int32_t>(
            kAlpha * newOffset + (1.0f - kAlpha) * offsetUs_);
    } else {
        offsetUs_ = newOffset;
        synced_ = true;
    }

    lastSyncUs_ = localTimeUs;
}

}  // namespace domes
```

**Target accuracy:** < 1ms between any two pods (see doc 09, §Timing Constants).

**When clock sync is NOT needed:** Single-pod reaction drills. Reaction time is always measured locally (see §Local Reaction Timing above).

---

## Drill Types (Phase 1: Built-In)

Phase 1 uses hardcoded drill configurations. The DrillInterpreter (see doc 12, §Drill Execution) reads these configs and drives the GameEngine accordingly. Phase 2 replaces these with downloadable bytecode programs (see doc 12, §Downloadable Drills).

Both phases use the same pod primitives (§Pod Primitives above) and the same GameEngine. The difference is where the drill program comes from — compiled into firmware vs. downloaded from the phone app.

### Reaction Drill

One pod lights up. User runs to it and touches. Measures reaction time. Repeats for N rounds with random pod selection and random delays.

```cpp
struct ReactionDrillConfig {
    uint8_t roundCount = 10;         // Number of rounds
    uint16_t reactionTimeoutMs = 3000;  // Max wait per round
    uint8_t feedbackMode = 3;        // 0=none, 1=audio, 2=haptic, 3=all
    bool randomDelay = true;         // Random delay before arming
    uint16_t minDelayMs = 500;       // Min inter-round delay
    uint16_t maxDelayMs = 2000;      // Max inter-round delay
};
```

### Sequence Drill

Multiple pods light up in a fixed order. User must touch them in sequence. Measures total time and per-pod split times. Requires clock synchronization (§Clock Synchronization above).

```cpp
struct SequenceDrillConfig {
    uint8_t podOrder[24];   // Pod IDs in order
    uint8_t podCount;       // Number of pods in sequence
    uint16_t intervalMs;    // Time between pod activations
};
```

### Speed Drill

As many touches as possible in a time window. Random pods light up continuously. Measures hit rate and average reaction time.

```cpp
struct SpeedDrillConfig {
    uint16_t durationMs;    // Total drill duration
    uint16_t targetCount;   // Target number of hits (for scoring)
};
```

---

## Task Architecture

The game task runs on Core 1 at a 1ms tick rate. It processes GameEngine state transitions and handles the touch event queue. On the master pod, the DrillInterpreter also ticks here.

```cpp
// In main.cpp

void gameTask(void* param) {
    auto* engine = static_cast<GameEngine*>(param);

    while (true) {
        // Process game state (timeouts, feedback completion)
        engine->tick();

        // Dequeue touch events (from ISR via queue)
        TouchEvent event;
        if (xQueueReceive(touchQueue, &event, 0) == pdTRUE) {
            engine->onTouch(event);
        }

        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms tick
    }
}

// Create task in app_main
xTaskCreatePinnedToCore(gameTask, "game", 8192, &gameEngine,
                        PRIORITY_MEDIUM, nullptr, 1);  // Core 1
```

Communication (ESP-NOW receive, BLE notifications) runs on Core 0 in a separate task (see doc 04):

```cpp
void commTask(void* param) {
    while (true) {
        // ESP-NOW receive is callback-driven (registered in CommService.init())
        // BLE events are handled by NimBLE host task
        // This task processes the command queue for game commands
        // arriving from ESP-NOW (on slave pods)

        GameCommand cmd;
        if (xQueueReceive(gameCommandQueue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Dispatch to GameEngine — enqueue to gameTask's queue
            // to avoid cross-task calls
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

xTaskCreatePinnedToCore(commTask, "comm", 4096, nullptr,
                        PRIORITY_HIGH, nullptr, 0);  // Core 0
```

**Thread safety:** The GameEngine is single-threaded — only the game task calls its methods. Commands arriving from ESP-NOW on Core 0 are enqueued and dequeued by the game task on Core 1.

---

## Verification

```bash
# Build and flash
cd firmware/domes && idf.py build && idf.py flash monitor

# Expected log output on entering GAME mode:
# I (XXX) game: Game engine initialized
# I (XXX) mode: State: CONNECTED -> GAME
# I (XXX) game: Armed (timeout: 3000 ms, feedback: 3)

# On touch:
# I (XXX) game: Touch! Reaction: 245000 us (245 ms)
# I (XXX) game: Feedback: hit (white flash, sound, haptic)

# On timeout:
# I (XXX) game: Timeout - miss
# I (XXX) game: Feedback: miss (red flash)

# On drill end:
# I (XXX) game: Disarmed -> Ready
# I (XXX) mode: State: GAME -> CONNECTED
```

---

*Prerequisites: 03-driver-development.md, 04-communication.md*
*Companions: 11-system-modes.md, 12-multi-pod-orchestration.md*
*Related: 09-reference.md (timing constants, pin mappings)*
