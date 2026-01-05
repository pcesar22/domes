# 12 - System Policies

## AI Agent Instructions

Load this file when implementing shared resources, memory allocation, or error handling.

Prerequisites: `03-driver-development.md`

---

## I2C Bus Sharing

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           I2C BUS ARCHITECTURE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│      ┌──────────────┐                  ┌──────────────┐                     │
│      │   LIS2DW12   │                  │   DRV2605L   │                     │
│      │    (IMU)     │                  │   (Haptic)   │                     │
│      │   Addr:0x18  │                  │   Addr:0x5A  │                     │
│      └──────┬───────┘                  └──────┬───────┘                     │
│             │                                 │                              │
│             └─────────────┬───────────────────┘                              │
│                           │                                                  │
│                    ┌──────┴──────┐                                          │
│                    │ I2cManager  │                                          │
│                    │   (mutex)   │                                          │
│                    └──────┬──────┘                                          │
│                           │                                                  │
│                    ┌──────┴──────┐                                          │
│                    │   I2C Bus   │                                          │
│                    │   400kHz    │                                          │
│                    └─────────────┘                                          │
│                                                                              │
│   All I2C access goes through I2cManager singleton with mutex               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Memory Allocation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           MEMORY REGIONS                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   SRAM (512KB - Fast)                 PSRAM (8MB - Slow)                    │
│   ─────────────────                   ──────────────────                    │
│   • Task stacks                       • Audio file cache                    │
│   • FreeRTOS queues                   • OTA staging buffer                  │
│   • LED color buffer                  • Large static data                   │
│   • Protocol buffers                  • Debug logs                          │
│   • DMA buffers                                                             │
│   • NVS cache                                                               │
│                                                                              │
│   Rule: Allocations ≤4KB → SRAM, >4KB → PSRAM                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Component | Memory | Size | Rationale |
|-----------|--------|------|-----------|
| Task stacks | SRAM | 4-8KB each | FreeRTOS requirement |
| LED buffer | SRAM | 64B | Small, frequent |
| Audio DMA | SRAM (DMA) | 4KB | I2S DMA requirement |
| Audio cache | PSRAM | 64KB | Large, sequential |
| OTA staging | PSRAM | 512KB | Large, temporary |

---

## Pod Identity

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         POD IDENTITY STORAGE                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   NVS Namespace: "identity"                                                  │
│   ─────────────────────────                                                  │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────┐           │
│   │  pod_id       │ u8    │ Assigned ID (1-24)                  │           │
│   │  name         │ str   │ User-assigned name ("Pod-1")        │           │
│   │  serial       │ u32   │ Factory serial number               │           │
│   │  hw_rev       │ u8    │ PCB revision                        │           │
│   └─────────────────────────────────────────────────────────────┘           │
│                                                                              │
│   MAC address is read from hardware (esp_read_mac)                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Network Joining

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         NETWORK JOIN PROTOCOL                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   New Pod                                    Network                         │
│      │                                          │                            │
│      │─── 1. DISCOVER ─────────────────────────►│                            │
│      │                                          │                            │
│      │◄── 2. DISCOVER_ACK (master info) ────────│                            │
│      │                                          │                            │
│      │─── 3. JOIN_REQUEST (identity) ──────────►│ Master                     │
│      │                                          │                            │
│      │◄── 4. JOIN_ACCEPT (assigned ID) ─────────│ Master                     │
│      │                                          │                            │
│      │◄── 5. Clock sync begins ─────────────────│                            │
│      │                                          │                            │
│   [CONNECTED]                                   │                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**ID Assignment Rules:**
- User-assigned ID in NVS takes priority
- Master assigns first available ID (1-24)
- Conflict: lower MAC address wins

---

## Error Handling

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ERROR RECOVERY FLOW                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Error Occurs                                                               │
│        │                                                                     │
│        ▼                                                                     │
│   ┌─────────────┐                                                           │
│   │  Classify   │                                                           │
│   │   Error     │                                                           │
│   └──────┬──────┘                                                           │
│          │                                                                   │
│    ┌─────┴─────┬─────────────┬─────────────┐                                │
│    │           │             │             │                                 │
│    ▼           ▼             ▼             ▼                                 │
│ Hardware    Comms       Resource     Protocol                                │
│    │           │             │             │                                 │
│    ▼           ▼             ▼             ▼                                 │
│ ┌─────┐   ┌─────┐       ┌─────┐       ┌─────┐                               │
│ │Retry│   │Retry│       │Reboot│      │Ignore│                              │
│ │ 3x  │   │ 3x  │       │      │      │      │                              │
│ └──┬──┘   └──┬──┘       └──────┘      └──────┘                              │
│    │         │                                                               │
│    ▼         ▼                                                               │
│ ┌─────┐   ┌─────┐                                                           │
│ │Reset│   │Fall-│                                                           │
│ │ I2C │   │back │                                                           │
│ └─────┘   └─────┘                                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Category | Retry | Then |
|----------|-------|------|
| Hardware (I2C stuck) | 3x | Reset bus |
| Communication (timeout) | 3x | Fallback to standalone |
| Resource (out of memory) | 0 | Reboot |
| Protocol (bad message) | 0 | Ignore |

---

## Error Codes

| Range | Category | Examples |
|-------|----------|----------|
| 0x01xx | Hardware | I2C timeout, ADC fail, IMU self-test |
| 0x02xx | Communication | ESP-NOW fail, BLE disconnect, master lost |
| 0x03xx | Resource | Out of memory, queue full |
| 0x04xx | Protocol | Invalid message, version mismatch |

---

## Interfaces

```cpp
// EXAMPLE: Minimal policy interfaces

class II2cManager {
public:
    virtual ~II2cManager() = default;
    virtual esp_err_t init(gpio_num_t sda, gpio_num_t scl) = 0;
    virtual esp_err_t writeRead(uint8_t addr, const uint8_t* wr, size_t wrLen,
                                 uint8_t* rd, size_t rdLen) = 0;
    virtual esp_err_t recover() = 0;
};

class IPodIdentity {
public:
    virtual ~IPodIdentity() = default;
    virtual uint8_t podId() const = 0;
    virtual const char* name() const = 0;
    virtual esp_err_t setPodId(uint8_t id) = 0;
};

class IErrorRecovery {
public:
    virtual ~IErrorRecovery() = default;
    virtual RecoveryAction getAction(ErrorCode code, uint8_t retries) = 0;
    virtual esp_err_t execute(RecoveryAction action) = 0;
};
```

---

*Related: `03-driver-development.md`, `10-master-election.md`*
