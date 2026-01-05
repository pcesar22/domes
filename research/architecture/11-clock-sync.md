# 11 - Clock Synchronization

## AI Agent Instructions

Load this file when implementing timing synchronization between pods.

Prerequisites: `04-communication.md`, `10-master-election.md`

---

## Overview

DOMES requires **±1ms timing accuracy** across all pods for:
- Synchronized LED animations
- Accurate reaction time measurement
- Coordinated feedback

Uses an **NTP-style algorithm** with RTT measurement (not hardcoded delays).

---

## Sync Algorithm

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         NTP-STYLE SYNCHRONIZATION                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│       Master                                Follower                         │
│          │                                      │                            │
│          │                                      │                            │
│    T1 ───┼──────── SYNC_REQUEST ───────────────►│                            │
│          │                                      │─── T2 (receive)            │
│          │                                      │                            │
│          │                                      │    [processing]            │
│          │                                      │                            │
│          │                                      │─── T3 (send)               │
│          │◄─────── SYNC_RESPONSE ───────────────┼                            │
│    T4 ───┼                                      │                            │
│          │                                      │                            │
│                                                                              │
│    Calculations:                                                             │
│    ─────────────                                                             │
│    RTT    = (T4 - T1) - (T3 - T2)     // Network delay only                 │
│    Offset = ((T2 - T1) + (T3 - T4)) / 2   // Clock difference               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Sync Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SYNCHRONIZATION FLOW                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐        │
│  │  TIME BEACON    │────►│   ROUGH SYNC    │────►│  FULL SYNC      │        │
│  │  (broadcast)    │     │   (±2ms)        │     │  (±200µs)       │        │
│  │  late joiners   │     │   fast start    │     │  converged      │        │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘        │
│                                                          │                   │
│                                                          ▼                   │
│                                                  ┌─────────────────┐        │
│                                                  │  DRIFT CHECK    │        │
│                                                  │  (every beacon) │        │
│                                                  └─────────────────┘        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Offset Filtering

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           OFFSET FILTERING                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    Raw offset samples (noisy due to WiFi/BLE)                               │
│           │                                                                  │
│           ▼                                                                  │
│    ┌─────────────────┐                                                      │
│    │  Median Filter  │   Reject outliers (8 sample window)                  │
│    └────────┬────────┘                                                      │
│             │                                                                │
│             ▼                                                                │
│    ┌─────────────────┐                                                      │
│    │  EMA Smoothing  │   α=0.5 (converging) → α=0.1 (steady)               │
│    └────────┬────────┘                                                      │
│             │                                                                │
│             ▼                                                                │
│    Stable offset estimate (±200µs)                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Time Conversion

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          TIME DOMAINS                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    LOCAL TIME                              NETWORK TIME                      │
│    (each pod's clock)                      (master's clock)                  │
│                                                                              │
│         │                                       │                            │
│         │    localToNetwork(t)                 │                            │
│         │────────────────────────────────────►│                            │
│         │                                       │                            │
│         │                                       │                            │
│         │◄────────────────────────────────────│                            │
│         │    networkToLocal(t)                 │                            │
│         │                                       │                            │
│                                                                              │
│    Conversion: network_time = local_time - offset                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Reaction Time Measurement

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      REACTION TIME CALCULATION                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    Master                              Pod                                   │
│       │                                  │                                   │
│       │──── ARM_CMD (T_arm) ────────────►│                                   │
│       │     (network time)               │                                   │
│       │                                  │                                   │
│       │                                  │ ← LED flash                       │
│       │                                  │                                   │
│       │                                  │ ← User touches                    │
│       │                                  │   (T_touch, local time)           │
│       │                                  │                                   │
│       │◄─── TOUCH_EVENT ─────────────────│                                   │
│       │                                  │                                   │
│                                                                              │
│    Reaction = localToNetwork(T_touch) - T_arm                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Error Sources

| Source | Magnitude | Mitigation |
|--------|-----------|------------|
| ESP-NOW jitter | 100-500 µs | Median filter |
| Processing variance | 10-50 µs | Precise T2/T3 capture |
| Crystal drift | 20 ppm | Periodic resync |
| WiFi/BLE coex | 0-2000 µs | Reject outliers |
| Task scheduling | 0-1000 µs | High priority callback |

---

## Accuracy Targets

| Phase | Accuracy | Samples |
|-------|----------|---------|
| Rough sync (beacon) | ±2ms | 1 |
| Converging | ±500µs | 5 |
| Steady state | ±200µs | 10+ |

---

## Timing Constants

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Sync interval | 500ms | Request/response rate |
| Beacon interval | 1000ms | Broadcast for late joiners |
| Stale threshold | 5000ms | Force resync if no update |
| Max RTT | 50ms | Reject suspicious samples |
| Drift threshold | 2ms | Trigger correction |

---

## Interface

```cpp
// EXAMPLE: Minimal timing interface

class ITimingService {
public:
    virtual ~ITimingService() = default;
    virtual uint64_t getSyncedTimeUs() const = 0;
    virtual uint64_t localToNetwork(uint64_t localUs) const = 0;
    virtual uint64_t networkToLocal(uint64_t networkUs) const = 0;
    virtual bool isSynced() const = 0;
    virtual void resetSync() = 0;
};
```

---

*Related: `04-communication.md`, `10-master-election.md`, `05-game-engine.md`*
