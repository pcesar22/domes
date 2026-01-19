# Firmware Development Guidelines

## Project Context

DOMES (Distributed Open-source Motion & Exercise System) is a reaction training pod system. Each pod has LEDs, audio, haptics, touch sensing, and communicates via ESP-NOW + BLE.

**Reference Documents:**
- `research/SOFTWARE_ARCHITECTURE.md` - Full firmware design spec
- `research/SYSTEM_ARCHITECTURE.md` - Hardware architecture, pin mappings
- `research/DEVELOPMENT_ROADMAP.md` - Milestone definitions, task dependencies

---

## Technical Stack

| Aspect | Choice |
|--------|--------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB PSRAM) |
| Framework | ESP-IDF v5.x (NOT Arduino) |
| RTOS | FreeRTOS (bundled with ESP-IDF) |
| Language | C++20 for application, C for low-level drivers |
| Build | CMake via `idf.py` |

---

## Code Style

### C++20 Features to Use
- `std::optional`, `std::variant`, `std::string_view`, `std::span`
- `constexpr` for compile-time constants
- `enum class` for type safety
- Structured bindings: `auto [success, value] = func();`
- RAII wrappers for hardware resources

### What to Avoid
- `<iostream>` - adds 200KB, use `ESP_LOG*` macros instead
- Exceptions - disabled by default
- RTTI - disabled by default
- Heavy STL in ISRs - use fixed-size buffers
- Global singletons - use dependency injection

### Logging
```cpp
#include "esp_log.h"
static const char* TAG = "module_name";

ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
ESP_LOGW(TAG, "Warning message");
ESP_LOGI(TAG, "Info message");
ESP_LOGD(TAG, "Debug message");
```

---

## Architecture Rules

### Driver Abstraction
All drivers MUST have an abstract interface for testability:

```cpp
// interfaces/i_haptic_driver.hpp
class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t playEffect(uint8_t effect_id) = 0;
};
```

Real implementations go in `drivers/`, mocks go in `test/mocks/`.

### Dependency Injection
Services receive driver interfaces via constructor, not globals:

```cpp
class FeedbackService {
public:
    FeedbackService(IHapticDriver& haptic, IAudioDriver& audio);
};
```

### Task Pinning
- Core 0: WiFi, BLE, ESP-NOW (protocol stack tasks)
- Core 1: Audio, game logic (user-responsive tasks)
- Either core: LED updates, touch polling

---

## Directory Structure

```
firmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── config.hpp              # Pin definitions, constants
│   ├── interfaces/             # Abstract base classes
│   ├── drivers/                # Hardware drivers
│   ├── services/               # Business logic services
│   ├── game/                   # Game engine, drills
│   └── utils/                  # Helpers, error codes
├── components/                 # Reusable ESP-IDF components
└── test/
    ├── mocks/                  # Mock implementations
    └── test_*.cpp              # Unit tests
```

---

## Hardware Interfaces

| Peripheral | Interface | Driver IC | Notes |
|------------|-----------|-----------|-------|
| LEDs | RMT | SK6812 RGBW | 16 LEDs in ring |
| Audio | I2S | MAX98357A | 23mm speaker |
| Haptic | I2C | DRV2605L | LRA motor |
| Touch | ESP32 touch peripheral | - | Capacitive sense |
| IMU | I2C | LIS2DW12 | Tap detection |
| Power | ADC | - | Battery voltage |

Pin assignments are in `research/SYSTEM_ARCHITECTURE.md`.

---

## Testing Strategy

### Unit Tests (Host)
- Run on Linux target: `idf.py --preview set-target linux`
- Use CMock for driver mocks
- Test business logic without hardware

### Smoke Tests (Device) - PLANNED, NOT YET IMPLEMENTED
- Will be built into firmware, triggered by button hold at boot
- Will validate each peripheral individually (LED, audio, haptic, IMU, touch)
- Used for hardware bring-up and CI
- **Status:** Planned for E7 milestone (see `MILESTONES.md`)
- **Current alternative:** Use `test_config.py` for protocol validation

---

## Performance Tracing

The firmware includes a lightweight tracing framework for post-mortem performance analysis.

### Adding Trace Points

```cpp
#include "trace/traceApi.hpp"

void myFunction() {
    // RAII scope trace - begin on entry, end on exit
    TRACE_SCOPE(TRACE_ID("Module.Function"), domes::trace::TraceCategory::kGame);

    // Manual begin/end
    TRACE_BEGIN(TRACE_ID("Module.SubOp"), domes::trace::TraceCategory::kGame);
    doWork();
    TRACE_END(TRACE_ID("Module.SubOp"), domes::trace::TraceCategory::kGame);

    // Point event
    TRACE_INSTANT(TRACE_ID("Module.Event"), domes::trace::TraceCategory::kGame);

    // Counter
    TRACE_COUNTER(TRACE_ID("Module.Value"), value, domes::trace::TraceCategory::kGame);
}
```

