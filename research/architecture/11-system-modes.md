# 11 - System Modes & Power Management

## AI Agent Instructions

Load this file when:
- Implementing system mode transitions
- Adding power management or sleep states
- Mapping features to operating modes
- Building the CLI triage workflow
- Adding battery monitoring
- Modifying service task polling rates

Prerequisites: `03-driver-development.md`, `04-communication.md`, `05-game-engine.md`
Companions: `05-game-engine.md` (game logic within GAME mode), `12-multi-pod-orchestration.md` (multi-pod coordination)

---

## How This Document Fits

Three documents define the DOMES runtime architecture. Each operates at a different scope:

```
+---------------------------------------------------------------+
|  Doc 12: Multi-Pod Orchestration                              |
|  Coordinates pods: discovery, roles, drill routing, results   |
|                                                               |
|  +----------------------------------------------------------+|
|  |  Doc 11: System Modes                    << THIS DOC      ||
|  |  Per-pod lifecycle: BOOTING > IDLE > CONNECTED > GAME     ||
|  |                                                           ||
|  |  +------------------------------------------------------+||
|  |  |  Doc 05: Game Engine                                  |||
|  |  |  Per-pod game logic within GAME mode                  |||
|  |  |  Ready > Armed > Triggered > Feedback                 |||
|  |  +------------------------------------------------------+||
|  +----------------------------------------------------------+|
+---------------------------------------------------------------+
```

| Document | Scope | Defines |
|----------|-------|---------|
| **05 - Game Engine** | Single pod, single round | GameState FSM, primitives, reaction timing |
| **11 - System Modes (this doc)** | Single pod, full lifecycle | SystemMode FSM, feature masks, power management |
| **12 - Multi-Pod Orchestration** | Multiple pods, full session | Roles, discovery, drill programs, result collection |

**This document** defines the **lifecycle of a single pod** from power-on to shutdown. The `SystemMode` FSM determines which hardware features are active at any given time. When the pod enters `SystemMode::GAME`, the GameState FSM (see doc 05) takes over game-specific logic. Mode transitions are driven by external events: BLE connections, ESP-NOW messages (see doc 12), CLI commands, and timeouts.

Every pod — master or slave — runs its own `ModeManager` independently. The orchestration layer (doc 12) coordinates mode transitions across pods by sending commands that each pod's `ModeManager` processes locally.

### Shared Terminology

These terms are used consistently across docs 05, 11, and 12:

| Term | Definition | Authoritative Doc |
|------|-----------|-------------------|
| **SystemMode** | Device lifecycle state (BOOTING, IDLE, TRIAGE, CONNECTED, GAME, ERROR) | Doc 11 (this doc) |
| **GameState** | Per-pod, per-round game state (Ready, Armed, Triggered, Feedback) | Doc 05 |
| **ModeManager** | Class that manages SystemMode transitions and feature masks | Doc 11 (this doc) |
| **GameEngine** | Class that manages GameState transitions and touch detection on a single pod | Doc 05 |
| **DrillInterpreter** | Master-only class that steps through a drill program and sends commands to pods | Doc 12 |
| **Primitive** | Atomic pod action: SET_COLOR, ARM_TOUCH, PLAY_SOUND, FIRE_HAPTIC | Doc 05 / Doc 04 |
| **CommService** | ESP-NOW send/receive singleton for inter-pod communication | Doc 04 |
| **Master** | Pod connected to phone via BLE; runs DrillInterpreter | Doc 12 |
| **Slave** | Pod receiving commands from master via ESP-NOW | Doc 12 |

---

## Problem Statement

After boot, the firmware currently enters a **flat, always-on state** where every service runs at full speed indefinitely. There is no concept of operating modes, no power management, and no lifecycle beyond "everything is on." This design is fine for bench testing but insufficient for production use:

- **No power awareness** -- All services poll at their maximum rate regardless of what the device is doing
- **No modal behavior** -- The device cannot distinguish between "waiting to be configured" and "running a drill"
- **No structured triage workflow** -- There is ad-hoc IMU triage but no system-level coordination of what triage means
- **No sleep or idle optimization** -- Battery-powered pods will drain quickly

This document defines a **System Mode Manager** that layers on top of the existing `FeatureManager` to provide modal behavior, power-aware polling, and structured triage.

---

## Design Principles

1. **Additive, not replacing** -- The `FeatureManager` atomic bitmask remains the authority on feature state. The ModeManager orchestrates which features are enabled in which mode, but `FeatureManager.isEnabled()` is still how services check state.
2. **Practical increments** -- Phase 1 works without hardware power management (`CONFIG_PM_ENABLE=n`). Power gating is Phase 2.
3. **Observable from CLI** -- Every mode transition is queryable and triggerable from `domes-cli` over any transport.
4. **Deterministic transitions** -- The state machine validates all transitions. Invalid transitions are logged and rejected.
5. **Hardware-agnostic** -- The mode manager does not assume battery ADC exists. Battery monitoring is optional and board-gated.
6. **Role-agnostic** -- The ModeManager operates identically on master and slave pods. Role assignment (see doc 12) is orthogonal to system mode.

---

## System Mode State Machine (SystemMode)

