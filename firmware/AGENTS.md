# Firmware Development Guidelines

## Project Context

DOMES is a reaction training pod system. Firmware runs on ESP32-S3 with FreeRTOS under ESP-IDF
v5.4.4. Application code is C++20, low-level code may be C. This is not an Arduino project.

| Aspect | Choice |
| --- | --- |
| Current development target | NFF carrier with the checked-in 8 MB flash partition layout |
| Production target | ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB PSRAM; not the active partition profile |
| Framework | ESP-IDF v5.4.4, matching CI and `dependencies.lock` |
| RTOS | FreeRTOS bundled with ESP-IDF |
| Language | C++20 for application, C for low-level drivers |
| Build | CMake through `idf.py` |
| Containers | Fixed-capacity storage where determinism requires it; bounded startup allocation elsewhere |
| Error handling | `esp_err_t` or project enums/results; no exceptions |

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
- Include order: corresponding header, ESP-IDF, standard library, project.

Use C++20 features that do not violate embedded constraints: `std::optional`, `std::variant`,
`std::string_view`, `std::span`, `constexpr`, `enum class`, structured bindings, and RAII wrappers.

Forbidden:

| Feature | Reason | Alternative |
| --- | --- | --- |
| `<iostream>` | Adds large binary size | `ESP_LOGx` |
| Exceptions | Disabled | `esp_err_t` or a project result enum/type |
| RTTI | Disabled | Avoid `dynamic_cast` designs |
| Unbounded allocation in deterministic loops or latency-critical tasks | Fragmentation and jitter | Static storage, fixed-capacity containers, or bounded startup allocation |

ETL and `tl::expected` are not repository dependencies. Do not require or include them unless a
separate dependency decision adds them. Existing code uses standard-library containers for bounded
startup/control-plane work and static or fixed-size storage in deterministic paths.

## Error Handling

Use `esp_err_t` for ESP-IDF-facing fallible operations and `ESP_RETURN_ON_ERROR` for simple
propagation. A small project enum or result type is appropriate when the caller needs domain-specific
errors.

