# Firmware Development Guidelines

## Project Context

DOMES (Distributed Open-source Motion & Exercise System) is a reaction training pod system. Each pod has LEDs, audio, haptics, touch sensing, IMU, and communicates via ESP-NOW + BLE. Firmware runs on ESP32-S3 with FreeRTOS under ESP-IDF v5.x (NOT Arduino). C++20 for application code, C for low-level drivers.

## Technical Stack

| Aspect | Choice |
|--------|--------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB PSRAM) |
| Framework | ESP-IDF v5.x (NOT Arduino) |
| RTOS | FreeRTOS (bundled with ESP-IDF) |
| Language | C++20 for application, C for low-level drivers |
| Build | CMake via `idf.py` |
| Containers | ETL (Embedded Template Library) — no STL containers |
| Error handling | `tl::expected<T, esp_err_t>` or `esp_err_t` — no exceptions |

## Coding Standards

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Files | camelCase | `ledDriver.hpp`, `feedbackService.cpp` |
| Classes | PascalCase | `LedDriver`, `FeedbackService` |
| Interfaces | I + PascalCase | `IHapticDriver`, `IAudioDriver` |
| Methods/Functions | camelCase | `init()`, `playEffect()` |
| Variables | camelCase | `reactionTime`, `ledCount` |
| Member variables | camelCase + trailing `_` | `ledDriver_`, `intensity_` |
| Constants | k + PascalCase | `kTag`, `kMaxRetries` |
| Namespaces | lowercase | `pins`, `config`, `utils` |
| Macros | SCREAMING_SNAKE_CASE | `CONFIG_DOMES_LED_COUNT` |

### Formatting

| Rule | Value |
|------|-------|
| Line length | 100 characters max |
| Indentation | 4 spaces (no tabs) |
| Braces | K&R style (opening brace same line) |
| Pointer/reference | `Type* ptr`, `Type& ref` (attached to type) |
| Include guard | `#pragma once` |
| Include order | 1. Corresponding header, 2. ESP-IDF, 3. stdlib, 4. ETL, 5. Project |

### C++20 Features to Use
- `std::optional`, `std::variant`, `std::string_view`, `std::span`, `constexpr`, `enum class`
- Structured bindings: `auto [success, value] = func();`
- RAII wrappers for hardware resources

### Forbidden Features

| Feature | Reason | Alternative |
|---------|--------|-------------|
| `<iostream>` | Adds ~200KB | `ESP_LOGx` macros |
| C++ exceptions | Disabled | `tl::expected` |
| RTTI | Disabled | Design without `dynamic_cast` |
| `std::vector`, `std::string`, `std::map` | Dynamic alloc | ETL equivalents |
| `new`/`delete`/`malloc`/`free` after init | Heap fragmentation | Pre-allocate at startup |

### ETL Containers (Required)

```cpp
// FORBIDDEN: STL containers
std::vector<int> values;
std::string name;

// REQUIRED: ETL fixed-capacity containers
etl::vector<int, 32> values;
etl::string<64> name;
etl::map<int, Data, 16> lookup;
```

### Error Handling

Use `tl::expected` for fallible operations, `ESP_RETURN_ON_ERROR` for simple propagation:

```cpp
tl::expected<SensorReading, esp_err_t> readSensor() {
    uint8_t data[4];
    esp_err_t err = i2cRead(kSensorAddr, data, sizeof(data));
    if (err != ESP_OK) {
        return tl::unexpected(err);
    }
    return SensorReading{data};
}

esp_err_t init() {
    ESP_RETURN_ON_ERROR(driverA.init(), kTag, "Driver A init failed");
    return ESP_OK;
}
```

### Logging

```cpp
static constexpr const char* kTag = "module_name";

ESP_LOGE(kTag, "Error: %s", esp_err_to_name(err));
ESP_LOGW(kTag, "Warning message");
ESP_LOGI(kTag, "Info message");
ESP_LOGD(kTag, "Debug: reg 0x%02X = 0x%02X", reg, val);
```

### ISR Safety

ISRs must do minimal work and defer to tasks:

```cpp
void IRAM_ATTR touchIsr(void* arg) {
    auto* self = static_cast<TouchDriver*>(arg);
    BaseType_t woken = pdFALSE;
    uint32_t timestamp = esp_timer_get_time();
    xQueueSendFromISR(self->queue_, &timestamp, &woken);
    portYIELD_FROM_ISR(woken);
}
```

ISR requirements: `IRAM_ATTR` on function, `DRAM_ATTR` on data, no logging, no heap, no floats, complete in <10us, only `*FromISR` FreeRTOS APIs.