```
+-------------------------------------------------------------------------+
|                        SYSTEM MODE STATE MACHINE                        |
+-------------------------------------------------------------------------+
|                                                                         |
|    +-----------+                                                        |
|    |  BOOTING  |                                                        |
|    +-----+-----+                                                        |
|          | init complete                                                |
|          v                                                              |
|    +-----------+  CLI message    +-----------+                          |
|    |   IDLE    |<-------------->|  TRIAGE   |                          |
|    +-----+-----+  idle timeout  +-----------+                          |
|          |                                                              |
|          | BLE connect (master)                                         |
|          | ESP-NOW JOIN_GAME (slave)                                     |
|          | long-press (solo drill)                                       |
|          v                                                              |
|    +-----------+  START_DRILL   +-----------+                          |
|    | CONNECTED |--------------->|   GAME    |                          |
|    +-----+-----+               +-----+-----+                          |
|          ^                           |                                  |
|          |    drill done / STOP_ALL  |                                  |
|          +---------------------------+                                  |
|                                                                         |
|    +-----------+                                                        |
|    |   ERROR   |---- recovery timeout ----> IDLE                        |
|    +-----------+                                                        |
|                                                                         |
|    Any state ---- critical fault ----> ERROR                            |
|    Any state ---- OTA begin ----> (pauses mode; OTA runs in transport)  |
|                                                                         |
|    IDLE ---- long-press ----> GAME (solo drill, no CONNECTED phase)     |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Mode Definitions

| Mode | Purpose | Entry Trigger | Exit Trigger |
|------|---------|---------------|--------------|
| **BOOTING** | Hardware init, self-test, OTA verify | Power-on / reset | Init complete |
| **IDLE** | Minimum-power standby, BLE advertising only | Init complete, peer disconnect, drill stop, triage timeout | CLI message, BLE connect, ESP-NOW join, long-press |
| **TRIAGE** | Hardware validation via CLI (any transport) | CLI config message received | CLI idle timeout (30s) or explicit exit |
| **CONNECTED** | Paired with master/phone, ready for drills | BLE peer established (master), ESP-NOW JOIN_GAME (slave) | Peer lost, drill starts |
| **GAME** | Active drill execution (latency-critical). GameState FSM (doc 05) is active. | START_DRILL command, or long-press solo drill | Drill complete, STOP_ALL, game timeout |
| **ERROR** | Fault recovery, red LED, limited function | Critical fault (watchdog, alloc, driver failure) | Recovery timeout (10s) -> IDLE |

### Valid Transitions

```cpp
bool isValidTransition(SystemMode from, SystemMode to) {
    switch (from) {
        case SystemMode::kBooting:
            return to == SystemMode::kIdle
                || to == SystemMode::kError;

        case SystemMode::kIdle:
            return to == SystemMode::kTriage
                || to == SystemMode::kConnected
                || to == SystemMode::kGame       // solo drill (long-press)
                || to == SystemMode::kError;

        case SystemMode::kTriage:
            return to == SystemMode::kIdle
                || to == SystemMode::kConnected
                || to == SystemMode::kError;

        case SystemMode::kConnected:
            return to == SystemMode::kIdle
                || to == SystemMode::kTriage
                || to == SystemMode::kGame
                || to == SystemMode::kError;

        case SystemMode::kGame:
            return to == SystemMode::kConnected
                || to == SystemMode::kIdle       // solo drill ends
                || to == SystemMode::kError;

        case SystemMode::kError:
            return to == SystemMode::kIdle;
    }
    return false;
}
```

**Note on IDLE -> GAME (solo drill):** A pod can enter GAME mode directly from IDLE via long-press, without a phone or master. In this case the pod acts as both master and sole target -- it runs a built-in drill (see doc 05, §Drill Types) against itself. When the drill completes, it returns to IDLE (not CONNECTED, since there is no peer).

**Note on GAME exit destination:** The exit destination depends on how GAME was entered:
- Entered from CONNECTED (peer drill) -> exits to CONNECTED
- Entered from IDLE (solo drill) -> exits to IDLE

---

## Mode -> Feature Mapping

Each mode defines which features are **active** (enabled in `FeatureManager`) and at what polling rates services should operate.

| Feature | BOOTING | IDLE | TRIAGE | CONNECTED | GAME | ERROR |
|---------|---------|------|--------|-----------|------|-------|
| LED_EFFECTS | OFF | BREATHE | ON | ON | ON | RED_PULSE |
| BLE_ADVERTISING | OFF | ON | ON | ON (connected) | ON | ON |
| WIFI | OFF | OFF | ON (if available) | ON (if available) | OFF | OFF |
| ESP_NOW | OFF | OFF | OFF | ON | ON | OFF |
| TOUCH | OFF | OFF | ON | ON | ON | OFF |
| HAPTIC | OFF | OFF | ON | ON | ON | OFF |
| AUDIO | OFF | OFF | ON | ON | ON | OFF |

### Polling Rate Profiles

Services currently poll at fixed rates. The ModeManager introduces **rate profiles** that adjust polling frequency per mode to save power when responsiveness is not needed.

| Service | IDLE | TRIAGE | CONNECTED | GAME |
|---------|------|--------|-----------|------|
| LED Service | 100ms (10 FPS, breathe only) | 16ms (60 FPS) | 16ms (60 FPS) | 16ms (60 FPS) |
| Touch Service | OFF | 10ms (100 Hz) | 10ms (100 Hz) | 10ms (100 Hz) |
| IMU Service | OFF | 10ms (100 Hz) | 10ms (100 Hz) | 5ms (200 Hz) |
| Audio Service | OFF | 50ms (queue check) | 50ms (queue check) | 50ms (queue check) |
| Serial OTA | 100ms (always on) | 100ms (always on) | 100ms (always on) | 100ms (always on) |
| BLE OTA | 100ms (always on) | 100ms (always on) | 100ms (always on) | 100ms (always on) |

**Transport tasks (Serial OTA, BLE OTA) are always active** regardless of mode because they carry config/OTA traffic. They cannot be disabled without losing device management capability.

---

## Mode Behaviors

### BOOTING

Duration: ~1-2 seconds. All hardware drivers initialize, NVS is read, self-test runs. No features are enabled. LEDs are dark. After all init completes, the ModeManager transitions to IDLE.

If init fails (driver error, corrupt NVS, etc.), the ModeManager transitions to ERROR instead.

### IDLE

The default resting state. The pod consumes minimum power while remaining discoverable:

1. **LED**: Slow breathing animation (2s period, low brightness) to show the device is alive
2. **Touch/IMU/Audio/Haptic**: Disabled (features turned off in FeatureManager)
3. **BLE**: Advertising active -- waiting for `domes-cli --ble` or phone connection
4. **WiFi**: Disabled unless `CONFIG_DOMES_WIFI_AUTO_CONNECT` is set
5. **ESP-NOW**: Disabled (no peer network until CONNECTED)
6. **Transport receivers**: Still polling for incoming frames (serial OTA, BLE OTA)

**Exit triggers:**
- CLI message received (any transport) -> TRIAGE
- BLE peer connects (phone app) -> CONNECTED (this pod becomes master, see doc 12)
- ESP-NOW JOIN_GAME from master -> CONNECTED then GAME (this pod is a slave, see doc 12)
- Long-press on touch pad -> GAME (solo drill, see §Solo Drills below)

#### Idle Power Budget (Estimated)

| Component | Current Draw | Notes |
|-----------|-------------|-------|
| ESP32-S3 active (WiFi off) | ~40 mA | CPU at 240 MHz, no PM |
| BLE advertising | ~5 mA avg | 100ms interval |
| LED breathing | ~5 mA avg | Low brightness, 1 LED pulsing |
| **Total IDLE** | **~50 mA** | **Without ESP-IDF power management** |

Phase 2 with `CONFIG_PM_ENABLE` and automatic light sleep could reduce this to ~15-20 mA.

### TRIAGE

The primary hardware validation flow. Activates when a CLI tool connects (via any transport: USB serial, BLE, or WiFi) and enables all sensors for systematic testing.

**Entry:** Any CLI config message received while in IDLE or CONNECTED. The `ConfigCommandHandler` detects this and calls `modeManager.transitionTo(SystemMode::kTriage)`.

**Timeout:** If no CLI message is received for 30 seconds, the ModeManager auto-transitions back to IDLE. Each incoming message resets the timer.

#### Triage on Slave Pods

Doc 12 defines that only the master pod has BLE from the phone. However, **any pod can be triaged individually** via:
- **USB serial**: `domes-cli --port /dev/ttyACM0 feature list` -- always available, no BLE needed
- **WiFi TCP**: `domes-cli --wifi <IP>:5000 feature list` -- if WiFi is enabled
- **BLE direct**: `domes-cli --ble "DOMES-Pod-3"` -- when the pod is not in a game session

Triage is a per-pod operation. It does not require or interact with the multi-pod orchestration layer.

#### Triage CLI Sequence

```bash
# Step 1: Connect to device
domes-cli --port /dev/ttyACM0       # Serial
domes-cli --ble "DOMES-Pod"         # BLE
domes-cli --wifi 192.168.1.x:5000   # WiFi

