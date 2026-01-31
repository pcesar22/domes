# 12 - Multi-Pod Orchestration

## AI Agent Instructions

Load this file when:
- Implementing ESP-NOW CommService
- Building the game engine or drill system
- Designing phone app ↔ pod communication
- Working on pod discovery or pairing
- Implementing drill execution or results reporting

Prerequisites: `04-communication.md`, `05-game-engine.md`, `11-system-modes.md`

---

## Overview

This document describes the end-to-end architecture for orchestrating multiple DOMES pods in a training session. It covers discovery, connection, drill execution, measurement, and results reporting.

**Key actors:**

| Actor | Role | Communication |
|-------|------|---------------|
| Phone app (iOS/Android) | User interface, drill selection, results display | BLE GATT to master pod |
| Master pod | BLE bridge to phone, ESP-NOW coordinator, drill execution | BLE to phone, ESP-NOW to slaves |
| Slave pods (1-N) | Execute primitives, report events | ESP-NOW to master |

**Core principle:** Pods are stateless and reactive. They execute primitives (light up, detect touch, play sound) and report events (touch, timeout). All intelligence lives in the master pod's drill interpreter and the phone app's UI.

---

## Communication Topology

```
                    Phone App
                   (iOS/Android)
                       │
                  BLE GATT (~30ms RTT)
                       │
┌──────────────────────▼───────────────────────────────┐
│  Master Pod                                          │
│  - BLE server (phone connection)                     │
│  - ESP-NOW master (pod coordination)                 │
│  - Drill interpreter (executes drill programs)       │
│  - Result collector (accumulates round data)         │
│  - Also participates as a game target                │
└──────────┬──────────┬──────────┬─────────────────────┘
           │          │          │   ESP-NOW (<1ms RTT)
           ▼          ▼          ▼
      ┌────────┐ ┌────────┐ ┌────────┐
      │ Pod 2  │ │ Pod 3  │ │ Pod N  │
      │ Slave  │ │ Slave  │ │ Slave  │
      └────────┘ └────────┘ └────────┘
```

- Phone can only talk to ONE pod (BLE limitation). That pod becomes master.
- Master relays all coordination to slaves via ESP-NOW broadcast/unicast.
- Any pod can be master. All pods run identical firmware.
- ESP-NOW is connectionless, fire-and-forget with <1ms latency.

---

## End-to-End Session Flow

### Phase 1: Discovery (IDLE → CONNECTED)

**Trigger:** User opens phone app and connects to a pod via BLE.

```
Phone App                    Pod 1 (becomes Master)         Pods 2,3,4
    │                            │                              │
    │ BLE scan                   │ BLE advertising (IDLE)       │ BLE advertising (IDLE)
    │ finds "DOMES-Pod" × 4     │                              │
    │                            │                              │
    │ BLE GATT connect           │                              │
    │ ──────────────────────►    │                              │
    │                            │ IDLE → CONNECTED             │
    │                            │ Start ESP-NOW                │
    │                            │                              │
    │                            │ ESP-NOW broadcast:           │
    │                            │ DISCOVERY beacon             │
    │                            │ ─────────────────────────►   │
    │                            │                              │
    │                            │ ◄─────────────────────────   │
    │                            │ ESP-NOW responses:           │ each pod responds
    │                            │ {mac, battery, rssi}         │ with identity
    │                            │                              │
    │                            │ builds pod roster            │ remain in IDLE
    │                            │                              │ (until master sends
    │ ◄──────────────────────    │                              │  JOIN command)
    │ BLE notify: pod roster     │                              │
    │ {4 pods found}             │                              │
    │                            │                              │
    │ Display: "4 pods ready"    │                              │
```

**System mode transitions:**
- Master: IDLE → CONNECTED (on BLE connect)
- Slaves: remain in IDLE until explicitly joined

**What's exchanged:**
- Phone → Master (BLE write): implicit — BLE connection makes this pod the master
- Master → All (ESP-NOW broadcast): discovery beacon with master MAC, channel
- Slaves → Master (ESP-NOW unicast): identity response (MAC, battery %, RSSI, firmware version)
- Master → Phone (BLE notify): pod roster for display