### Thread Safety (RAII Mutex)

```cpp
class Mutex {
public:
    Mutex() { handle_ = xSemaphoreCreateMutexStatic(&buffer_); }
    void lock() { xSemaphoreTake(handle_, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(handle_); }
private:
    StaticSemaphore_t buffer_;
    SemaphoreHandle_t handle_;
};

class MutexGuard {
public:
    explicit MutexGuard(Mutex& m) : mutex_(m) { mutex_.lock(); }
    ~MutexGuard() { mutex_.unlock(); }
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
private:
    Mutex& mutex_;
};
```

### Memory & Ownership

- Heap allocation allowed **during init only** — no allocation after `app_main()` setup completes
- `std::unique_ptr` for factory-created objects at init; references for DI after init
- Raw pointers only for C API interop or nullable references
- Prefer static allocation for FreeRTOS tasks, queues, semaphores
- Use fixed-width integer types for hardware registers, protocol fields, buffers, timestamps
- Strict const-correctness: `const` on non-mutating methods and parameters, `constexpr` for constants
- Doxygen-style comments on all public APIs

---

## Architecture Rules

### Driver Abstraction

All drivers MUST have an abstract interface for testability. Real implementations in `drivers/`, mocks in `test/mocks/`.

### Dependency Injection

Services receive driver interfaces via constructor, not globals:

```cpp
class FeedbackService {
public:
    FeedbackService(IHapticDriver& haptic, IAudioDriver& audio);
};
```

### Task Pinning

- **Core 0**: WiFi, BLE, ESP-NOW (protocol stack tasks)
- **Core 1**: Audio, game logic (user-responsive tasks)
- **Either core**: LED updates, touch polling

### Platform Abstraction

Platforms selected via Kconfig. Pin mappings in `platform/pinsDevkit.hpp` (or `pinsPcbV1.hpp`), unified via `platform/pins.hpp` with `#if defined(CONFIG_DOMES_PLATFORM_*)`. Hardware features gated by Kconfig flags.

---

## Code Organization Principles

**Keep it simple. No over-engineering.**

- **No backward compatibility wrappers** — when refactoring, just change the code and update all call sites
- **No umbrella headers** — include what you need directly
- **Delete unused code** — don't comment out or deprecate, just delete
- **One file, one purpose** — one class per `.hpp`/`.cpp` pair
- **Flat over nested** — avoid unnecessary directory hierarchies

---

## Runtime Configuration

### Transport Architecture

```
┌─────────────────┐                    ┌─────────────────┐
│   Host Tool     │   USB-CDC/TCP/BLE  │   ESP32-S3      │
│   (domes-cli)   │◄──────────────────►│   Firmware      │
└─────────────────┘                    └─────────────────┘
        │                                      │
   Frame Protocol                    ConfigCommandHandler
   [AA 55 Len Type Payload CRC]              │
                                     FeatureManager
                                     (atomic bitmask)
```

### Key Components

| Component | File | Purpose |
|-----------|------|---------|
| ITransport | `transport/iTransport.hpp` | Abstract transport interface |
| UsbCdcTransport | `transport/usbCdcTransport.hpp` | USB serial transport |
| TcpTransport | `transport/tcpTransport.hpp` | TCP socket transport |
| TcpConfigServer | `transport/tcpConfigServer.hpp` | TCP server (port 5000) |
| BleOtaService | `transport/bleOtaService.hpp` | BLE GATT service (NimBLE) |
| ConfigCommandHandler | `config/configCommandHandler.hpp` | Handles config messages |
| FeatureManager | `config/featureManager.hpp` | Feature state (atomic bitmask) |
| FrameDecoder | `protocol/frameCodec.hpp` | Frame parsing |

---

## Multi-Device Architecture

Each pod is standalone with per-pod singletons (FeatureManager, ModeManager, GameEngine, TraceRecorder). Multi-device coordination happens via host CLI (dev/test) or ESP-NOW (production games).

**Per-pod identity:**
- `pod_id` stored in NVS (`config_key::kPodId`, uint8_t 1-255), read at boot in `main.cpp::readPodId()`
- Used for BLE name: `DOMES-Pod-01` (or MAC fallback: `DOMES-Pod-3A2B`)
- Included in protocol responses (`ListFeaturesResponse.pod_id`, `GetSystemInfoResponse.pod_id`)
- Set via CLI: `domes-cli system set-pod-id 1` (persisted to NVS, reboot for BLE name)

**mDNS (WiFi):** Service `_domes._tcp.local.` on port 5000, hostname `domes-pod-{pod_id}.local`