# Step 2: Identify -- list all features and their states
domes-cli feature list
# Output:
#   led-effects: enabled
#   ble: enabled
#   wifi: disabled
#   esp-now: disabled
#   touch: enabled
#   haptic: disabled (no driver)
#   audio: enabled

# Step 3: Per-feature validation
domes-cli led set solid --color ff0000   # Red -- visually verify
domes-cli led set solid --color 00ff00   # Green
domes-cli led set solid --color 0000ff   # Blue
domes-cli led set breathing              # Breathing animation
domes-cli led set off                    # LEDs off

domes-cli imu triage on                  # Enable tap detection
# Tap pod -- expect white flash + beep per tap
domes-cli imu triage off                 # Disable

# Step 4: System info
domes-cli system mode                    # Show current SystemMode
domes-cli system info                    # Firmware version, uptime, heap, boot count

# Step 5: Done -- disconnect
# Device auto-returns to IDLE after 30s with no CLI traffic
```

#### Triage Timeout

```cpp
namespace mode_timing {
constexpr uint32_t kTriageTimeoutMs = 30'000;   // 30s idle -> IDLE mode
constexpr uint32_t kErrorRecoveryMs = 10'000;   // 10s in ERROR -> IDLE
constexpr uint32_t kGameTimeoutMs   = 300'000;  // 5min max drill
}
```

### CONNECTED

The pod is paired with a peer and ready for drills. How the pod reaches CONNECTED depends on its role (see doc 12):

- **Master pod**: BLE connection from phone app triggers IDLE -> CONNECTED. The master then starts ESP-NOW discovery to find other pods.
- **Slave pod**: Receives ESP-NOW JOIN_GAME from master, transitions IDLE -> CONNECTED.

In CONNECTED mode, ESP-NOW is active for inter-pod communication. The pod waits for a START_DRILL command (from phone via BLE on master, or from master via ESP-NOW on slave) to enter GAME.

**Exit triggers:**
- START_DRILL received -> GAME
- Peer lost (BLE disconnect on master, ESP-NOW timeout on slave) -> IDLE
- CLI message received -> TRIAGE

### GAME

Active drill execution. This is the **latency-critical** mode. The GameState FSM (see doc 05, §Game State Machine) is active within this mode:

```
SystemMode:   ... CONNECTED ---> GAME -------------------------------------> CONNECTED
                                  |                                              ^