### Phase 2: Drill Setup

**Trigger:** User selects a drill and taps "Start" in the phone app.

```
Phone App                    Master Pod                    Slave Pods
    │                            │                              │
    │ User picks drill           │                              │
    │ e.g. "Reaction Pro"        │                              │
    │                            │                              │
    │ BLE write:                 │                              │
    │ DrillProgram               │                              │
    │ (bytecode, ~200 bytes)     │                              │
    │ ──────────────────────►    │                              │
    │                            │ stores program in RAM        │
    │                            │                              │
    │ BLE write:                 │                              │
    │ START_DRILL                │                              │
    │ {pods: [1,2,3,4]}          │                              │
    │ ──────────────────────►    │                              │
    │                            │ CONNECTED → GAME             │
    │                            │                              │
    │                            │ ESP-NOW broadcast:           │
    │                            │ JOIN_GAME {master_mac}       │
    │                            │ ─────────────────────────►   │
    │                            │                              │ IDLE → CONNECTED → GAME
    │                            │                              │
    │                            │ ESP-NOW broadcast:           │
    │                            │ SYNC_CLOCK                   │
    │                            │ ─────────────────────────►   │
    │                            │                              │ adjust clock offset
    │                            │                              │
    │                            │ begins drill execution       │
```

**System mode transitions:**
- Master: CONNECTED → GAME (on START_DRILL)
- Slaves: IDLE → CONNECTED → GAME (on JOIN_GAME from master)

**Phone disconnection resilience:** Once the master has the drill program and START_DRILL is received, the master executes autonomously. BLE can drop without affecting the drill.

### Phase 3: Drill Execution

**Trigger:** Master's drill interpreter begins executing the program.

This is the core game loop. The master steps through the drill program instruction by instruction. For a reaction drill with 10 rounds:

```
FOR EACH ROUND:

Master (drill interpreter)         Target Pod (e.g. Pod 3)
    │                                      │
    │ execute: SELECT_PODS RANDOM 1        │
    │ execute: DELAY 500-2000ms            │
    │ ... waiting ...                      │
    │                                      │
    │ ESP-NOW unicast:                     │
    │ SET_COLOR {0,255,0, 3000ms}          │
    │ ─────────────────────────────►       │
    │                                      │ lights up GREEN
    │                                      │
    │ ESP-NOW unicast:                     │
    │ ARM_TOUCH {timeout: 3000ms}          │
    │ ─────────────────────────────►       │
    │                                      │ starts touch detection
    │                                      │ records: armed_at = now()
    │                                      │
    │                                      │ ... user runs to pod ...
    │                                      │ ... TOUCHES the pad ...
    │                                      │
    │                                      │ touch_at = now()
    │                                      │ reaction_time = touch_at - armed_at
    │                                      │ plays hit sound (local)
    │                                      │ fires haptic (local)
    │                                      │ LEDs flash white (local)
    │                                      │
    │ ◄─────────────────────────────       │
    │ ESP-NOW unicast:                     │
    │ TOUCH_EVENT {pod:3, time:342000us}   │
    │                                      │
    │ stores: round[n] = {pod:3, 342ms, hit}
    │                                      │
    │ BLE notify to phone (if connected):  │
    │ {pod:3, reaction_ms:342, hit:true}   │
    │                                      │
    │ ... advance to next round ...        │
```

**Timing accuracy:** Reaction time is measured **locally on the touched pod** as `touch_at - armed_at`. Both timestamps come from the same local clock (`esp_timer_get_time()`). ESP-NOW transmission latency (~500us) does not affect the measurement.

**If the user misses (timeout):**

```
Target Pod (no touch within 3000ms):
    │
    │ timeout fires
    │ LEDs go red briefly
    │
    │ ESP-NOW unicast to master:
    │ TIMEOUT_EVENT {pod:3}
    │
Master:
    │ stores: round[n] = {pod:3, 3000ms, miss}
    │ advance to next round
```

**If the master pod is a target:** The master can arm itself locally (function call, no ESP-NOW needed). Touch detection and timing work identically since they run in their own FreeRTOS task.

