# 12 - Multi-Pod Orchestration

## AI Agent Instructions

Load this file when:
- Implementing ESP-NOW CommService discovery and peer management
- Building the DrillInterpreter or PodCommandHandler
- Designing phone app <-> pod communication
- Working on pod discovery, pairing, or role assignment
- Implementing drill execution, result collection, or reporting

Prerequisites: `04-communication.md`, `05-game-engine.md`, `11-system-modes.md`
Companions: `05-game-engine.md` (per-pod game logic), `11-system-modes.md` (per-pod lifecycle)

---

## How This Document Fits

Three documents define the DOMES runtime architecture. Each operates at a different scope:

```
+---------------------------------------------------------------+
|  Doc 12: Multi-Pod Orchestration            << THIS DOC       |
|  Coordinates pods: discovery, roles, drill routing, results   |
|                                                               |
|  +----------------------------------------------------------+|
|  |  Doc 11: System Modes                                     ||
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
| **11 - System Modes** | Single pod, full lifecycle | SystemMode FSM, feature masks, power management |
| **12 - Multi-Pod Orchestration (this doc)** | Multiple pods, full session | Roles, discovery, drill programs, result collection |

**This document** defines how multiple pods work together to run a training session. It introduces the concepts of **master** and **slave** roles, the **DrillInterpreter** that executes drill programs, and the **session lifecycle** from phone connection through drill execution to results reporting. It drives the SystemMode transitions (doc 11) across pods and orchestrates the GameState FSM (doc 05) on each pod.

### Shared Terminology

These terms are used consistently across docs 05, 11, and 12:

| Term | Definition | Authoritative Doc |
|------|-----------|-------------------|
| **SystemMode** | Device lifecycle state (BOOTING, IDLE, TRIAGE, CONNECTED, GAME, ERROR) | Doc 11 |
| **GameState** | Per-pod, per-round game state (Ready, Armed, Triggered, Feedback) | Doc 05 |
| **ModeManager** | Class that manages SystemMode transitions and feature masks | Doc 11 |
| **GameEngine** | Class that manages GameState transitions and touch detection on a single pod | Doc 05 |
| **DrillInterpreter** | Master-only class that steps through a drill program and sends commands to pods | Doc 12 (this doc) |
| **PodCommandHandler** | Slave-side class that receives ESP-NOW commands and dispatches to local services | Doc 12 (this doc) |
| **Primitive** | Atomic pod action: SET_COLOR, ARM_TOUCH, PLAY_SOUND, FIRE_HAPTIC | Doc 05 / Doc 04 |
| **CommService** | ESP-NOW send/receive singleton for inter-pod communication | Doc 04 |
| **Master** | Pod connected to phone via BLE; runs DrillInterpreter | Doc 12 (this doc) |
| **Slave** | Pod receiving commands from master via ESP-NOW | Doc 12 (this doc) |

---

## Overview

### Actors and Roles

| Actor | Role | Communication |
|-------|------|---------------|
| **Phone app** (iOS/Android) | User interface, drill selection, results display | BLE GATT to master pod |
| **Master pod** | BLE bridge to phone, ESP-NOW coordinator, DrillInterpreter | BLE to phone, ESP-NOW to slaves |
| **Slave pods** (1-N) | Execute primitives (doc 05), report events | ESP-NOW to master |

### Core Principles

1. **Pods are stateless and reactive.** They execute primitives (light up, detect touch, play sound) and report events (hit, miss). All drill intelligence lives in the master's DrillInterpreter.
2. **Role is assigned at runtime.** The first pod to receive a BLE connection from the phone becomes the master. All pods run identical firmware. There is no persistent master/slave distinction.
3. **Drill execution is autonomous.** Once the master has the drill program and START_DRILL is received, it executes without the phone. BLE can drop without affecting the drill.
4. **Every pod runs its own ModeManager and GameEngine.** The orchestration layer coordinates transitions, but each pod manages its own SystemMode (doc 11) and GameState (doc 05) locally.

### Role Assignment

When a phone connects to a pod via BLE:
- That pod becomes the **master** for this session
- It starts ESP-NOW and broadcasts discovery beacons
- Other pods that respond become **slaves**

The NVS key `is_master` (see doc 09) is a **default preference**, not a hard assignment. Any pod can be master if the phone connects to it. The preference is used for BLE advertising name customization (e.g., "DOMES-Pod-Master") to help the user find the preferred pod in the scanner.

---

## Communication Topology

```
                    Phone App
                   (iOS/Android)
                       |
                  BLE GATT (~30ms RTT)
                       |
