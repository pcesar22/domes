# 05 - Game Engine

## AI Agent Instructions

Load this file when implementing game logic, state machine, or drills.

Prerequisites: `03-driver-development.md`, `04-communication.md`

---

## User Session: What Happens When You Play

This section ties the API to real user actions. Follow a single drill session:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         COMPLETE DRILL SESSION                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 1. USER POWERS ON POD                                                │    │
│  │    Pod boots, LEDs cycle through startup animation                   │    │
│  │    State: Initializing → Idle                                        │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 2. USER OPENS PHONE APP, TAPS "CONNECT"                             │    │
│  │    Phone scans BLE, finds pod, pairs                                 │    │
│  │    State: Idle → Connecting → Connected                             │    │
│  │    LEDs pulse blue during connect, solid green when connected       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 3. USER TAPS "START DRILL" IN APP                                   │    │
│  │    Master broadcasts ARM command with random delay                   │    │
│  │    State: Connected → Armed                                          │    │
│  │    LEDs fade to dim white (anticipation)                            │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 4. RANDOM DELAY ELAPSES, POD LIGHTS UP                              │    │
│  │    LEDs flash bright (signal to user)                               │    │
│  │    Timer starts counting reaction time                              │    │
│  │    State: Armed (active phase)                                       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 5. USER TAPS THE POD                                                │    │
│  │    Touch detected (capacitive + IMU tap fusion)                     │    │
│  │    Reaction time captured: 245ms                                    │    │
│  │    State: Armed → Triggered                                          │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 6. POD PLAYS FEEDBACK                                               │    │
│  │    LEDs flash green (success)                                       │    │
│  │    Beep sound plays                                                 │    │
│  │    Haptic vibration pulses                                          │    │
│  │    State: Triggered → Feedback                                       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                   │                                          │
│                                   ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ 7. POD REPORTS TO MASTER                                            │    │
│  │    ESP-NOW message: {pod_id, reaction_time_us, timestamp}           │    │
│  │    Master logs result, decides next pod                             │    │
│  │    State: Feedback → Armed (next round) or → Connected (drill end)  │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## State Machine Overview

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              POD STATE MACHINE                                │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│                          ┌──────────────┐                                     │
│                          │ Initializing │                                     │
│                          └──────┬───────┘                                     │
│                                 │                                             │
│                                 ▼                                             │
│   ┌──────────────┐       ┌──────────────┐       ┌──────────────┐             │
│   │  Standalone  │◄──────│     Idle     │──────►│  Connecting  │             │
│   │ (long press) │       └──────────────┘       └──────┬───────┘             │
│   └──────┬───────┘                                     │                      │
│          │                                             ▼                      │
│          │                                      ┌──────────────┐              │
│          └──────────────────────────────────────│  Connected   │◄─────┐       │
│                                                 └──────┬───────┘      │       │
│                                                        │              │       │
│                                                        ▼              │       │
│                            ┌───────────────────────────┐              │       │
│                            │                           │              │       │
│                            ▼                           ▼              │       │
│                     ┌──────────────┐            ┌──────────────┐      │       │
│                     │    Armed     │            │   OTA/Cal    │      │       │
│                     └──────┬───────┘            └──────┬───────┘      │       │
│                            │ touch                     │              │       │
│                            ▼                           └──────────────┤       │
│                     ┌──────────────┐                                  │       │
│                     │  Triggered   │                                  │       │
│                     └──────┬───────┘                                  │       │
│                            │                                          │       │
│                            ▼                                          │       │
│                     ┌──────────────┐                                  │       │
│                     │   Feedback   │──────────────────────────────────┘       │
│                     └──────────────┘                                          │
│                                                                               │
│   ┌──────────────┐   ┌──────────────┐                                        │
│   │    Error     │   │  LowBattery  │   (can enter from any active state)    │
│   └──────────────┘   └──────────────┘                                        │
│                                                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## State Descriptions

