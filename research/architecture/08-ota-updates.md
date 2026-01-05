# 08 - OTA Updates

## AI Agent Instructions

Load this file when implementing OTA update functionality.

Prerequisites: `02-build-system.md`, `04-communication.md`

---

## OTA Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          FLASH MEMORY LAYOUT (16MB)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   0x000000  ┌─────────────────────┐                                         │
│             │    Bootloader       │  32KB                                   │
│   0x008000  ├─────────────────────┤                                         │
│             │  Partition Table    │  4KB                                    │
│   0x009000  ├─────────────────────┤                                         │
│             │       NVS           │  24KB                                   │
│   0x00F000  ├─────────────────────┤                                         │
│             │    OTA Data         │  8KB (boot selection)                   │
│   0x020000  ├─────────────────────┤                                         │
│             │                     │                                         │
│             │      OTA_0          │  4MB (Firmware A)                       │
│             │   (Active/Inactive) │                                         │
│   0x420000  ├─────────────────────┤                                         │
│             │                     │                                         │
│             │      OTA_1          │  4MB (Firmware B)                       │
│             │   (Active/Inactive) │                                         │
│   0x820000  ├─────────────────────┤                                         │
│             │                     │                                         │
│             │      SPIFFS         │  6MB (Audio, config)                    │
│             │                     │                                         │
│   0xE20000  ├─────────────────────┤                                         │
│             │    Core Dump        │  64KB                                   │
│   0xE30000  └─────────────────────┘                                         │
│                                                                              │
│   Boot Flow: Bootloader → OTA Data → Active Partition → App                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## OTA Update Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            OTA UPDATE SEQUENCE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Phone App              Master Pod              Target Pod                  │
│       │                      │                       │                       │
│       │── OTA_BEGIN ────────►│                       │                       │
│       │   (size, version,    │                       │                       │
│       │    SHA-256 hash)     │                       │                       │
│       │                      │── Relay OTA_BEGIN ───►│                       │
│       │                      │                       │                       │
│       │◄─────────────────────┼───────────────────────│ ACK                   │
│       │                      │                       │                       │
│       │── OTA_DATA ─────────►│                       │                       │
│       │   (offset, chunk)    │── Relay chunk ───────►│                       │
│       │                      │                       │ Write to              │
│       │                      │                       │ inactive              │
│       │◄─────────────────────┼───────────────────────│ partition             │
│       │         ACK          │                       │                       │
│       │                      │                       │                       │
│       │   ... repeat for all chunks (4KB each) ...   │                       │
│       │                      │                       │                       │
│       │── OTA_END ──────────►│                       │                       │
│       │   (final hash)       │── Relay OTA_END ─────►│                       │
│       │                      │                       │ Verify SHA-256        │
│       │                      │                       │ Set boot partition    │
│       │◄─────────────────────┼───────────────────────│ SUCCESS               │
│       │                      │                       │                       │
│       │                      │                       │ Reboot ───┐           │
│       │                      │                       │           │           │
│       │                      │                       │◄──────────┘           │
│       │                      │                       │                       │
│       │                      │                       │ Self-test on boot     │
│       │                      │                       │ Confirm or rollback   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Rollback Protection

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ROLLBACK FLOW                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Boot from new partition                                                    │
│           │                                                                  │
│           ▼                                                                  │
│   ┌───────────────┐                                                         │
│   │  Run Self-Test │                                                        │
│   │  • Drivers init │                                                       │
│   │  • Heap check   │                                                       │
│   │  • Basic I/O    │                                                       │
│   └───────┬───────┘                                                         │
│           │                                                                  │
│     ┌─────┴─────┐                                                           │
│     │           │                                                           │
│   Pass        Fail                                                          │
│     │           │                                                           │
│     ▼           ▼                                                           │
│ ┌─────────┐ ┌─────────┐                                                     │
│ │ Confirm │ │Rollback │                                                     │
│ │Firmware │ │to prev  │                                                     │
│ └─────────┘ └────┬────┘                                                     │
│                  │                                                          │
│                  ▼                                                          │
│            Reboot to                                                        │
│            old partition                                                    │
│                                                                              │
│   Timeout: If not confirmed within 60s, auto-rollback on next boot         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Self-Test Criteria

| Test | Pass Criteria | Action on Fail |
|------|---------------|----------------|
| Driver init | All drivers ESP_OK | Rollback |
| Heap check | Free heap > 50KB | Rollback |
| LED test | Visual confirmation | Continue |
| Comms check | ESP-NOW init OK | Rollback |
| Boot time | < 5 seconds | Log warning |

---

## OTA States

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            OTA STATE MACHINE                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌────────┐                                                                │
│   │  Idle  │◄─────────────────────────────────────┐                         │
│   └───┬────┘                                      │                         │
│       │ OTA_BEGIN                                 │                         │
│       ▼                                           │                         │
│   ┌────────────┐                                  │                         │
│   │ Receiving  │──── Timeout/Error ───────────────┤                         │
│   │  chunks    │                                  │                         │
│   └─────┬──────┘                                  │                         │
│         │ All chunks received                     │                         │
│         ▼                                         │                         │
│   ┌────────────┐                                  │                         │
│   │ Verifying  │──── Hash mismatch ───────────────┤                         │
│   │  SHA-256   │                                  │                         │
│   └─────┬──────┘                                  │                         │
│         │ Hash OK                                 │                         │
│         ▼                                         │ Abort                   │
│   ┌────────────┐                                  │                         │
│   │ Rebooting  │                                  │                         │
│   └─────┬──────┘                                  │                         │
│         │                                         │                         │
│         ▼                                         │                         │
│   ┌────────────┐      ┌────────────┐              │                         │
│   │ Self-Test  │─Fail─►│  Rollback  │─────────────┘                         │
│   └─────┬──────┘      └────────────┘                                        │
│         │ Pass                                                              │
│         ▼                                                                   │
│   ┌────────────┐                                                            │
│   │ Confirmed  │                                                            │
│   └────────────┘                                                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Partition Table

| Name | Type | Offset | Size | Purpose |
|------|------|--------|------|---------|
| nvs | data | 0x9000 | 24KB | Config storage |
| otadata | data | 0xF000 | 8KB | Boot selection |
| ota_0 | app | 0x20000 | 4MB | Firmware A |
| ota_1 | app | 0x420000 | 4MB | Firmware B |
| spiffs | data | 0x820000 | 6MB | Audio files |
| coredump | data | 0xE20000 | 64KB | Crash data |

**Constraints:**
- Binary must be < 4MB to fit OTA partition
- OTA_0 and OTA_1 must be same size

---

## Interface

```cpp
// EXAMPLE: Minimal OTA interface

class IOtaService {
public:
    virtual ~IOtaService() = default;
    virtual esp_err_t begin(size_t imageSize, const uint8_t* sha256) = 0;
    virtual esp_err_t writeChunk(const uint8_t* data, size_t len) = 0;
    virtual esp_err_t finish() = 0;
    virtual void abort() = 0;
    virtual OtaState state() const = 0;
    virtual size_t progress() const = 0;

    static esp_err_t confirmFirmware();
    static esp_err_t rollback();
    static const char* getVersion();
};
```

---

## Version Management

| Method | Source | Example |
|--------|--------|---------|
| Manual | CMakeLists.txt | `set(PROJECT_VER "1.0.0")` |
| Git tag | Build script | `git describe --tags` |
| CI build | Environment | `$CI_COMMIT_TAG` |

Version stored in app descriptor, readable via `esp_app_get_description()`.

---

*Prerequisites: 02-build-system.md, 04-communication.md*
*Related: 09-reference.md (partition layout)*