GameState:                  Ready -> Armed -> Triggered -> Feedback -> Ready     |
(doc 05)                      ^                              |                   |
                              +----- next round -------------+                   |
                                                                                 |
                              Ready (final) --- drill complete -----------------+
```

The **ModeManager** controls which services are powered on (touch, audio, haptic, LEDs, ESP-NOW). The **GameEngine** (doc 05) controls game logic within those services (when to arm touch, how to measure reaction time, what feedback to play).

| Concern | ModeManager (this doc) | GameEngine (doc 05) |
|---------|----------------------|---------------------|
| Scope | Device lifecycle | Single drill round |
| Controls | Feature enables, polling rates, power | Arm/touch/feedback cycle |
| Transition triggers | BLE connect, CLI, fault | arm() call, touch event, timeout |
| Persistence | Survives across drills | Resets each round |

**On entering GAME:** ModeManager applies the GAME feature mask (all sensors enabled, WiFi off). GameEngine initializes to `GameState::kReady`.

**On exiting GAME:** ModeManager applies the CONNECTED or IDLE feature mask. GameEngine is `disarm()`-ed.

### ERROR

Critical fault recovery mode. Only BLE advertising and a red pulsing LED are active. After 10 seconds, auto-recovers to IDLE.

**Entry triggers:**
- Task watchdog timeout
- Memory allocation failure
- Driver initialization failure
- Unrecoverable communication error

**Error propagation in multi-pod context (see doc 12):** If a slave pod enters ERROR during a drill, it sends an `kErrorReport` message to the master via ESP-NOW (best-effort, since ESP-NOW is being disabled). The master's DrillInterpreter can skip that pod for remaining rounds or abort the drill, depending on configuration.

---

## Solo Drills (IDLE -> GAME without Phone)

A pod can run drills without a phone connection or master coordination. This covers two scenarios:

### 1. Long-Press Solo Drill

The user long-presses the touch pad while in IDLE. The pod transitions directly to GAME and runs a built-in reaction drill (see doc 05, §Drill Types) against itself -- acting as both the DrillInterpreter and the sole target.

```
User long-press  -->  IDLE -> GAME  -->  GameState cycles (Ready/Armed/Triggered/Feedback)
                                    -->  Built-in drill completes  -->  GAME -> IDLE
```

In this mode:
- No ESP-NOW is needed (single pod)
- No phone is needed (built-in drill config)
- No clock sync is needed (local timing only)
- The pod runs a default `ReactionDrillConfig` from doc 05

### 2. Drill Repeat Without Phone

After a phone-initiated drill completes, the master pod retains the drill program in RAM. A long-press on the master re-enters GAME and re-executes the last drill, sending commands to the same slave pods via ESP-NOW.

```
Drill completes  -->  GAME -> CONNECTED  -->  Long-press  -->  CONNECTED -> GAME
                                                           -->  Same drill re-executes
```

This path goes through CONNECTED (peers are still known) rather than IDLE.

---

## Integration with Existing Code

### ModeManager Class

The ModeManager wraps a `SystemMode` state machine and holds a reference to `FeatureManager`. On each mode transition, it applies the corresponding feature mask.

```cpp
// config/modeManager.hpp
#pragma once

#include "configProtocol.hpp"
#include "featureManager.hpp"

#include <atomic>
#include <cstdint>
#include <functional>

namespace domes::config {

enum class SystemMode : uint8_t {
    kBooting    = 0,
    kIdle       = 1,
    kTriage     = 2,
    kConnected  = 3,
    kGame       = 4,
    kError      = 5,
    kCount
};

const char* modeToString(SystemMode mode);

/**
 * @brief Callback invoked on mode transitions
 *
 * Called AFTER the new mode's feature mask has been applied.
 */
using ModeTransitionCallback = std::function<void(SystemMode from, SystemMode to)>;

class ModeManager {
public:
    explicit ModeManager(FeatureManager& features);

    /**
     * @brief Transition to a new system mode
     *
     * Validates the transition, applies the feature mask, and invokes
     * the transition callback if registered.
     *
     * Thread-safe: uses atomic compare-exchange for mode transitions.
     *
     * @param newMode Target mode
     * @return true if transition succeeded
     */
    bool transitionTo(SystemMode newMode);