+----------------------v----------------------------------------+
|  Master Pod                                                   |
|  - BLE server (phone connection)                              |
|  - ESP-NOW master (pod coordination)                          |
|  - DrillInterpreter (executes drill programs)                 |
|  - Result collector (accumulates round data)                  |
|  - Also participates as a game target (runs own GameEngine)   |
+----------+----------+----------+------------------------------+
           |          |          |   ESP-NOW (<1ms RTT)
           v          v          v
      +--------+ +--------+ +--------+
      | Pod 2  | | Pod 3  | | Pod N  |
      | Slave  | | Slave  | | Slave  |
      +--------+ +--------+ +--------+
      Each slave runs its own ModeManager (doc 11)
      and GameEngine (doc 05) independently
```

- Phone can only talk to ONE pod (BLE limitation). That pod becomes master.
- Master relays all coordination to slaves via ESP-NOW broadcast/unicast.
- ESP-NOW is connectionless, fire-and-forget with <1ms latency (see doc 04, §Latency Measurement).
- The master also runs a GameEngine for when it is selected as a drill target.

### Two Separate Protocols

The system uses two distinct protocols for different purposes:

| Protocol | Transport | Encoding | Purpose | Defined In |
|----------|-----------|----------|---------|-----------|
| **Config protocol** | USB serial, WiFi TCP, BLE GATT | Protobuf (nanopb/prost), framed `[0xAA][0x55][Len][Type][Payload][CRC32]` | Feature toggles, OTA, system mode queries, CLI commands | `config.proto`, doc 11 |
| **Game protocol** | ESP-NOW | Packed C structs, `MessageHeader` + payload | Drill commands (SET_COLOR, ARM_TOUCH), events (TOUCH_EVENT, TIMEOUT_EVENT), discovery | Doc 04, doc 05 |

These protocols do not overlap. Config messages travel over serial/BLE/WiFi to individual pods. Game messages travel over ESP-NOW between pods during drills.

---

## Session Lifecycle

A full training session has four phases. Each phase maps to specific SystemMode transitions (doc 11) and GameState transitions (doc 05).

### Phase 1: Discovery (IDLE -> CONNECTED)

**Trigger:** User opens phone app and connects to a pod via BLE.

```
Phone App                    Pod 1 (becomes Master)         Pods 2,3,4
    |                            |                              |
    | BLE scan                   | BLE advertising              | BLE advertising
    | finds "DOMES-Pod" x 4     | SystemMode: IDLE (doc 11)    | SystemMode: IDLE
    |                            |                              |
    | BLE GATT connect           |                              |
    | ------------------------>  |                              |
    |                            | SystemMode: IDLE -> CONNECTED|
    |                            | Start ESP-NOW via CommService|
    |                            | (doc 04)                     |
    |                            |                              |
    |                            | ESP-NOW broadcast:           |
    |                            | DISCOVERY beacon             |
    |                            | ------------------------------>
    |                            |                              |
    |                            | <------------------------------
    |                            | ESP-NOW responses:           | each pod responds
    |                            | {mac, battery, rssi, fw_ver} | with identity
    |                            |                              |
    |                            | builds pod roster            | remain in IDLE
    |                            |                              | (until master sends
    | <--------------------------  |                              |  JOIN_GAME)
    | BLE notify: pod roster     |                              |
    | {4 pods found}             |                              |
    |                            |                              |
    | Display: "4 pods ready"    |                              |