### Phase 4: Results

**Trigger:** Drill program reaches END_DRILL instruction.

```
Master Pod                              Slave Pods
    │                                      │
    │ ESP-NOW broadcast:                   │
    │ STOP_ALL                             │
    │ ─────────────────────────────►       │
    │                                      │ GAME → CONNECTED
    │ GAME → CONNECTED                     │ LEDs return to idle
    │                                      │
    │ compile DrillResult:                 │
    │ {                                    │
    │   drill: "Reaction Pro"              │
    │   rounds: [                          │
    │     {pod:3, time_ms:342, hit:true},  │
    │     {pod:1, time_ms:287, hit:true},  │
    │     {pod:4, time_ms:3000, hit:false},│
    │     ...                              │
    │   ],                                 │
    │   summary: {                         │
    │     total: 10,                       │
    │     hits: 8,                         │
    │     misses: 2,                       │
    │     avg_ms: 356,                     │
    │     best_ms: 245,                    │
    │     worst_ms: 510                    │
    │   }                                  │
    │ }                                    │
    │                                      │
    │                                      │

Master Pod                              Phone App
    │                                      │
    │ BLE notify:                          │
    │ DrillResult (chunked if needed)      │
    │ ──────────────────────────────►      │
    │                                      │ displays results screen
    │                                      │ saves to local database
    │                                      │ optional: sync to cloud
```

**Drill repeat without phone:** The master retains the drill program in RAM. The user can re-trigger execution (e.g. tap the master pod or use a physical button) without the phone app. Results accumulate on the master and are pushed to the phone whenever BLE reconnects.

---

## Reporting Chain