**GameEvent tagging:** `GameEvent.podId` populated from NVS at `GameEngine` construction for per-pod attribution.

**What NOT to change:**
- FeatureManager, ModeManager, GameEngine are per-pod singletons — correct by design
- Transport trait is single-connection — multi-device handled at CLI dispatch layer

---

## Performance Tracing

```cpp
#include "trace/traceApi.hpp"

void processGameTick() {
    TRACE_SCOPE(TRACE_ID("Game.Tick"), domes::trace::TraceCategory::kGame);
    TRACE_INSTANT(TRACE_ID("Game.Hit"), domes::trace::TraceCategory::kGame);
    TRACE_COUNTER(TRACE_ID("Game.Score"), score, domes::trace::TraceCategory::kGame);
}
```

Categories: `kKernel`, `kTransport`, `kOta`, `kWifi`, `kLed`, `kAudio`, `kTouch`, `kGame`, `kUser`, `kHaptic`, `kBle`, `kNvs`

Dump: `python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json` then open at `https://ui.perfetto.dev`

Key files: `main/trace/traceApi.hpp` (macros), `main/trace/traceRecorder.hpp` (recorder), `tools/trace/trace_dump.py` (host tool). See `research/architecture/07-debugging.md` for full docs.

---

## Initialization Order (main.cpp)

1. **WiFi** before TCP config server and BLE (for coexistence)
2. **BLE OTA service** early (advertising starts automatically)
3. **FeatureManager** before TCP/Serial/BLE config handlers
4. **TcpConfigServer** before Serial OTA (to see logs)
5. **Serial OTA** last (takes over USB-CDC console — logs stop after this point)

**Important**: After `initSerialOta()`, USB-CDC becomes OTA/config transport. Debug init issues by adding delays before serial OTA init or temporarily disabling it.

---

## Validation Checklist

- [ ] `idf.py build` succeeds with no warnings
- [ ] No `new`/`malloc` after initialization phase
- [ ] No `std::vector`, `std::string`, `std::map` (use ETL)
- [ ] No `<iostream>` includes
- [ ] Every driver has an abstract interface and mock
- [ ] All public APIs have Doxygen comments
- [ ] All non-mutating methods are `const`
- [ ] All ISR code has `IRAM_ATTR`, data has `DRAM_ATTR`
- [ ] Error returns use `tl::expected` or `esp_err_t` — no unchecked returns
- [ ] Mutex access uses `MutexGuard` RAII wrapper
- [ ] Key operations have `ESP_LOGI`/`ESP_LOGD` logging
- [ ] No hardcoded values — use `config.hpp` / constants for pins and magic numbers

---

## Common Pitfalls

| Pitfall | Solution |
|---------|----------|
| DMA buffers in PSRAM | Use `MALLOC_CAP_DMA` for I2S/SPI buffers |
| WiFi + BLE conflicts | Enable coexistence: `ESP_COEX_PREFER_BALANCE` |
| Stack overflow | Monitor with `uxTaskGetStackHighWaterMark()` |
| Watchdog timeout | Call `esp_task_wdt_reset()` in long loops |
| Flash write in ISR | Queue data, write in task context |
| USB-CDC console lost | Serial OTA takes over — debug init issues before that point |

---

## Hardware Interfaces

| Peripheral | Interface | Driver IC | Notes |
|------------|-----------|-----------|-------|
| LEDs | RMT | SK6812 RGBW | 16 LEDs in ring |
| Audio | I2S | MAX98357A | 23mm speaker |
| Haptic | I2C | DRV2605L | LRA motor |
| Touch | ESP32 touch peripheral | — | Capacitive sense |
| IMU | I2C | LIS2DW12 | Tap detection |
| Power | ADC | — | Battery voltage |

---

## Key Constants

```cpp
constexpr uint8_t LED_COUNT = 16;
constexpr uint32_t ESPNOW_LATENCY_TARGET_US = 2000;  // P95 < 2ms
constexpr size_t AUDIO_SAMPLE_RATE = 16000;
constexpr size_t OTA_PARTITION_SIZE = 0x400000;  // 4MB
```

---

## Reference Documents

| Document | Purpose |
|----------|---------|
| `firmware/MILESTONES.md` | Project status and milestone tracking |
| `research/architecture/` | Deep design docs (per-subsystem) |
| `research/SYSTEM_ARCHITECTURE.md` | Hardware architecture and specs |
| `docs/PIN_REFERENCE.md` | Pin mappings for all platforms |
| `research/architecture/07-debugging.md` | Tracing and debugging guide |