    /**
     * @brief Get current system mode (lock-free read)
     */
    SystemMode currentMode() const {
        return mode_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get time in current mode (microseconds)
     */
    uint32_t timeInModeUs() const;

    /**
     * @brief Register transition callback
     */
    void onTransition(ModeTransitionCallback callback);

    /**
     * @brief Periodic tick -- checks timeouts and auto-transitions
     *
     * Call from the main system task at ~10 Hz.
     * Handles:
     * - Triage timeout -> IDLE
     * - Error recovery timeout -> IDLE
     * - Game timeout -> CONNECTED or IDLE (depending on entry path)
     */
    void tick();

    /**
     * @brief Reset the activity timer (called on CLI message receipt)
     *
     * Prevents triage timeout while the user is actively working.
     */
    void resetActivityTimer();

    /**
     * @brief Track how GAME mode was entered (for correct exit destination)
     */
    SystemMode gameEnteredFrom() const { return gameEnteredFrom_; }

private:
    FeatureManager& features_;
    std::atomic<SystemMode> mode_{SystemMode::kBooting};
    std::atomic<uint32_t> modeEnteredAtUs_{0};
    std::atomic<uint32_t> lastActivityAtUs_{0};
    SystemMode gameEnteredFrom_{SystemMode::kIdle};
    ModeTransitionCallback transitionCb_;

    void applyFeatureMask(SystemMode mode);
    bool isValidTransition(SystemMode from, SystemMode to) const;
};

}  // namespace domes::config
```

### Feature Masks (Compile-Time Constants)

```cpp
// config/modeManager.cpp (partial)
namespace {

// Feature bitmask constants: bit N = Feature(N) enabled
// Feature IDs from config.proto: LED=1, BLE=2, WiFi=3, ESP_NOW=4, Touch=5, Haptic=6, Audio=7

constexpr uint32_t bit(uint8_t n) { return 1u << n; }

// BOOTING: Nothing enabled (drivers initializing)
constexpr uint32_t kBootingMask = 0;

// IDLE: BLE advertising + LED breathing only
constexpr uint32_t kIdleMask =
    bit(1) |  // LED_EFFECTS (breathing pattern)
    bit(2);   // BLE_ADVERTISING

// TRIAGE: Everything except ESP-NOW (no peer network during triage)
constexpr uint32_t kTriageMask =
    bit(1) |  // LED_EFFECTS
    bit(2) |  // BLE_ADVERTISING
    bit(3) |  // WIFI (if available)
    bit(5) |  // TOUCH
    bit(6) |  // HAPTIC
    bit(7);   // AUDIO

// CONNECTED: Everything except WiFi (BLE + ESP-NOW for game comms)
constexpr uint32_t kConnectedMask =
    bit(1) |  // LED_EFFECTS
    bit(2) |  // BLE_ADVERTISING
    bit(4) |  // ESP_NOW
    bit(5) |  // TOUCH
    bit(6) |  // HAPTIC
    bit(7);   // AUDIO

// GAME: Full sensor suite, no WiFi (latency-sensitive)
constexpr uint32_t kGameMask =
    bit(1) |  // LED_EFFECTS
    bit(2) |  // BLE_ADVERTISING
    bit(4) |  // ESP_NOW
    bit(5) |  // TOUCH
    bit(6) |  // HAPTIC
    bit(7);   // AUDIO

// ERROR: BLE advertising + error LED only
constexpr uint32_t kErrorMask =
    bit(1) |  // LED_EFFECTS (red pulse)
    bit(2);   // BLE_ADVERTISING

constexpr uint32_t kModeMasks[] = {
    kBootingMask,     // kBooting
    kIdleMask,        // kIdle
    kTriageMask,      // kTriage
    kConnectedMask,   // kConnected
    kGameMask,        // kGame
    kErrorMask,       // kError
};

}  // anonymous namespace

void ModeManager::applyFeatureMask(SystemMode mode) {
    features_.setMask(kModeMasks[static_cast<size_t>(mode)]);
}
```

### Integration Point: main.cpp

The ModeManager is created after `FeatureManager` and transitioned to `kIdle` once all init is complete:

```cpp
// In app_main(), after existing init sequence:

// After: initFeatureManager();
static domes::config::ModeManager modeManager(*featureManager);
// modeManager starts in kBooting

// ... all other init ...

// At the end of app_main(), where green LED is currently set:
modeManager.transitionTo(domes::config::SystemMode::kIdle);
ESP_LOGI(kTag, "System mode: IDLE");
```

### Integration Point: ConfigCommandHandler

When any config message arrives, the handler notifies the ModeManager to reset the activity timer and auto-transition to TRIAGE if in IDLE:

```cpp
void ConfigCommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    // Existing dispatch logic...