```cpp
esp_err_t readSensor(SensorReading& reading) {
    uint8_t data[4];
    esp_err_t err = i2cRead(kSensorAddr, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }
    reading = SensorReading{data};
    return ESP_OK;
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

- Prefer initialization-time allocation and fixed-capacity storage.
- Do not add unbounded allocation to deterministic loops, ISRs, or latency-critical paths. Existing
  network and audio-library allocations must remain bounded and be reviewed for task impact.
- Use `std::unique_ptr` for factory-created init-time objects only.
- Use references for dependency injection after init.
- Use raw pointers only for C API interop or nullable references.
- Prefer static allocation for FreeRTOS tasks, queues, semaphores, and DMA buffers.
- Use fixed-width integer types for registers, protocol fields, buffers, and timestamps.
- Mark non-mutating methods and parameters `const`.
- Use Doxygen comments on public APIs.

## Architecture Rules

Hardware drivers used by service logic should expose an injectable interface when isolation has
meaningful test value. Real implementations live in `firmware/domes/main/drivers/`; host fakes and
simulators live under `firmware/test_app/`. Do not create a mock solely to satisfy a file-count rule.

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

`main/config.hpp` contains the verified NFF DevKit pin mapping. Keep the active mapping in that file
until the project adopts a generated or Kconfig-backed platform header. Add another board profile
only with verified pins, peripherals, flash configuration, partition layout, and hardware tests.

## Organization Principles

- Keep the design simple.
- Do not add backward compatibility wrappers during refactors; update call sites.
- Do not add umbrella headers.
- Delete unused code instead of commenting it out.
- Use one file for one purpose, usually one class per `.hpp`/`.cpp` pair.
- Avoid unnecessary directory nesting.

## Protocol And Runtime Config

Runtime configuration uses framed protobuf messages over UART0, TCP, and BLE. On the active NFF
DevKit, UART0 reaches the host through the CP2102N bridge; native USB Serial/JTAG remains the console
and debug interface.

```text
[AA 55 Len Type Payload CRC]
```

Key components:

| Component | File | Purpose |
| --- | --- | --- |
| UartTransport | `main/transport/uartTransport.hpp` | CP2102N-backed UART0 transport |
| TcpTransport | `main/transport/tcpTransport.hpp` | TCP client transport |
| TcpConfigServer | `main/transport/tcpConfigServer.hpp` | TCP server on port 5000 |
| BleOtaService | `main/transport/bleOtaService.hpp` | BLE GATT service |
| ConfigCommandHandler | `main/config/configCommandHandler.hpp` | Config protocol handler |
| FeatureManager | `main/config/featureManager.hpp` | Supported-feature mask, atomic runtime state, and change hooks |
| FrameCodec | `firmware/common/protocol/frameCodec.hpp` | Shared frame parsing |

Config and trace messages come from `firmware/common/proto/*.proto`. Existing OTA transfer structs,
compact trace recorder events, and internal ESP-NOW peer packets are bounded fixed-binary
exceptions. Keep mirrored implementations compatible and do not create another exception family.

Most command responses encode `[Status:u8][protobuf response]` inside the frame payload. Responses
without a command status, including list and diagnostic responses and unsolicited notifications,
contain only their protobuf. When adding a command or notification, document and test which
envelope it uses in both firmware and host code.

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
    TRACE_SCOPE(TRACE_ID("Game.Tick"), domes::trace::Category::kGame);
    TRACE_INSTANT(TRACE_ID("Game.Hit"), domes::trace::Category::kGame);
    TRACE_COUNTER(TRACE_ID("Game.Score"), score, domes::trace::Category::kGame);
}
```

Categories include `kKernel`, `kTransport`, `kOta`, `kWifi`, `kLed`, `kAudio`, `kTouch`, `kGame`,
`kUser`, `kHaptic`, `kBle`, `kNvs`, `kEspNow`, and `kSync`.

`kKernel` currently classifies explicit diagnostics and memory counters. FreeRTOS scheduler, ISR,
and queue trace hooks are not wired; `CONFIG_FREERTOS_USE_TRACE_FACILITY` is enabled for task-health
introspection, not automatic scheduler event capture.

Dump traces with:

```bash
domes-cli --port "$PORT" trace start
domes-cli --port "$PORT" system health
domes-cli --port "$PORT" trace stop
domes-cli --port "$PORT" trace dump -o trace.json --names tools/trace/trace_names.json
```

Open the output in `https://ui.perfetto.dev`.

## Initialization Order

In `main.cpp`, preserve this order:

1. WiFi before TCP config server and BLE.
2. BLE OTA service early.
3. FeatureManager before TCP/Serial/BLE config handlers.
4. TcpConfigServer before UART config/OTA.
5. UART config/OTA last.

Keep native USB console output separate from UART0 protocol bytes. Do not redirect ESP-IDF logs to
UART0 while config or OTA framing is active.

## Validation Checklist

- An ESP-IDF v5.4.4 build with a fresh build directory and isolated `SDKCONFIG` succeeds with no new
  warnings. Prefer `scripts/verify.sh` or the root `AGENTS.md` command; an ignored project-local
  `sdkconfig` is not final evidence.
- No unbounded allocation in ISRs, deterministic loops, or latency-critical tasks.
- No forbidden STL dynamic containers.
- No `<iostream>`, exceptions, or RTTI.
- Driver/service boundaries are injectable where host isolation provides meaningful coverage.
- Public APIs have Doxygen comments.
- Non-mutating methods are `const`.
- ISR code is IRAM-safe and uses only `FromISR` APIs.
- Error returns use `esp_err_t` or an established project result type.
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
| Protocol frames corrupted by logs | Keep the console on native USB Serial/JTAG; UART0 is protocol-only |

## Hardware Interfaces

| Peripheral | Interface | Driver IC | Notes |
| --- | --- | --- | --- |
| LEDs | RMT | Addressable LED ring | Current NFF profile uses 16 devices in RGB mode; RGBW is a production target |
| Audio | I2S | MAX98357A | 23 mm speaker |
| Haptic | I2C | DRV2605L | LRA motor |
| Touch | ESP32 touch peripheral | none | Capacitive sense |
| IMU | I2C | LIS2DW12 | Tap detection |
| Power | ADC | none | Battery voltage |

Reference documents:

- `firmware/MILESTONES.md`
- `docs/README.md`
- `docs/TESTING.md`
- `research/SOFTWARE_ARCHITECTURE.md`
- `research/architecture/README.md`
- `research/SYSTEM_ARCHITECTURE.md`
- `docs/PIN_REFERENCE.md`
- `research/architecture/07-debugging.md`