```

**SystemMode transitions (doc 11):**
- Master: IDLE -> CONNECTED (triggered by BLE connect, see doc 11, §Integration Point: BLE Connection)
- Slaves: remain in IDLE until Phase 2

**What's exchanged:**
- Phone -> Master (BLE write): implicit -- BLE connection makes this pod the master
- Master -> All (ESP-NOW broadcast): discovery beacon with master MAC, channel
- Slaves -> Master (ESP-NOW unicast): identity response (MAC, battery %, RSSI, firmware version)
- Master -> Phone (BLE notify): pod roster for display

### Phase 2: Drill Setup (CONNECTED -> GAME)

**Trigger:** User selects a drill and taps "Start" in the phone app.

```
Phone App                    Master Pod                    Slave Pods
    |                            |                              |
    | User picks drill           |                              |
    | e.g. "Reaction Pro"        |                              |
    |                            |                              |
    | BLE write:                 |                              |
    | DrillProgram               |                              |
    | (config or bytecode)       |                              |
    | ------------------------>  |                              |
    |                            | stores program in RAM        |
    |                            |                              |
    | BLE write:                 |                              |
    | START_DRILL                |                              |
    | {pods: [1,2,3,4]}          |                              |
    | ------------------------>  |                              |
    |                            | SystemMode:                  |
    |                            | CONNECTED -> GAME (doc 11)   |
    |                            | GameEngine -> kReady (doc 05)|
    |                            |                              |
    |                            | ESP-NOW broadcast:           |
    |                            | JOIN_GAME {master_mac}       |
    |                            | ------------------------------>
    |                            |                              | SystemMode:
    |                            |                              | IDLE -> CONNECTED -> GAME
    |                            |                              | (doc 11)
    |                            |                              | GameEngine -> kReady
    |                            |                              | (doc 05)
    |                            |                              |
    |                            | ESP-NOW broadcast:           |
    |                            | SYNC_CLOCK                   |
    |                            | ------------------------------>
    |                            |                              | TimingService adjusts
    |                            |                              | offset (doc 05)
    |                            |                              |
    |                            | DrillInterpreter begins      |
```

**SystemMode transitions (doc 11):**
- Master: CONNECTED -> GAME (on START_DRILL from phone)
- Slaves: IDLE -> CONNECTED -> GAME (on JOIN_GAME from master; see doc 11, §Integration Point: ESP-NOW JOIN_GAME)

**Phone disconnection resilience:** Once the master has the drill program and START_DRILL is received, the DrillInterpreter executes autonomously. BLE can drop without affecting the drill. Results accumulate on the master and are pushed to the phone whenever BLE reconnects.

### Phase 3: Drill Execution (GameState Cycling)

**Trigger:** Master's DrillInterpreter begins stepping through the drill program.

This is the core game loop. All pods are in `SystemMode::GAME` (doc 11), and each pod's GameEngine (doc 05) is in `GameState::kReady`. The DrillInterpreter on the master drives each round by sending primitives (doc 05, §Pod Primitives) to target pods.

**For each round of a reaction drill:**

```
Master (DrillInterpreter)         Target Pod (e.g. Pod 3)
    |                                      |
    | SELECT_PODS: picks Pod 3 randomly    |
    | DELAY: random 500-2000ms wait        |
    | ... waiting ...                      |
    |                                      |
    | Primitive: SET_COLOR (green, 3000ms) |        Pod 3 GameState: kReady
    | ESP-NOW unicast to Pod 3             |
    | ----------------------------------->  |        LEDs go green
    |                                      |
    | Primitive: ARM_TOUCH (3000ms timeout)|
    | ESP-NOW unicast to Pod 3             |
    | ----------------------------------->  |        PodCommandHandler receives
    |                                      |        calls GameEngine.arm()
    |                                      |        GameState: kReady -> kArmed
    |                                      |        (doc 05)
    |                                      |
    |                                      |        ... user runs to pod ...
    |                                      |        ... TOUCHES the pad ...
    |                                      |
    |                                      |        GameState: kArmed -> kTriggered
    |                                      |        (doc 05)
    |                                      |        reaction_time = touch_at - armed_at
    |                                      |        (local clock, doc 05 §Local Reaction Timing)
    |                                      |
    |                                      |        GameState: kTriggered -> kFeedback
    |                                      |        plays hit sound (local)
    |                                      |        fires haptic (local)
    |                                      |        LEDs flash white (local)
    |                                      |
    |                                      |        GameEngine fires event callback
    |                                      |        PodCommandHandler sends:
    | <-----------------------------------  |
    | ESP-NOW unicast:                     |
    | TOUCH_EVENT {pod:3, time:342000us}   |        GameState: kFeedback -> kReady
    |                                      |
    | DrillInterpreter stores result:      |
    | round[n] = {pod:3, 342ms, hit}       |
    |                                      |
    | BLE notify to phone (if connected):  |
    | {pod:3, reaction_ms:342, hit:true}   |
    |                                      |
    | ... advance to next round ...        |
