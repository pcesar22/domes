# Firmware Development Guidelines

## Project Context

DOMES is a reaction training pod system. Firmware runs on ESP32-S3 with FreeRTOS under ESP-IDF
v5.x. Application code is C++20, low-level code may be C. This is not an Arduino project.

| Aspect | Choice |
| --- | --- |
| MCU | ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB PSRAM |
| Framework | ESP-IDF v5.x |
| RTOS | FreeRTOS bundled with ESP-IDF |
| Language | C++20 for application, C for low-level drivers |
| Build | CMake through `idf.py` |
| Containers | ETL fixed-capacity containers, not STL dynamic containers |
| Error handling | `tl::expected<T, esp_err_t>` or `esp_err_t`, no exceptions |

## Coding Standards

| Element | Convention | Example |
| --- | --- | --- |
| Files | camelCase | `ledDriver.hpp`, `feedbackService.cpp` |
| Classes | PascalCase | `LedDriver`, `FeedbackService` |
| Interfaces | `I` + PascalCase | `IHapticDriver`, `IAudioDriver` |
| Methods/functions | camelCase | `init()`, `playEffect()` |
| Variables | camelCase | `reactionTime`, `ledCount` |
| Member variables | camelCase + trailing underscore | `ledDriver_`, `intensity_` |
| Constants | `k` + PascalCase | `kTag`, `kMaxRetries` |
| Namespaces | lowercase | `pins`, `config`, `utils` |
| Macros | SCREAMING_SNAKE_CASE | `CONFIG_DOMES_LED_COUNT` |

Formatting:

- 100 character max line length.
- 4 spaces, no tabs.
- K&R braces.
- Pointer/reference style: `Type* ptr`, `Type& ref`.
- Use `#pragma once`.
- Include order: corresponding header, ESP-IDF, stdlib, ETL, project.

Use C++20 features that do not violate embedded constraints: `std::optional`, `std::variant`,
`std::string_view`, `std::span`, `constexpr`, `enum class`, structured bindings, and RAII wrappers.

Forbidden:

| Feature | Reason | Alternative |
| --- | --- | --- |
| `<iostream>` | Adds large binary size | `ESP_LOGx` |
| Exceptions | Disabled | `tl::expected` or `esp_err_t` |
| RTTI | Disabled | Avoid `dynamic_cast` designs |
| `std::vector`, `std::string`, `std::map` | Dynamic allocation | ETL equivalents |
| `new`, `delete`, `malloc`, `free` after init | Heap fragmentation | Static or startup allocation |

Prefer ETL fixed-capacity containers:

```cpp
#include <etl/vector.h>

etl::vector<int, 32> values;
etl::string<64> name;
```

## Error Handling

Use `tl::expected` for fallible operations and `ESP_RETURN_ON_ERROR` for simple propagation.

```cpp
tl::expected<SensorReading, esp_err_t> readSensor() {
    uint8_t data[4];
    esp_err_t err = i2cRead(kSensorAddr, data, sizeof(data));
    if (err != ESP_OK) {
        return tl::unexpected(err);
    }
    return SensorReading{data};
}
```

Do not ignore `esp_err_t` results from ESP-IDF calls.

## Logging

```cpp
static constexpr const char* kTag = "module_name";

ESP_LOGE(kTag, "Error: %s", esp_err_to_name(err));
ESP_LOGW(kTag, "Warning message");
ESP_LOGI(kTag, "Info message");
ESP_LOGD(kTag, "Debug: reg 0x%02X = 0x%02X", reg, val);
```

Never log from ISR context.

## ISR Safety

ISRs must be minimal and defer work to tasks:

```cpp
void IRAM_ATTR touchIsr(void* arg) {
    auto* self = static_cast<TouchDriver*>(arg);
    BaseType_t woken = pdFALSE;
    uint32_t timestamp = esp_timer_get_time();
    xQueueSendFromISR(self->queue_, &timestamp, &woken);
    portYIELD_FROM_ISR(woken);
}
```

ISR requirements:

- `IRAM_ATTR` on ISR functions.
- `DRAM_ATTR` on data accessed when cache may be disabled.
- No logging, heap allocation, floating point, or blocking calls.
- Only use FreeRTOS APIs with `FromISR` suffix.
- Complete in less than about 10 us.

## Memory And Ownership

- Heap allocation is allowed during initialization only.
- After `app_main()` setup completes, avoid heap allocation.
- Use `std::unique_ptr` for factory-created init-time objects only.
- Use references for dependency injection after init.
- Use raw pointers only for C API interop or nullable references.
- Prefer static allocation for FreeRTOS tasks, queues, semaphores, and DMA buffers.
- Use fixed-width integer types for registers, protocol fields, buffers, and timestamps.
- Mark non-mutating methods and parameters `const`.
- Use Doxygen comments on public APIs.

## Architecture Rules

All hardware drivers must have abstract interfaces for testability. Real implementations live in
`firmware/domes/main/drivers/`; mocks live in `firmware/domes/test/mocks/`.

Services receive driver interfaces through constructors, not globals.

```cpp
class FeedbackService {
public:
    FeedbackService(IHapticDriver& haptic, IAudioDriver& audio);
};
```

Task pinning:

| Core | Work |
| --- | --- |
| Core 0 | WiFi, BLE, ESP-NOW protocol stack tasks |
| Core 1 | Audio and game logic |
| Either | LED updates and touch polling |