### Available Categories

`kKernel`, `kTransport`, `kOta`, `kWifi`, `kLed`, `kAudio`, `kTouch`, `kGame`, `kUser`, `kHaptic`, `kBle`, `kNvs`

### Dumping Traces

```bash
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json
# Open trace.json in https://ui.perfetto.dev
```

### Key Files

- `main/trace/traceApi.hpp` - User macros (`TRACE_SCOPE`, `TRACE_BEGIN`, etc.)
- `main/trace/traceRecorder.hpp` - Singleton recorder
- `main/trace/traceEvent.hpp` - 16-byte event struct
- `tools/trace/trace_dump.py` - Host dump tool

See `research/architecture/07-debugging.md` for full documentation.

---

## Error Handling

Use ESP-IDF error codes. Check and propagate errors:

```cpp
esp_err_t init() {
    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}
```

Do NOT silently ignore errors. Every `esp_err_t` must be checked.

---

## Validation Before Commit

Before considering any task complete:

1. **Compiles** - `idf.py build` succeeds with no warnings
2. **No hardcoded values** - Use `config.hpp` for pins, constants
3. **Interface exists** - Every driver has an abstract interface
4. **Mock exists** - Every interface has a mock for testing
5. **Logs present** - Key operations have `ESP_LOGI`/`ESP_LOGD`
6. **Errors handled** - No unchecked `esp_err_t` returns

---

## Common Pitfalls

| Pitfall | Solution |
|---------|----------|
| DMA buffers in PSRAM | Use `MALLOC_CAP_DMA` for I2S/SPI buffers |
| WiFi + BLE conflicts | Enable coexistence: `ESP_COEX_PREFER_BALANCE` |
| Stack overflow | Monitor with `uxTaskGetStackHighWaterMark()` |
| Watchdog timeout | Call `esp_task_wdt_reset()` in long loops |
| Flash write in ISR | Queue data, write in task context |

---

## Key Constants

```cpp
// From SYSTEM_ARCHITECTURE.md
constexpr uint8_t LED_COUNT = 16;
constexpr uint32_t ESPNOW_LATENCY_TARGET_US = 2000;  // P95 < 2ms
constexpr size_t AUDIO_SAMPLE_RATE = 16000;
constexpr size_t OTA_PARTITION_SIZE = 0x400000;  // 4MB
```

---

## Code Organization Principles

**Keep it simple. No over-engineering.**

- **No backward compatibility wrappers** - When refactoring, just change the code. Update all call sites. Don't create wrapper files or alias namespaces.
- **No umbrella headers** - Don't create `all.hpp` or `index.hpp` files. Include what you need directly.
- **Delete unused code** - Don't comment it out, don't deprecate it, just delete it.
- **One file, one purpose** - Don't split simple configs into multiple files for "organization".
- **Flat over nested** - Avoid unnecessary directory hierarchies.

---

## Runtime Configuration

The firmware supports runtime feature toggles via a binary protocol.

### Transport Architecture

```
┌─────────────────┐                    ┌─────────────────┐
│   Host Tool     │   USB-CDC/TCP      │   ESP32-S3      │
│   (CLI/Python)  │◄──────────────────►│   Firmware      │
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
| ConfigCommandHandler | `config/configCommandHandler.hpp` | Handles config messages |
| FeatureManager | `config/featureManager.hpp` | Feature state (atomic bitmask) |
| FrameDecoder | `protocol/frameCodec.hpp` | Frame parsing |

### Adding New Features

1. Add feature ID to `config/configProtocol.hpp`:
```cpp
enum class Feature : uint8_t {
    kLedEffects = 1,
    kBle = 2,
    // Add new feature here
    kMyFeature = 8,
};
```

2. Handle the feature in your service by checking FeatureManager:
```cpp
if (featureManager.isEnabled(Feature::kMyFeature)) {
    // Feature is enabled
}
```

### Initialization Order (main.cpp)

1. `initFeatureManager()` - Create feature manager (required first)
2. `initTcpConfigServer()` - Start TCP server (if WiFi connected)
3. `initSerialOta()` - Start serial OTA receiver (takes over USB-CDC console)

**Important**: FeatureManager must be initialized before both TCP and serial config handlers.

---

## When in Doubt

1. Check `research/SOFTWARE_ARCHITECTURE.md` for design decisions
2. Check `research/SYSTEM_ARCHITECTURE.md` for hardware specs
3. Check `research/DEVELOPMENT_ROADMAP.md` for task dependencies
4. Prefer explicit over clever
5. Prefer tested over fast
6. **Delete code rather than deprecate it**