```

**Timing accuracy:** Reaction time is measured **locally on the touched pod** as `touch_at - armed_at`. Both timestamps come from the same local `esp_timer_get_time()` (see doc 05, §Local Reaction Timing). ESP-NOW transmission latency (~500us) does not affect the measurement.

**If the user misses (timeout):**

```
Target Pod (no touch within 3000ms):
    |
    |  GameEngine.tick() detects timeout (doc 05)
    |  GameState: kArmed -> kFeedback (miss)
    |  LEDs go red briefly
    |
    |  GameEngine fires event callback (kMiss)
    |  PodCommandHandler sends:
    |  ESP-NOW unicast to master:
    |  TIMEOUT_EVENT {pod:3}
    |
Master:
    |  DrillInterpreter stores result:
    |  round[n] = {pod:3, 3000ms, miss}
    |  advance to next round
```

**If the master pod is a target:** The master can arm its own GameEngine locally (direct function call, no ESP-NOW needed). The DrillInterpreter calls `gameEngine_.arm()` and receives the event callback directly. Touch detection and timing work identically since they run in the game task (doc 05, §Task Architecture).

### Phase 4: Results (GAME -> CONNECTED)

**Trigger:** DrillInterpreter reaches the end of the drill program.

```
Master Pod                              Slave Pods
    |                                      |
    | ESP-NOW broadcast:                   |
    | STOP_ALL                             |
    | ----------------------------------->  |
    |                                      | GameEngine.disarm() (doc 05)
    |                                      | GameState -> kReady
    |                                      | SystemMode: GAME -> CONNECTED
    |                                      | (doc 11)
    |                                      |
    | SystemMode: GAME -> CONNECTED        |
    | (doc 11)                             |
    |                                      |
    | Compile DrillResult:                 |
    | {                                    |
    |   drill: "Reaction Pro",             |
    |   rounds: [                          |
    |     {pod:3, time_ms:342, hit:true},  |
    |     {pod:1, time_ms:287, hit:true},  |
    |     {pod:4, time_ms:3000, hit:false},|
    |     ...                              |
    |   ],                                 |
    |   summary: {                         |
    |     total: 10,                       |
    |     hits: 8,                         |
    |     misses: 2,                       |
    |     avg_ms: 356,                     |
    |     best_ms: 245,                    |
    |     worst_ms: 510                    |
    |   }                                  |
    | }                                    |
    |                                      |

Master Pod                              Phone App
    |                                      |
    | BLE notify:                          |
    | DrillResult (chunked if needed)      |
    | ----------------------------------->  |
    |                                      | displays results screen
    |                                      | saves to local database
    |                                      | optional: sync to cloud
```

**Drill repeat without phone:** The master retains the drill program in RAM. A long-press on the master re-enters GAME (see doc 11, §Solo Drills, scenario 2) and re-executes the same drill. Results accumulate and are pushed to the phone whenever BLE reconnects.

---

## Reporting Chain

```
Touch pad (capacitive hardware)
    | interrupt, <1ms
    v
GameEngine on target pod (doc 05)
    | reaction_time = touch_at - armed_at (local clock)
    | GameState: Armed -> Triggered -> Feedback
    | fires GameEventCallback
    v
PodCommandHandler (on slave) or DrillInterpreter (on master)
    | packages GameEvent into TOUCH_EVENT message
    v
Target Pod -> Master (ESP-NOW unicast, ~500us)
    | TouchEventMsg {pod_id, reaction_time_us, touch_strength}
    v
DrillInterpreter (on master)
    | stores per-round result, decides next action
    v
Master -> Phone (BLE notify, ~30ms)
    | live round result for real-time display
    v
Phone App (local)
    | display + persist to local database
    v