    // Notify mode manager of CLI activity
    if (modeManager_) {
        modeManager_->resetActivityTimer();
        if (modeManager_->currentMode() == SystemMode::kIdle) {
            modeManager_->transitionTo(SystemMode::kTriage);
        }
    }
}
```

### Integration Point: BLE Connection (Master Role)

When a phone connects via BLE, the pod becomes master (see doc 12, §Discovery) and transitions to CONNECTED:

```cpp
// In BLE gap event handler:
void onBleConnect(uint16_t connHandle) {
    if (modeManager_->currentMode() == SystemMode::kIdle
        || modeManager_->currentMode() == SystemMode::kTriage) {
        modeManager_->transitionTo(SystemMode::kConnected);
        // This pod is now the master — start ESP-NOW discovery (doc 12)
        commService_.startDiscovery();
    }
}
```

### Integration Point: ESP-NOW JOIN_GAME (Slave Role)

When a slave pod receives JOIN_GAME from a master (see doc 12, §Drill Setup), it transitions through CONNECTED to GAME:

```cpp
// In PodCommandHandler (ESP-NOW receive):
void onJoinGame(const uint8_t* masterMac) {
    modeManager_->transitionTo(SystemMode::kConnected);
    modeManager_->transitionTo(SystemMode::kGame);
    // GameEngine initializes to GameState::kReady (doc 05)
    gameEngine_->init();
}
```

### Integration Point: Service Rate Profiles

Services check the current mode to adjust their polling delay. This does **not** require changing the service interface -- each service already has a feature-disabled idle path:

```cpp
// In LedService::run() — existing code already has this structure:
while (running_) {
    if (features_.isEnabled(config::Feature::kLedEffects)) {
        updateAnimation();
        driver_.refresh();
        vTaskDelay(pdMS_TO_TICKS(16));   // 60 FPS
    } else {
        driver_.clear();
        driver_.refresh();
        vTaskDelay(pdMS_TO_TICKS(100));  // Idle
    }
}

// The mode manager disabling LED_EFFECTS in IDLE already triggers the
// 100ms idle path. For the IDLE breathing effect, the mode manager
// keeps LED_EFFECTS enabled but the LedService detects IDLE mode
// and runs a slow breathing pattern instead of the user-set pattern.
```

---

## Protocol Extension

### New Message Types

Add to `config.proto`:

```protobuf
// System mode management (0x30-0x3F range)
enum MsgType {
    // ... existing 0x20-0x2B ...
    MSG_TYPE_GET_MODE_REQ = 0x30;
    MSG_TYPE_GET_MODE_RSP = 0x31;
    MSG_TYPE_SET_MODE_REQ = 0x32;
    MSG_TYPE_SET_MODE_RSP = 0x33;
    MSG_TYPE_GET_SYSTEM_INFO_REQ = 0x34;
    MSG_TYPE_GET_SYSTEM_INFO_RSP = 0x35;
}

// System operating modes — matches SystemMode enum in firmware
enum SystemMode {
    SYSTEM_MODE_BOOTING = 0;
    SYSTEM_MODE_IDLE = 1;
    SYSTEM_MODE_TRIAGE = 2;
    SYSTEM_MODE_CONNECTED = 3;
    SYSTEM_MODE_GAME = 4;
    SYSTEM_MODE_ERROR = 5;
}

message GetModeRequest {
    // Empty
}

message GetModeResponse {
    SystemMode mode = 1;
    uint32 time_in_mode_ms = 2;
}

message SetModeRequest {
    SystemMode mode = 1;
}

message SetModeResponse {
    SystemMode mode = 1;          // Actual mode after request
    bool transition_ok = 2;       // Whether transition succeeded
}

message GetSystemInfoRequest {
    // Empty
}

message GetSystemInfoResponse {
    string firmware_version = 1;
    uint32 uptime_s = 2;
    uint32 free_heap = 3;
    uint32 boot_count = 4;
    SystemMode mode = 5;
    uint32 feature_mask = 6;
}
```

**Note:** These config protocol messages use protobuf over the framed transport (`[0xAA][0x55][Len][Type][Payload][CRC32]`). This is separate from the ESP-NOW game protocol (packed structs, see doc 04, §Protocol Messages) used for inter-pod communication during drills.

### CLI Commands (New)

```bash
# Query current mode
domes-cli system mode
# Output: IDLE (12.3s)

# Force mode transition (for testing)
domes-cli system mode set triage
# Output: Mode: IDLE -> TRIAGE (ok)

# System info
domes-cli system info
# Output:
#   Firmware: v0.3.0
#   Uptime: 142s
#   Heap: 187432 bytes
#   Boot count: 47
#   Mode: TRIAGE
#   Features: led-effects ble wifi touch audio
```

---

## OTA During Any Mode

OTA updates are handled at the **transport layer** and bypass the ModeManager entirely. The existing `SerialOtaReceiver` and `BleOtaReceiver` dispatch OTA frames (type 0x01-0x05) directly to the OTA handler regardless of system mode.

This is correct behavior: the user should be able to flash firmware regardless of what mode the device is in. The ModeManager does not interfere with OTA.

After OTA completes, the device reboots and goes through BOOTING -> IDLE as normal.

---

## Battery Monitoring (Phase 2, DOMES_V1 Board Only)

The DOMES Pod v1 PCB has:
- Battery voltage ADC on `GPIO_NUM_5` (`ADC_CHANNEL_4`)
- Charge status input on `GPIO_NUM_8` (not available on NFF devboard)

**Note**: The NFF devboard uses GPIO5 for `kImuInt1` (IMU interrupt). Battery monitoring is only applicable to `BOARD_DOMES_V1`.

### Low Battery Behavior

| Battery Level | Action |
|---------------|--------|
| > 20% | Normal operation |
| 10-20% | LED warning (amber pulse every 30s), limit to IDLE/TRIAGE |
| < 10% | Force IDLE, disable all non-essential features |
| < 5% | Controlled shutdown (save state to NVS, deep sleep) |

### Battery Service (Future)

```cpp
// services/batteryService.hpp -- Phase 2 only
class BatteryService {
public:
    BatteryService(ModeManager& modeManager);

