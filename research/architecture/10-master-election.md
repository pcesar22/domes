# 10 - Master Election Protocol

## AI Agent Instructions

Load this file when implementing master/follower roles or network topology.

Prerequisites: `04-communication.md`

---

## Overview

The DOMES network uses a single **master pod** that:
- Bridges BLE (phone) to ESP-NOW (pod network)
- Coordinates game timing across all pods
- Broadcasts clock synchronization messages

Election is **automatic** with a **manual override hook** for testing.

---

## Election Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           ELECTION FLOW                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐                  │
│  │  DISCOVERY  │─────►│ CAMPAIGNING │─────►│   VOTING    │                  │
│  │   (500ms)   │      │   (200ms)   │      │   (300ms)   │                  │
│  └─────────────┘      └─────────────┘      └──────┬──────┘                  │
│        │                                          │                          │
│        │ no peers                                 │                          │
│        ▼                                          ▼                          │
│  ┌─────────────┐                          ┌─────────────┐                   │
│  │   MASTER    │◄─────────────────────────│  COMPARE    │                   │
│  │ (standalone)│     highest priority     │ PRIORITIES  │                   │
│  └─────────────┘                          └──────┬──────┘                   │
│                                                  │ lower priority            │
│                                                  ▼                           │
│                                           ┌─────────────┐                   │
│                                           │  FOLLOWER   │                   │
│                                           └─────────────┘                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## State Machine

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        ELECTION STATE MACHINE                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│        Boot                                                                  │
│          │                                                                   │
│          ▼                                                                   │
│    ┌──────────┐     no master heartbeat      ┌─────────────┐                │
│    │   Idle   │─────────────────────────────►│ Discovering │                │
│    └──────────┘                              └──────┬──────┘                │
│          ▲                                          │                        │
│          │                                          ▼                        │
│          │                                   ┌─────────────┐                │
│          │                                   │ Campaigning │                │
│          │                                   └──────┬──────┘                │
│          │                                          │                        │
│          │              ┌───────────────────────────┼──────────────────┐    │
│          │              │                           │                  │    │
│          │              ▼                           ▼                  │    │
│          │       ┌──────────┐               ┌──────────┐              │    │
│          │       │  Master  │               │ Follower │──────────────┘    │
│          │       └────┬─────┘               └────┬─────┘  master dies      │
│          │            │                          │                          │
│          │            │ resign                   │                          │
│          └────────────┴──────────────────────────┘                          │
│                                                                              │
│    ┌──────────────┐                                                         │
│    │ ForcedMaster │  (NVS/BLE/Serial override for testing)                  │
│    └──────────────┘                                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Priority Score

Pods elect master based on weighted priority:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PRIORITY CALCULATION                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    Score = (battery << 24) | (uptime << 16) | (signal << 8) | random        │
│                                                                              │
│    ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐           │
│    │  Battery   │  │   Uptime   │  │   Signal   │  │   Random   │           │
│    │  (0-100)   │  │  (0-255)   │  │  (RSSI)    │  │ tiebreaker │           │
│    │  HIGHEST   │  │   HIGH     │  │   MEDIUM   │  │    LOW     │           │
│    │  WEIGHT    │  │  WEIGHT    │  │   WEIGHT   │  │   WEIGHT   │           │
│    └────────────┘  └────────────┘  └────────────┘  └────────────┘           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Message Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           MULTI-POD ELECTION                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│     Pod A (priority: 90)              Pod B (priority: 85)                  │
│     ─────────────────                 ─────────────────                     │
│              │                                 │                             │
│              │──── DISCOVER ──────────────────►│                             │
│              │◄─── DISCOVER_ACK ───────────────│                             │
│              │                                 │                             │
│              │──── CANDIDATE (priority=90) ───►│                             │
│              │◄─── VOTE (accept=true) ─────────│                             │
│              │                                 │                             │
│              │──── MASTER_ANNOUNCE ───────────►│   (every 1s)               │
│              │                                 │                             │
│         [MASTER]                          [FOLLOWER]                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Failover

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              FAILOVER SCENARIOS                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Master Dies Suddenly:                                                       │
│  ────────────────────                                                        │
│    Follower monitors heartbeat (every 1s)                                   │
│    3 missed heartbeats (3s) → declare master dead                           │
│    Start new election                                                        │
│                                                                              │
│  Master Resigns Gracefully:                                                  │
│  ─────────────────────────                                                   │
│    Master broadcasts RESIGN with suggested successor                         │
│    Successor fast-tracks to master                                          │
│    Others wait for new master announcement                                  │
│                                                                              │
│  Split Brain (two masters):                                                  │
│  ─────────────────────────                                                   │
│    Both receive each other's MASTER_ANNOUNCE                                │
│    Lower priority master yields immediately                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Test Hooks

| Method | Purpose |
|--------|---------|
| NVS: `force_master = 1` | Force master on boot |
| Serial: `force_master` | Force master at runtime |
| Serial: `resign_master` | Trigger graceful handoff |
| BLE: Write to 0x1905 | Remote master control |

---

## Timing Constants

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Discovery timeout | 500ms | Wait for peer responses |
| Campaign timeout | 200ms | Broadcast candidacy |
| Vote timeout | 300ms | Collect votes |
| Heartbeat interval | 1000ms | Master announcement |
| Master timeout | 3000ms | Declare master dead |

---

## Interface

```cpp
// EXAMPLE: Minimal election interface

class IElectionService {
public:
    virtual ~IElectionService() = default;
    virtual bool isMaster() const = 0;
    virtual void forceBecomeMaster() = 0;
    virtual void resignAsMaster() = 0;
    virtual void onMasterElected(std::function<void(bool)> cb) = 0;
};
```

---

*Related: `04-communication.md`, `11-clock-sync.md`*