Cloud (optional, async)
    | history, trends, coach dashboard
```

---

## Data Persistence Model

| Location | What's stored | Lifetime |
|----------|--------------|----------|
| Pod firmware (RAM) | Current drill program, current round state | Until drill ends or power off |
| Master pod (RAM) | Accumulated DrillResult | Until pushed to phone, then discarded |
| Phone app (local DB) | All drill history, player profiles | Persistent across sessions |
| Cloud (optional) | Synced from phone | Permanent, multi-device access |

Pods are deliberately stateless. They hold nothing after a drill ends. The phone is the source of truth for all historical data.

---

## Pod Command Protocol (ESP-NOW)

### Overview

Commands between master and slave pods travel over ESP-NOW using the game protocol (packed C structs with `MessageHeader`; see doc 04, §Protocol Messages). This is separate from the config protocol (protobuf over serial/BLE/WiFi) used for `domes-cli` and OTA.

### Message Categories

| Category | Direction | Messages | Purpose |
|----------|-----------|----------|---------|
| **Discovery** | Master -> All, Slaves -> Master | DISCOVERY, DISCOVERY_RESPONSE | Find nearby pods, build roster |
| **Session** | Master -> Slaves | JOIN_GAME, LEAVE_GAME | Join/leave a game session |
| **Clock** | Master -> All | SYNC_CLOCK | Align pod clocks (doc 05, §Clock Synchronization) |
| **Primitives** | Master -> Target | SET_COLOR, ARM_TOUCH, PLAY_SOUND, FIRE_HAPTIC, STOP_ALL | Control target pod (doc 05, §Pod Primitives) |
| **Events** | Target -> Master | TOUCH_EVENT, TIMEOUT_EVENT | Report game events (doc 05, §Response Events) |
| **Health** | Slaves -> Master | STATUS_REPORT, ERROR_REPORT | Battery, RSSI, errors |

### Protocol Requirements

1. **Fits ESP-NOW constraints** -- Max 250 bytes per message, connectionless, no guaranteed delivery
2. **Common header** -- All messages share a versioned `MessageHeader` (doc 04) for type dispatch and sequencing
3. **Idempotent commands** -- Duplicate messages (from retries) must not cause incorrect behavior
4. **Latency budget** -- Command dispatch on the pod must complete in <1ms (no blocking operations in the receive path)
5. **Forward compatible** -- Unknown message types are silently ignored, allowing firmware version skew between pods
6. **No ACK for primitives** -- SET_COLOR, PLAY_SOUND, FIRE_HAPTIC are fire-and-forget. The master does not wait for ACKs.
7. **Events are best-effort with retry** -- TOUCH_EVENT and TIMEOUT_EVENT are retried 2-3 times since they carry scoring data, but delivery is not guaranteed
8. **Packed structs, not protobuf** -- ESP-NOW messages use packed C structs for zero-copy parsing and minimum overhead (see doc 04, §Protocol Messages for struct definitions)

### PodCommandHandler (Slave Side)

On slave pods, incoming ESP-NOW messages are dispatched by the `PodCommandHandler`:

```cpp
// game/pod_command_handler.hpp (conceptual)
class PodCommandHandler {
public:
    PodCommandHandler(GameEngine& engine, ModeManager& mode,
                      LedService& leds, AudioService& audio,
                      HapticService& haptic, CommService& comm);

    // Called from CommService receive callback (Core 0)
    // Enqueues command for processing on game task (Core 1)
    void onEspNowMessage(const EspNowMessage& msg);

private:
    void handleSetColor(const SetColorMsg& msg);
    void handleArmTouch(const ArmTouchMsg& msg);    // -> GameEngine.arm()
    void handlePlaySound(const PlaySoundMsg& msg);
    void handleHapticPulse(const HapticPulseMsg& msg);
    void handleStopAll();                             // -> GameEngine.disarm()
    void handleJoinGame(const uint8_t* masterMac);   // -> ModeManager transitions
    void handleSyncClock(uint32_t masterTimeUs);      // -> TimingService