| State | User Sees | System Does |
|-------|-----------|-------------|
| **Initializing** | Boot animation | Drivers init, self-test |
| **Idle** | Slow pulse | Waiting for BLE connection |
| **Connecting** | Blue pulse | BLE handshake |
| **Connected** | Solid green | Ready for commands |
| **Armed** | Dim → bright flash | Waiting for touch, counting |
| **Triggered** | (instant) | Captures reaction time |
| **Feedback** | Green flash + beep + vibe | Multi-modal success/fail |
| **Standalone** | Orange | Local drills without network |
| **LowBattery** | Red pulse | Reduced functionality |
| **Error** | Red solid | Recovery or shutdown |

---

## API Tied to User Actions

| User Action | API Call | State Transition |
|-------------|----------|------------------|
| Power on | `app_main()` | → Initializing → Idle |
| Open app, connect | BLE `connect` characteristic | Idle → Connecting → Connected |
| Tap "Start Drill" | `gameEngine.startDrill(config)` | Connected → Armed |
| Wait for light | (internal timer) | Armed active phase |
| Tap the pod | `touchDriver.onTouch()` callback | Armed → Triggered |
| (automatic) | `feedbackService.playSuccess()` | Triggered → Feedback |
| (automatic) | `commService.sendEvent(...)` | Feedback → Armed or Connected |
| Tap "Stop" | `gameEngine.stopDrill()` | Any → Connected |
| Long press | GPIO interrupt | Idle → Standalone |
| Low battery | `powerDriver.onLowBattery()` | Any → LowBattery |

---

## Reaction Time Measurement

```
                    TIMING DIAGRAM

    Master clock    ─────────────────────────────────────────►
                            │                    │
                            │ ARM_CMD            │ TOUCH_EVENT
                            │ (synced time T0)   │ (synced time T1)
                            ▼                    ▼
    ┌───────────────┬───────────────────────────────────────────┐
    │   RANDOM      │           REACTION TIME                   │
    │   DELAY       │         T1 - T0 = 245ms                   │
    │  500-2000ms   │                                           │
    └───────────────┴───────────────────────────────────────────┘
                    │                            │
                    │ LED flash                  │ Touch detected
                    │ (user sees)                │ (sensor reports)
```

---

