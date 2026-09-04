# Firmware Development Guidelines

These rules supplement the repository root `AGENTS.md`. For commands and hardware procedures, use
`firmware/README.md`, `docs/TESTING.md`, and `.codex/PLATFORM.md`.

## Project Context

DOMES firmware targets ESP32-S3 with FreeRTOS under ESP-IDF v5.4.4. Application code is C++20;
low-level drivers may use C. This is not an Arduino project.

| Aspect | Current contract |
| --- | --- |
| Active board | NFF carrier with the checked-in 8 MB partition layout |
| Production target | ESP32-S3-WROOM-1-N16R8; not the active partition profile |
| Error handling | `esp_err_t` or established project enums/results; no exceptions |
| Deterministic storage | Fixed capacity in ISRs and latency-critical loops; bounded startup allocation elsewhere |

## Coding Standards

| Element | Convention | Example |
| --- | --- | --- |
| Files | camelCase | `ledDriver.hpp` |
| Classes | PascalCase | `LedDriver` |
| Interfaces | `I` + PascalCase | `IHapticDriver` |
| Methods and variables | camelCase | `playEffect()`, `reactionTime` |
| Members | camelCase + trailing underscore | `ledDriver_` |
| Constants | `k` + PascalCase | `kMaxRetries` |
| Namespaces | lowercase | `config` |
| Macros | SCREAMING_SNAKE_CASE | `CONFIG_DOMES_LED_COUNT` |

Use 4 spaces, no tabs, K&R braces, a 100-character line limit, `Type* ptr` pointer style,
`#pragma once`, and include order of corresponding header, ESP-IDF, standard library, then project.

Use embedded-safe C++20 such as `std::optional`, `std::variant`, `std::string_view`, `std::span`,
`constexpr`, `enum class`, structured bindings, and RAII wrappers. Do not use `<iostream>`,
exceptions, RTTI, or unbounded allocation in deterministic loops, ISRs, or latency-critical tasks.
ETL and `tl::expected` are not dependencies; do not introduce them without a separate dependency
decision.

## Errors And Logging

Use `esp_err_t` for ESP-IDF-facing fallible operations and `ESP_RETURN_ON_ERROR` for straightforward
propagation. A small project enum/result is appropriate when callers need domain-specific errors.
Never ignore an ESP-IDF error result.

Use `ESP_LOGE/W/I/D` with a module-local `static constexpr const char* kTag`. Never log from ISR
context. Keep all logs on native USB Serial/JTAG, never on framed UART0.

## ISR Safety

ISRs must be minimal and defer work to tasks:

- Mark handlers `IRAM_ATTR` and cache-sensitive data `DRAM_ATTR`.
- Do not log, allocate, use floating point, or block.
- Use only FreeRTOS APIs with the `FromISR` suffix and request a yield when needed.
- Keep execution below about 10 microseconds.

## Memory And Ownership

- Prefer initialization-time allocation, static FreeRTOS objects, and fixed-capacity buffers.
- Existing network/audio allocations must remain bounded and be reviewed for task impact.
- Use `std::unique_ptr` only for factory-created initialization-time objects.
- Use references for dependency injection after initialization.
- Use raw pointers only for C interop or nullable references.
- Use fixed-width integers for registers, protocols, buffers, and timestamps.
- Make non-mutating methods/parameters `const` and document public APIs with Doxygen.
- Protect shared state with the project RAII mutex wrapper.

## Architecture Rules

Drivers used by service logic should expose an injectable interface when that provides meaningful
host-test isolation. Real implementations live in `firmware/domes/main/drivers/`; fakes and
simulators live under `firmware/test_app/`. Services receive driver interfaces through
constructors, not globals. Do not add mocks solely to satisfy a structure rule.

Core affinity remains:

| Core | Work |
| --- | --- |
| 0 | WiFi, BLE, and ESP-NOW stack tasks |
| 1 | Audio and game logic |
| Either | LED updates and touch polling |

`firmware/domes/main/config.hpp` owns the verified active pin mapping. Add another board profile
only with verified pins, peripherals, flash configuration, partition layout, and hardware tests.

Keep designs direct: update call sites instead of adding compatibility wrappers, avoid umbrella
headers and unnecessary nesting, and delete unused code rather than commenting it out.

## Protocol And Multi-Device Invariants

Follow the root protobuf and framing contract. Key implementation points are
`firmware/common/protocol/frameCodec.hpp`, `firmware/domes/main/config/configCommandHandler.hpp`,
`firmware/domes/main/config/featureManager.hpp`, and `firmware/domes/main/transport/`.

FeatureManager, ModeManager, GameEngine, and TraceRecorder are per-pod singletons by design. The
transport trait represents one connection; multi-device fan-out belongs in host CLI dispatch.
`pod_id` is stored in NVS as `config_key::kPodId`, with valid range 1-255.

## Initialization Order

Preserve this order in `main.cpp`:

1. WiFi before TCP config server and BLE for coexistence.
2. BLE OTA service early because advertising starts automatically.
3. FeatureManager before TCP, serial, and BLE config handlers.
4. TCP config server before UART config/OTA.
5. UART config/OTA last, after native USB console availability.

UART0 is framed protocol only. If native USB is absent, use BLE diagnostics or attach the separate
native USB connection before depending on boot logs.

## Firmware Verification

Use `firmware/README.md` and the canonical helpers under `tools/firmware/` for build and device
operations. The required software and physical evidence for OTA, BLE, and multi-device work is defined in
`docs/TESTING.md`; platform prerequisites and stable device paths are in `.codex/PLATFORM.md`.

Before handoff, check the changed code for bounded allocation, ISR-safe APIs, error propagation,
correct mutex use, active board configuration, and adequate host isolation. A clean ESP-IDF v5.4.4
build with a fresh `SDKCONFIG` is required for firmware code changes. Hardware-facing completion
still requires the physical evidence mandated by the root verification contract.

Common embedded hazards include DMA buffers in PSRAM, WiFi/BLE coexistence changes, insufficient
task stacks, watchdog starvation, flash writes from ISR context, and logs mixed into UART0 frames.