```
Touch pad (capacitive hardware)
    │ interrupt, <1ms
    ▼
TouchService (on target pod)
    │ reaction_time = touch_at - armed_at (local clock)
    ▼
Target Pod → Master (ESP-NOW unicast, ~500us)
    │ TouchEvent {pod_id, reaction_time_us, touch_strength}
    ▼
Drill Interpreter (on master)
    │ stores per-round result, decides next action
    ▼
Master → Phone (BLE notify, ~30ms)
    │ live round result for real-time display
    ▼
Phone App (local)
    │ display + persist to local database
    ▼
Cloud (optional, async)
    │ history, trends, coach dashboard
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

Pods understand a small, fixed set of commands. These are the primitives that all drill programs are built from.

### Master → Pod (Commands)

| Message | Purpose | Key Fields |
|---------|---------|------------|
| DISCOVERY | Find nearby pods | master_mac, channel |
| JOIN_GAME | Invite pod to game session | master_mac |
| SYNC_CLOCK | Clock synchronization | master_timestamp_us |
| SET_COLOR | Light up LEDs | r, g, b, duration_ms, transition |
| ARM_TOUCH | Enable touch detection | timeout_ms, feedback_mode |
| PLAY_SOUND | Play audio clip | sound_id, volume |
| HAPTIC | Fire haptic motor | effect_id, intensity |
| STOP_ALL | Reset pod to idle state | (none) |

### Pod → Master (Events)

| Message | Purpose | Key Fields |
|---------|---------|------------|
| IDENTITY | Response to DISCOVERY | mac, battery_pct, rssi, fw_version |
| TOUCH_EVENT | User touched the pod | pod_id, reaction_time_us, touch_strength |
| TIMEOUT_EVENT | Touch window expired | pod_id |
| STATUS_REPORT | Periodic health check | pod_id, battery_pct, rssi, state |
| ERROR_REPORT | Pod-level fault | pod_id, error_code |

### Message Format

All ESP-NOW messages use a common header (defined in `04-communication.md`):

```
┌─────────┬──────┬──────────┬─────────────┬─────────────────┐
│ version │ type │ sequence │ timestamp   │ payload (var)   │
│ 1 byte  │ 1 B  │ 2 bytes  │ 4 bytes     │ up to 241 bytes │
└─────────┴──────┴──────────┴─────────────┴─────────────────┘
```

---

## Downloadable Drills (Future — High-Level Requirement)

### Motivation

Drills should be **data, not code**. New drill types should be deployable from the phone app or a cloud drill store without firmware updates to any pod. Users should be able to download, configure, and execute new drills entirely from the app.

### Concept

Pods ship with a small **drill interpreter** that understands the fixed set of primitives listed above. Drills are encoded as programs (instruction sequences) that compose these primitives with flow control (loops, delays, conditionals on touch/timeout events, scoring instructions).

A drill program is:
- **Authored** on a server or in the app (drill designer tool)
- **Downloaded** to the phone app (from a drill store or shared by a coach)
- **Transferred** to the master pod via BLE before execution (compact, ~200-500 bytes)
- **Executed** by the master pod's drill interpreter autonomously
- **Results reported** back to the phone when complete

### Requirements

1. **Compact encoding** — A drill program must fit in a single BLE transfer or a small number of chunks. Target: <1KB for typical drills.
2. **Safe execution** — The interpreter must guarantee termination (bounded loops, mandatory timeouts on waits, drill-level watchdog). No arbitrary memory access. Sandboxed to pod primitives only.
3. **Pod-agnostic** — Programs reference pods by role (SELECTED, ALL, SEQ[i]) not by MAC address. The master resolves roles to actual pods at runtime.
4. **Offline repeat** — Once transferred to the master, the drill can be re-executed without the phone.
5. **Extensible primitives** — New pod capabilities (e.g. new sensor types) can be added as new primitive opcodes without breaking existing drill programs (forward compatibility via version field).
6. **Scoring flexibility** — The interpreter records raw events (reaction times, hits, misses). Advanced scoring and analytics are computed on the phone app, not on the pod.

### Detailed design of the drill encoding format, instruction set, and interpreter is deferred to a future document.

---

## System Mode Integration

The mode manager (doc 11) gates which features are active. The orchestration layer drives the transitions:

| Event | Mode Transition | Triggered By |
|-------|----------------|--------------|
| Phone BLE connects | IDLE → CONNECTED | BLE stack (master only) |
| Master sends JOIN_GAME | IDLE → CONNECTED → GAME | ESP-NOW (slaves) |
| START_DRILL received | CONNECTED → GAME | Phone app via BLE |
| Drill completes / STOP_ALL | GAME → CONNECTED | Drill interpreter / phone app |
| Phone BLE disconnects | CONNECTED → IDLE | BLE stack (after timeout) |
| ESP-NOW peer lost | GAME → CONNECTED or IDLE | CommService (slaves) |
| Critical fault | Any → ERROR | Watchdog, driver failure |

---

## Clock Synchronization

Clock sync is needed for **sequence drills** where multiple pods must activate at precise relative times. For single-pod reaction drills, clock sync is not required because timing is measured locally.

The master periodically broadcasts SYNC_CLOCK messages. Each slave estimates its offset from the master using a low-pass filter (see `05-game-engine.md` TimingService). Target accuracy: <1ms between any two pods.

Clock sync is NOT used for reaction time measurement. Reaction time = `touch_at - armed_at`, both from the same local clock on the same pod.

---

## Implementation Layers (Build Order)

| Layer | What | Depends On |
|-------|------|-----------|
| 1. ESP-NOW CommService | Send/receive, discovery, peer management | WiFi stack (exists) |
| 2. Pod command handlers | Execute SET_COLOR, ARM_TOUCH etc. from ESP-NOW | CommService, existing services (LED, touch, audio, haptic) |
| 3. BLE game service | DrillControl + results characteristics | NimBLE stack (exists) |
| 4. Drill interpreter | Execute drill programs, manage rounds | CommService, pod command handlers |
| 5. Result collector | Accumulate per-round data, compile summary | Drill interpreter |
| 6. Phone app | BLE scanner, drill UI, results display | BLE game service |
| 7. Drill store | Download/share drill programs | Phone app, cloud backend |

Layers 1-2 can be developed and tested with CLI commands (no phone app needed). Layer 4 can be tested by sending hardcoded drill programs via the existing serial/BLE config transport.

---

*Prerequisites: 04-communication.md, 05-game-engine.md, 11-system-modes.md*
*Related: 09-reference.md (message types, timing constants)*