    uint8_t getPercentage() const;
    bool isCharging() const;

    // Called at 1 Hz
    void tick();

private:
    ModeManager& modeManager_;
    // ADC reading, filtering, voltage-to-percentage mapping
};
```

---

## Phase 2: ESP-IDF Power Management

Currently `CONFIG_PM_ENABLE` is not set in `sdkconfig.defaults`. When enabled in Phase 2:

### Automatic Light Sleep

```cpp
// Phase 2: Enable power management in sdkconfig.defaults
// CONFIG_PM_ENABLE=y
// CONFIG_FREERTOS_USE_TICKLESS_IDLE=y

// In initInfrastructure():
esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,     // Minimum CPU freq when idle
    .light_sleep_enable = true
};
esp_pm_configure(&pm_config);
```

### Mode-Aware Power Locks

```cpp
// GAME mode acquires a power lock to prevent light sleep
// (latency-critical: touch response must be < 10ms)
esp_pm_lock_handle_t gameLock;
esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "game", &gameLock);

void ModeManager::applyFeatureMask(SystemMode mode) {
    features_.setMask(kModeMasks[static_cast<size_t>(mode)]);

    if (mode == SystemMode::kGame) {
        esp_pm_lock_acquire(gameLock);
    } else {
        esp_pm_lock_release(gameLock);
    }
}
```

---

## NVS Persistence

### New NVS Keys

Add to `infra/nvsConfig.hpp`:

```cpp
namespace mode_key {
constexpr const char* kLastMode = "last_mode";          // uint8_t -- last mode before shutdown
constexpr const char* kModeTransitions = "mode_trans";   // uint32_t -- total transitions
}
```

### Boot Recovery

On boot, the ModeManager reads `last_mode` from NVS. If the last mode was ERROR, it logs a warning but still starts in IDLE (clean slate). The `mode_trans` counter tracks total transitions for telemetry.

---

## Implementation Phases

### Phase 1: Core Mode Manager (Immediate)

**Files to create:**
- `firmware/domes/main/config/modeManager.hpp`
- `firmware/domes/main/config/modeManager.cpp`

**Files to modify:**
- `firmware/common/proto/config.proto` -- Add SystemMode enum, Get/SetMode messages
- `firmware/common/proto/config.options` -- Add size constraints for new messages
- `firmware/domes/main/config/configProtocol.hpp` -- Add MsgType wrappers for 0x30-0x35
- `firmware/domes/main/config/configCommandHandler.hpp/.cpp` -- Add mode query/set dispatch, activity timer reset
- `firmware/domes/main/main.cpp` -- Create ModeManager, transition to IDLE after init
- `tools/domes-cli/src/commands/` -- Add `system` subcommand (mode, info)

**Scope:**
- SystemMode enum and ModeManager class
- Feature mask application on transitions
- Triage auto-entry on CLI message
- Triage timeout -> IDLE
- Error recovery timeout -> IDLE
- Solo drill (IDLE -> GAME via long-press)
- Protocol messages for GET_MODE and SET_MODE
- CLI `system mode` and `system info` commands

### Phase 2: Power Optimization (After V1 PCB)

**Files to create:**
- `firmware/domes/main/services/batteryService.hpp`
- `firmware/domes/main/services/batteryService.cpp`

**Files to modify:**
- `firmware/domes/sdkconfig.defaults` -- Enable `CONFIG_PM_ENABLE`, `CONFIG_FREERTOS_USE_TICKLESS_IDLE`
- `firmware/domes/main/config.hpp` -- Add battery pin config for BOARD_DOMES_V1
- Service files -- Add mode-aware polling rate adjustment

**Scope:**
- Battery ADC reading and filtering
- Low-battery mode enforcement
- ESP-IDF power management configuration
- Automatic light sleep in IDLE
- GAME mode power lock (prevent sleep)
- Mode-aware polling rate profiles in services

### Phase 3: Game Integration (With Game Engine and Multi-Pod)

**Files to modify:**
- `firmware/domes/main/config/modeManager.hpp` -- Add GameEngine hooks
- `firmware/domes/main/game/` -- GameEngine implementation (see doc 05)
- Integration with DrillInterpreter (see doc 12)

**Scope:**
- CONNECTED -> GAME transition on START_DRILL
- GAME -> CONNECTED on drill complete
- GameState FSM (doc 05) operating within GAME mode
- ESP-NOW peer coordination during GAME mode (doc 12)

---

## Testing Strategy

### Unit Tests (Phase 1)

```cpp
// test_app/main/test_mode_manager.cpp

TEST(ModeManager, StartsInBooting) {
    FeatureManager features;
    ModeManager manager(features);
    EXPECT_EQ(manager.currentMode(), SystemMode::kBooting);
}

