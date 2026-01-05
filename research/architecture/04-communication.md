# 04 - Communication

## AI Agent Instructions

Load this file when implementing ESP-NOW, BLE, or protocol messages.

Prerequisites: `03-driver-development.md`

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        COMMUNICATION ARCHITECTURE                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                         ┌─────────────┐                                     │
│                         │  Phone App  │                                     │
│                         │ (iOS/Android)│                                     │
│                         └──────┬──────┘                                     │
│                                │ BLE (GATT)                                  │
│                                ▼                                             │
│                         ┌─────────────┐                                     │
│                         │ Master Pod  │                                     │
│                         │  BLE + ESP  │                                     │
│                         └──────┬──────┘                                     │
│                                │ ESP-NOW (<1ms)                              │
│              ┌─────────────────┼─────────────────┐                          │
│              │                 │                 │                          │
│              ▼                 ▼                 ▼                          │
│        ┌─────────┐       ┌─────────┐       ┌─────────┐                     │
│        │  Pod 2  │       │  Pod 3  │       │  Pod N  │                     │
│        └─────────┘       └─────────┘       └─────────┘                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Points:**
- Phone connects to ONE pod via BLE (the "master")
- Master relays commands to other pods via ESP-NOW
- ESP-NOW provides <1ms latency between pods
- Any pod can be master (elected at runtime)

---

## Protocol Stack

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            PROTOCOL LAYERS                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Application Layer                                                   │   │
│   │  • Game commands (ARM, TRIGGER, FEEDBACK)                           │   │
│   │  • Status reports                                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Protocol Layer                                                      │   │
│   │  • Message header (type, seq, timestamp)                            │   │
│   │  • Serialization (packed structs)                                   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│   ┌────────────────────┐    ┌────────────────────┐                         │
│   │     ESP-NOW        │    │       BLE          │                         │
│   │  • Pod-to-pod      │    │  • Phone-to-master │                         │
│   │  • Broadcast       │    │  • GATT services   │                         │
│   │  • <1ms latency    │    │  • Notifications   │                         │
│   └────────────────────┘    └────────────────────┘                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Message Types

| Range | Direction | Examples |
|-------|-----------|----------|
| 0x01-0x0F | Master → Pod | SyncClock, SetColor, ArmTouch, PlaySound |
| 0x10-0x1F | Pod → Master | TouchEvent, TimeoutEvent, StatusReport |
| 0x20-0x2F | System | Ping/Pong, OTA, Reboot |
| 0x30-0x3F | Election | Discover, Candidate, Vote, MasterAnnounce |
| 0x40-0x4F | Sync | SyncRequest, SyncResponse, TimeBeacon |

---

## Message Flow: Drill Round

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          DRILL ROUND MESSAGE FLOW                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Phone              Master                Pod 3                             │
│     │                  │                     │                               │
│     │── START_DRILL ──►│                     │                               │
│     │   (BLE write)    │                     │                               │
│     │                  │                     │                               │
│     │                  │── ARM_TOUCH ───────►│                               │
│     │                  │   (ESP-NOW)         │                               │
│     │                  │                     │                               │
│     │                  │                     │ ← LED flash                   │
│     │                  │                     │                               │
│     │                  │                     │ ← User touches                │
│     │                  │                     │                               │
│     │                  │◄── TOUCH_EVENT ─────│                               │
│     │                  │    {time: 245ms}    │                               │
│     │                  │                     │                               │
│     │◄─ NOTIFY ────────│                     │                               │
│     │  (BLE notify)    │                     │                               │
│     │                  │                     │                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## BLE Service

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            GATT SERVICE                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Service: DOMES (0x1234)                                                   │
│   ─────────────────────                                                     │
│                                                                              │
│   ┌────────────────┬──────────────┬────────────────────────┐               │
│   │ Characteristic │ UUID         │ Properties             │               │
│   ├────────────────┼──────────────┼────────────────────────┤               │
│   │ Pod Status     │ 0x1235       │ Read, Notify           │               │
│   │ Drill Control  │ 0x1236       │ Write                  │               │
│   │ Touch Event    │ 0x1237       │ Notify                 │               │
│   │ Config         │ 0x1238       │ Read, Write            │               │
│   └────────────────┴──────────────┴────────────────────────┘               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## RF Coexistence

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          RF COEXISTENCE                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│     WiFi (ESP-NOW)              BLE                                         │
│     ──────────────              ───                                          │
│     Channel 1 (fixed)           2.4GHz hopping                              │
│     250 bytes/msg               20 bytes/char                               │
│     <1ms latency                15-30ms conn interval                       │
│                                                                              │
│     Coex Mode: ESP_COEX_PREFER_BALANCE                                      │
│     • Both stacks share antenna time                                         │
│     • BLE scan window < interval                                            │
│     • ESP-NOW bursts fit between BLE                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Latency Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| ESP-NOW P50 | < 1ms | Median of 1000 pings |
| ESP-NOW P95 | < 2ms | 95th percentile |
| ESP-NOW P99 | < 5ms | 99th percentile |
| Packet loss | < 1% | Count failures |

---

## Interfaces

```cpp
// EXAMPLE: Minimal communication interfaces

class ICommService {
public:
    virtual ~ICommService() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t send(const uint8_t* mac, const uint8_t* data, size_t len) = 0;
    virtual esp_err_t broadcast(const uint8_t* data, size_t len) = 0;
    virtual void onReceive(std::function<void(const Message&)> cb) = 0;
};

class IBleService {
public:
    virtual ~IBleService() = default;
    virtual esp_err_t init(const char* name) = 0;
    virtual esp_err_t startAdvertising() = 0;
    virtual esp_err_t notify(uint16_t charUuid, const uint8_t* data, size_t len) = 0;
    virtual bool isConnected() const = 0;
};
```

---

*Related: `03-driver-development.md`, `10-master-election.md`, `11-clock-sync.md`*