    // GameEngine event callback — sends ESP-NOW event to master
    void onGameEvent(const GameEvent& event);
};
```

### DrillInterpreter (Master Side)

On the master pod, the `DrillInterpreter` steps through the drill program and sends commands to target pods:

```cpp
// game/drill_interpreter.hpp (conceptual)
class DrillInterpreter {
public:
    DrillInterpreter(GameEngine& localEngine, CommService& comm,
                     ModeManager& mode);

    // Load a drill program (Phase 1: built-in config, Phase 2: bytecode)
    esp_err_t loadDrill(const ReactionDrillConfig& config, const PodRoster& roster);

    // Execute the drill (called from game task tick)
    void tick();

    // Handle events from local GameEngine or remote pods
    void onLocalEvent(const GameEvent& event);
    void onRemoteEvent(uint8_t podId, const GameEvent& event);

    // Get accumulated results
    const DrillResult& getResult() const;

    bool isRunning() const;

private:
    GameEngine& localEngine_;
    CommService& comm_;
    ModeManager& mode_;

    // Drill state
    DrillResult result_;
    uint8_t currentRound_ = 0;
    uint8_t targetPodId_ = 0;
    // ... timing, pod selection logic ...
};
```

---

## System Mode Coordination

The orchestration layer drives SystemMode transitions (doc 11) across all pods in a session. Each pod's ModeManager processes transitions locally, but the triggers come from this layer.

| Event | Who Triggers | Master SystemMode | Slave SystemMode |
|-------|-------------|-------------------|------------------|
| Phone BLE connects | BLE stack | IDLE -> CONNECTED | (no change) |
| Master discovers slaves | CommService | (no change) | (no change, still IDLE) |
| Phone sends START_DRILL | BLE write | CONNECTED -> GAME | (no change yet) |
| Master sends JOIN_GAME | DrillInterpreter | (no change) | IDLE -> CONNECTED -> GAME |
| Touch detected on pod | GameEngine (doc 05) | GameState: Armed -> Triggered | GameState: Armed -> Triggered |
| Drill completes | DrillInterpreter | GAME -> CONNECTED | (after STOP_ALL) GAME -> CONNECTED |
| Master sends STOP_ALL | DrillInterpreter | (no change) | GAME -> CONNECTED |
| Phone BLE disconnects | BLE stack | CONNECTED -> IDLE (after timeout) | (no change unless master sends LEAVE_GAME) |
| Master sends LEAVE_GAME | CommService | (no change) | CONNECTED -> IDLE |
| Slave enters ERROR | ModeManager (doc 11) | (no change, handles missing pod) | Any -> ERROR |
| Slave error reported | CommService | DrillInterpreter skips pod | (already in ERROR) |

### Error Handling During Drills

If a slave pod enters `SystemMode::ERROR` (doc 11, §ERROR) during a drill:

1. The slave's ModeManager sends a best-effort `kErrorReport` via ESP-NOW to the master
2. The master's DrillInterpreter marks that pod as unavailable
3. If the errored pod was the current target, the DrillInterpreter records a miss and advances to the next round
4. Remaining rounds skip the errored pod
5. The slave auto-recovers to IDLE after 10s (doc 11, §ERROR) and can rejoin in a future drill

If the **master** enters ERROR, the drill is aborted. Slaves detect master loss via ESP-NOW heartbeat timeout and return to IDLE.

---

## Clock Synchronization

Clock sync is needed for **sequence drills** (doc 05, §Drill Types) where multiple pods must activate at precise relative times. For single-pod reaction drills, clock sync is not required because timing is measured locally (doc 05, §Local Reaction Timing).

The master periodically broadcasts `SYNC_CLOCK` messages containing its `esp_timer_get_time()`. Each slave's `TimingService` (doc 05, §Clock Synchronization) estimates its offset using a low-pass filter. Target accuracy: < 1ms between any two pods.

Clock sync is **NOT** used for reaction time measurement. Reaction time = `touch_at - armed_at`, both from the same local clock on the same pod (doc 05).

---

## Downloadable Drills (Phase 2: Bytecode Interpreter)

### Motivation

Phase 1 uses built-in drill configurations compiled into firmware (see doc 05, §Drill Types). This limits drill variety to what was known at firmware build time. Phase 2 introduces downloadable drills that can be created, shared, and executed without firmware updates.

### Phase 1 vs Phase 2

| Aspect | Phase 1 (Built-In) | Phase 2 (Bytecode) |
|--------|--------------------|--------------------|
| Drill definition | C++ structs in firmware (doc 05) | Bytecode programs downloaded from phone |
| Adding new drills | Firmware update required | App/cloud download, no firmware change |
| DrillInterpreter | Reads `ReactionDrillConfig`, `SequenceDrillConfig`, etc. | Executes instruction stream |
| Primitives used | Same (doc 05, §Pod Primitives) | Same (doc 05, §Pod Primitives) |
| GameEngine interaction | Same (`arm()`, event callbacks) | Same (`arm()`, event callbacks) |

Both phases use the **same pod primitives** (doc 05), the **same GameEngine** (doc 05), and the **same SystemMode transitions** (doc 11). The only difference is where the drill program comes from.

### Bytecode Concept

Pods ship with a small **drill interpreter** that understands the fixed set of primitives from doc 05. Drills are encoded as programs (instruction sequences) that compose these primitives with flow control (loops, delays, conditionals on touch/timeout events, scoring instructions).

A drill program is:
- **Authored** on a server or in the app (drill designer tool)
- **Downloaded** to the phone app (from a drill store or shared by a coach)
- **Transferred** to the master pod via BLE before execution (compact, ~200-500 bytes)
- **Executed** by the master pod's DrillInterpreter autonomously
- **Results reported** back to the phone when complete

### Requirements

1. **Compact encoding** -- A drill program must fit in a single BLE transfer or a small number of chunks. Target: < 1KB for typical drills.
2. **Safe execution** -- The interpreter must guarantee termination (bounded loops, mandatory timeouts on waits, drill-level watchdog). No arbitrary memory access. Sandboxed to pod primitives only.
3. **Pod-agnostic** -- Programs reference pods by role (SELECTED, ALL, SEQ[i]) not by MAC address. The master resolves roles to actual pods at runtime.
4. **Offline repeat** -- Once transferred to the master, the drill can be re-executed without the phone (see doc 11, §Solo Drills, scenario 2).
5. **Extensible primitives** -- New pod capabilities (e.g., new sensor types) can be added as new primitive opcodes without breaking existing drill programs (forward compatibility via version field).
6. **Scoring on phone** -- The interpreter records raw events (reaction times, hits, misses). Advanced scoring and analytics are computed on the phone app, not on the pod.

### Detailed design of the drill encoding format, instruction set, and interpreter is deferred to a future document.

---

## Implementation Layers (Build Order)

| Layer | What | Depends On | Can Test With |
|-------|------|-----------|---------------|
| **1. ESP-NOW CommService** | Send/receive, discovery, peer management | WiFi stack (exists), doc 04 | Two pods, CLI logs |
| **2. PodCommandHandler** | Execute SET_COLOR, ARM_TOUCH etc. from ESP-NOW | CommService, existing services (LED, touch, audio, haptic), GameEngine (doc 05) | Two pods, hardcoded commands |
| **3. DrillInterpreter (Phase 1)** | Execute built-in drill configs, manage rounds | CommService, PodCommandHandler, GameEngine (doc 05), ModeManager (doc 11) | Two pods, serial trigger |
| **4. BLE game service** | DrillControl + results GATT characteristics | NimBLE stack (exists), DrillInterpreter | Phone app mockup |
| **5. Result collector** | Accumulate per-round data, compile summary | DrillInterpreter | CLI dump after drill |
| **6. Phone app** | BLE scanner, drill UI, results display | BLE game service | End-to-end |
| **7. DrillInterpreter (Phase 2)** | Bytecode interpreter | Phase 1 interpreter working | Hardcoded bytecode via serial |
| **8. Drill store** | Download/share drill programs | Phone app, cloud backend | App-level testing |

Layers 1-3 can be developed and tested with CLI commands and serial output (no phone app needed). Layer 3 can be tested by sending hardcoded drill programs via the existing serial/BLE config transport.

---

*Prerequisites: 04-communication.md, 05-game-engine.md, 11-system-modes.md*
*Companions: 05-game-engine.md, 11-system-modes.md*
*Related: 09-reference.md (message types, timing constants, NVS schema)*