Platform selection is through Kconfig. Keep pin mappings in platform-specific headers and expose
them through a unified platform header selected by `CONFIG_DOMES_PLATFORM_*`.

## Organization Principles

- Keep the design simple.
- Do not add backward compatibility wrappers during refactors; update call sites.
- Do not add umbrella headers.
- Delete unused code instead of commenting it out.
- Use one file for one purpose, usually one class per `.hpp`/`.cpp` pair.
- Avoid unnecessary directory nesting.

## Protocol And Runtime Config

Runtime configuration uses framed protobuf messages over USB-CDC, TCP, and BLE.

```text
[AA 55 Len Type Payload CRC]
```

Key components:

| Component | File | Purpose |
| --- | --- | --- |
| UsbCdcTransport | `main/transport/usbCdcTransport.hpp` | USB serial transport |
| TcpTransport | `main/transport/tcpTransport.hpp` | TCP client transport |
| TcpConfigServer | `main/transport/tcpConfigServer.hpp` | TCP server on port 5000 |
| BleOtaService | `main/transport/bleOtaService.hpp` | BLE GATT service |
| ConfigCommandHandler | `main/config/configCommandHandler.hpp` | Config protocol handler |
| FeatureManager | `main/config/featureManager.hpp` | Atomic feature bitmask |
| FrameCodec | `firmware/common/protocol/frameCodec.hpp` | Shared frame parsing |

All message definitions come from `firmware/common/proto/*.proto`.

## Multi-Device Architecture

Each pod is standalone with per-pod singletons: FeatureManager, ModeManager, GameEngine, and
TraceRecorder. Multi-device coordination happens through the host CLI during development and
through ESP-NOW for production games.

Do not change these assumptions without a design update:

- FeatureManager, ModeManager, and GameEngine are per-pod singletons by design.
- The transport trait is single-connection; multi-device fan-out belongs in CLI dispatch.

Pod identity:

- `pod_id` is stored in NVS as `config_key::kPodId`, range 1-255.
- BLE names use `DOMES-Pod-01`, or a MAC fallback such as `DOMES-Pod-3A2B`.
- Protocol responses include pod ID where relevant.
- Set pod ID with `domes-cli system set-pod-id 1`.

## Tracing

```cpp
#include "trace/traceApi.hpp"

void processGameTick() {
    TRACE_SCOPE(TRACE_ID("Game.Tick"), domes::trace::TraceCategory::kGame);
    TRACE_INSTANT(TRACE_ID("Game.Hit"), domes::trace::TraceCategory::kGame);
    TRACE_COUNTER(TRACE_ID("Game.Score"), score, domes::trace::TraceCategory::kGame);
}
```

Categories include `kKernel`, `kTransport`, `kOta`, `kWifi`, `kLed`, `kAudio`, `kTouch`, `kGame`,
`kUser`, `kHaptic`, `kBle`, and `kNvs`.

Dump traces with:

```bash
domes-cli --port /dev/ttyACM0 trace dump -o trace.json --names tools/trace/trace_names.json
```

Open the output in `https://ui.perfetto.dev`.

## Initialization Order

In `main.cpp`, preserve this order:

1. WiFi before TCP config server and BLE.
2. BLE OTA service early.
3. FeatureManager before TCP/Serial/BLE config handlers.
4. TcpConfigServer before Serial OTA.
5. Serial OTA last.

After `initSerialOta()`, USB-CDC becomes OTA/config transport and logging may stop or interleave
with binary protocol data.

## Validation Checklist

- `idf.py build` succeeds with no new warnings.
- No `new` or `malloc` after initialization.
- No forbidden STL dynamic containers.
- No `<iostream>`, exceptions, or RTTI.
- Every driver has an abstract interface and mock.
- Public APIs have Doxygen comments.
- Non-mutating methods are `const`.
- ISR code is IRAM-safe and uses only `FromISR` APIs.
- Error returns use `tl::expected` or `esp_err_t`.
- Shared state is protected with the project RAII mutex wrapper.
- No hardcoded values that belong in `config.hpp`, Kconfig, or named constants.

## Common Pitfalls

| Pitfall | Fix |
| --- | --- |
| DMA buffers in PSRAM | Allocate with DMA-capable memory |
| WiFi + BLE conflicts | Enable coexistence with balanced preference |
| Stack overflow | Monitor `uxTaskGetStackHighWaterMark()` |
| Watchdog timeout | Reset watchdog in long loops or split work |
| Flash writes in ISR | Queue work and write in task context |
| USB-CDC console lost | Debug before Serial OTA takes over |

## Hardware Interfaces

| Peripheral | Interface | Driver IC | Notes |
| --- | --- | --- | --- |
| LEDs | RMT | SK6812 RGBW | 16 LEDs in ring |
| Audio | I2S | MAX98357A | 23 mm speaker |
| Haptic | I2C | DRV2605L | LRA motor |
| Touch | ESP32 touch peripheral | none | Capacitive sense |
| IMU | I2C | LIS2DW12 | Tap detection |
| Power | ADC | none | Battery voltage |

Reference documents:

- `firmware/MILESTONES.md`
- `research/architecture/`
- `research/SYSTEM_ARCHITECTURE.md`
- `docs/PIN_REFERENCE.md`
- `research/architecture/07-debugging.md`