TEST(ModeManager, BootToIdle) {
    FeatureManager features;
    ModeManager manager(features);
    EXPECT_TRUE(manager.transitionTo(SystemMode::kIdle));
    EXPECT_EQ(manager.currentMode(), SystemMode::kIdle);
    // Verify IDLE feature mask
    EXPECT_TRUE(features.isEnabled(Feature::kLedEffects));
    EXPECT_TRUE(features.isEnabled(Feature::kBleAdvertising));
    EXPECT_FALSE(features.isEnabled(Feature::kTouch));
    EXPECT_FALSE(features.isEnabled(Feature::kAudio));
}

TEST(ModeManager, InvalidTransitionRejected) {
    FeatureManager features;
    ModeManager manager(features);
    // Cannot go directly from BOOTING to GAME
    EXPECT_FALSE(manager.transitionTo(SystemMode::kGame));
    EXPECT_EQ(manager.currentMode(), SystemMode::kBooting);
}

TEST(ModeManager, TriageEnablesAllSensors) {
    FeatureManager features;
    ModeManager manager(features);
    manager.transitionTo(SystemMode::kIdle);
    manager.transitionTo(SystemMode::kTriage);
    EXPECT_TRUE(features.isEnabled(Feature::kTouch));
    EXPECT_TRUE(features.isEnabled(Feature::kAudio));
    EXPECT_TRUE(features.isEnabled(Feature::kHaptic));
}

TEST(ModeManager, TriageTimeout) {
    FeatureManager features;
    ModeManager manager(features);
    manager.transitionTo(SystemMode::kIdle);
    manager.transitionTo(SystemMode::kTriage);
    // Simulate 30s passing without activity
    advanceClock(30'000'000);  // 30s in microseconds
    manager.tick();
    EXPECT_EQ(manager.currentMode(), SystemMode::kIdle);
}

TEST(ModeManager, SoloDrillFromIdle) {
    FeatureManager features;
    ModeManager manager(features);
    manager.transitionTo(SystemMode::kIdle);
    // Long-press triggers IDLE -> GAME directly
    EXPECT_TRUE(manager.transitionTo(SystemMode::kGame));
    EXPECT_EQ(manager.currentMode(), SystemMode::kGame);
    // GAME feature mask: touch, audio, haptic all enabled
    EXPECT_TRUE(features.isEnabled(Feature::kTouch));
    EXPECT_TRUE(features.isEnabled(Feature::kAudio));
}

TEST(ModeManager, SoloDrillExitsToIdle) {
    FeatureManager features;
    ModeManager manager(features);
    manager.transitionTo(SystemMode::kIdle);
    manager.transitionTo(SystemMode::kGame);
    // Solo drill: entered from IDLE, exits to IDLE
    EXPECT_TRUE(manager.transitionTo(SystemMode::kIdle));
    EXPECT_EQ(manager.currentMode(), SystemMode::kIdle);
}

TEST(ModeManager, PeerDrillExitsToConnected) {
    FeatureManager features;
    ModeManager manager(features);
    manager.transitionTo(SystemMode::kIdle);
    manager.transitionTo(SystemMode::kConnected);
    manager.transitionTo(SystemMode::kGame);
    // Peer drill: entered from CONNECTED, exits to CONNECTED
    EXPECT_TRUE(manager.transitionTo(SystemMode::kConnected));
    EXPECT_EQ(manager.currentMode(), SystemMode::kConnected);
}
```

### Hardware Verification (Phase 1)

```bash
# Flash firmware with mode manager
/flash

# Verify IDLE mode on boot
# Expected: slow breathing LED, BLE advertising visible

# Connect via CLI -- should auto-transition to TRIAGE
domes-cli --port /dev/ttyACM0 system mode
# Expected: TRIAGE

# Verify all sensors active in TRIAGE
domes-cli --port /dev/ttyACM0 feature list
# Expected: led, ble, touch, haptic, audio all enabled

# Wait 30s without sending commands
# Expected: device returns to IDLE (breathing LED)

# Query mode again
domes-cli --port /dev/ttyACM0 system mode
# Expected: TRIAGE (because the query itself resets the timer)
```

---

## Summary

| Aspect | Current State | After Phase 1 |
|--------|--------------|---------------|
| Boot behavior | Init -> all services on | Init -> IDLE (minimal) |
| CLI connect | No mode change | Auto-transition to TRIAGE |
| Feature control | Manual per-feature | Mode-driven bulk enable/disable |
| Power awareness | None | Feature masks reduce active peripherals |
| Triage workflow | Ad-hoc IMU toggle | Structured mode with timeout |
| Game mode | Not implemented | SystemMode::GAME with GameState FSM (doc 05) |
| Solo drill | Not implemented | IDLE -> GAME via long-press |
| Multi-pod coordination | Not implemented | Mode transitions driven by doc 12 events |
| Error handling | Log and continue | ERROR mode with recovery timeout |
| Observability | `feature list` only | `system mode`, `system info` |

---

*Prerequisites: 03-driver-development.md, 04-communication.md, 05-game-engine.md*
*Companions: 05-game-engine.md, 12-multi-pod-orchestration.md*
*Related: 09-reference.md (pin mappings), firmware/common/proto/config.proto*