## Multi-Pod Coordination

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         MULTI-POD DRILL FLOW                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    MASTER POD                           FOLLOWER PODS                        │
│    ──────────                           ──────────────                       │
│                                                                              │
│    ┌───────────────┐                                                        │
│    │ User taps     │                                                        │
│    │ "Start Drill" │                                                        │
│    └───────┬───────┘                                                        │
│            │                                                                 │
│            ▼                                                                 │
│    ┌───────────────┐      ESP-NOW broadcast                                 │
│    │ Select random │ ─────────────────────────────────────►                 │
│    │ pod (Pod #3)  │      ARM_CMD {pod_id: 3, delay: 1.2s}                  │
│    └───────────────┘                                                        │
│            │                            │                                    │
│            │                            ▼                                    │
│            │                    ┌───────────────┐                           │
│            │                    │ Pod #3 only:  │                           │
│            │                    │ → Armed state │                           │
│            │                    │ → Wait 1.2s   │                           │
│            │                    │ → Flash LEDs  │                           │
│            │                    └───────┬───────┘                           │
│            │                            │                                    │
│            │                            │ USER TAPS                          │
│            │                            ▼                                    │
│            │                    ┌───────────────┐                           │
│            │    ESP-NOW         │ TOUCH_EVENT   │                           │
│            │ ◄──────────────────│ {time: 245ms} │                           │
│            │                    └───────────────┘                           │
│            ▼                                                                 │
│    ┌───────────────┐                                                        │
│    │ Log result    │                                                        │
│    │ Select next   │                                                        │
│    │ pod → repeat  │                                                        │
│    └───────────────┘                                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Drill Types

| Drill | Description | Pod Selection |
|-------|-------------|---------------|
| **Reaction** | Single pod lights, measure time | Random |
| **Sequence** | Pods light in order | Predetermined |
| **Speed** | Many taps in time limit | Round-robin |
| **Memory** | Remember and repeat pattern | Scripted |
| **Chase** | Next pod lights on touch | Sequential |

---

## State Machine Interface

```cpp
// EXAMPLE: Minimal state machine API

enum class PodState : uint8_t {
    kInitializing, kIdle, kConnecting, kConnected,
    kArmed, kTriggered, kFeedback,
    kStandalone, kLowBattery, kError
};

class StateMachine {
public:
    bool transitionTo(PodState newState);
    PodState currentState() const;
    void onEnter(PodState state, std::function<void()> handler);
    void onExit(PodState state, std::function<void()> handler);
    void tick();  // Call in main loop
};
```

---

## Game Engine Interface

```cpp
// EXAMPLE: Game engine API

struct DrillConfig {
    uint16_t timeoutMs;
    uint16_t minDelayMs;
    uint16_t maxDelayMs;
    uint8_t feedbackMode;  // LED, audio, haptic flags
};

class GameEngine {
public:
    esp_err_t startDrill(const DrillConfig& config);
    esp_err_t stopDrill();
    PodState currentState() const;
    uint32_t lastReactionTimeUs() const;
    void onTouch(const TouchEvent& event);
    void tick();  // Process state
};
```

---

## Task Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              DUAL-CORE TASK LAYOUT                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│         CORE 0 (Protocol)                    CORE 1 (Application)           │
│         ─────────────────                    ────────────────────            │
│                                                                              │
│    ┌──────────────────┐                 ┌──────────────────┐                │
│    │   WiFi/BLE Task  │                 │   Game Task      │                │
│    │   (NimBLE host)  │                 │   (state machine)│                │
│    │   Priority: HIGH │                 │   Priority: MED  │                │
│    └──────────────────┘                 └──────────────────┘                │
│                                                                              │
│    ┌──────────────────┐                 │                                   │
│    │  ESP-NOW RX Task │                 │                                   │
│    │  (message queue) │ ───────────────►│                                   │
│    │  Priority: HIGH  │     events      │                                   │
│    └──────────────────┘                 │                                   │
│                                                                              │
│    ┌──────────────────┐                 ┌──────────────────┐                │
│    │   Timing Sync    │                 │   LED Animation  │                │
│    │   (clock adjust) │                 │   (RMT DMA)      │                │
│    │   Priority: MED  │                 │   Priority: LOW  │                │
│    └──────────────────┘                 └──────────────────┘                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Error Recovery Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           ERROR RECOVERY                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    ┌───────────────────────────────────────────────────────┐                │
│    │                    ANY STATE                          │                │
│    └───────────────────────────┬───────────────────────────┘                │
│                                │ error detected                              │
│                                ▼                                             │
│    ┌───────────────────────────────────────────────────────┐                │
│    │                    ERROR STATE                         │                │
│    │                                                        │                │
│    │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐ │                │
│    │   │  Hardware   │   │   Comm      │   │  Protocol   │ │                │
│    │   │  Failure    │   │  Timeout    │   │   Error     │ │                │
│    │   └──────┬──────┘   └──────┬──────┘   └──────┬──────┘ │                │
│    │          │                 │                 │        │                │
│    │          ▼                 ▼                 ▼        │                │
│    │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐ │                │
│    │   │ Reinit      │   │ Reconnect   │   │ Reset to    │ │                │
│    │   │ drivers     │   │ network     │   │ connected   │ │                │
│    │   └─────────────┘   └─────────────┘   └─────────────┘ │                │
│    │                                                        │                │
│    └───────────────────────────┬───────────────────────────┘                │
│                                │ recovery successful                         │
│                                ▼                                             │
│    ┌───────────────────────────────────────────────────────┐                │
│    │             IDLE or CONNECTED                         │                │
│    └───────────────────────────────────────────────────────┘                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Related: `03-driver-development.md`, `04-communication.md`, `11-clock-sync.md`*
