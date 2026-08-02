# DOMES Software Architecture

> **Document status: Current as-built overview.** This document describes the software that is
> assembled in this repository. Product goals and proposed production hardware belong in
> [`SYSTEM_ARCHITECTURE.md`](SYSTEM_ARCHITECTURE.md); historical designs live under
> [`architecture/`](architecture/README.md).

Last checked against the repository: 2026-08-02.

## System Context

DOMES is split into three software surfaces:

| Surface | Responsibility | Primary location |
| --- | --- | --- |
| Pod firmware | Hardware control, per-pod state, local game timing, configuration endpoints, OTA, tracing, and ESP-NOW participation | `firmware/domes/` |
| Host CLI | Device discovery and registration, transport selection, command dispatch, multi-device fan-out, OTA upload, and trace export | `tools/domes-cli/` |
| Flutter app | Mobile UI and generated protocol consumers | `ios/domes_app/` |

The firmware does not own host device registration or multi-device CLI fan-out. Each pod owns its
own feature, mode, game, and trace state.

## Firmware Composition

[`firmware/domes/main/main.cpp`](../firmware/domes/main/main.cpp) is the composition root. It creates
long-lived drivers and services, wires their dependencies, starts managed tasks, and preserves the
initialization order required by WiFi, BLE, ESP-NOW, and USB-CDC.

The current runtime is organized as follows:

| Layer | Current implementation |
| --- | --- |
| Board configuration | `main/config.hpp` selects one `BOARD_*` target and supplies compiled pins and peripheral constants |
| Driver contracts | `main/interfaces/` contains hardware-facing interfaces used by services and host tests |
| Concrete drivers | `main/drivers/` implements LED, touch, IMU, haptic, and audio hardware access |
| Services | `main/services/` owns LED animation, touch/IMU polling, audio, WiFi, OTA, and ESP-NOW behavior |
| Device state | `FeatureManager` controls runtime feature bits; `ModeManager` controls system mode and mode-driven feature masks |
| Game state | `GameEngine` runs the per-pod arm, touch-or-timeout, and feedback state machine |
| Transports | USB-CDC, TCP, and BLE expose config/OTA framing; ESP-NOW provides pod-to-pod discovery and the current fixed drill |
| Infrastructure | NVS, task management, watchdog, diagnostics, shutdown snapshots, and memory profiling live under `main/infra/` |
| Observability | The trace recorder, command handler, Perfetto export data, and optional TCP stream live under `main/trace/` |

`FeatureManager`, `ModeManager`, `GameEngine`, and the trace recorder are per-pod state. The current
ESP-NOW service assigns roles from pod MAC addresses and runs a fixed master/slave drill. The
phone-selected master and general `DrillInterpreter` described in research documents are target
designs, not current production classes.

## Protocol Boundaries

USB-CDC, TCP, and BLE config traffic use the common frame defined by
[`frameCodec.hpp`](../firmware/common/protocol/frameCodec.hpp):

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

Length and CRC cover `Type + Payload`.

| Protocol family | Source of truth | Consumers | Status |
| --- | --- | --- | --- |
| Runtime config and system commands | `firmware/common/proto/config.proto` | nanopb firmware, prost CLI, generated Dart | Current protobuf contract |
| Trace control and metadata | `firmware/common/proto/trace.proto` | nanopb firmware, prost CLI | Current protobuf contract; fixed-size trace events remain binary records |
| OTA chunk transfer | `firmware/common/protocol/otaProtocol.hpp` plus the matching CLI implementation | Firmware and CLI | Legacy fixed-binary exception; keep wire-compatible until migrated |
| ESP-NOW game/discovery | `firmware/domes/main/services/espNowProtocol.hpp` | Pod firmware | Legacy packed-struct exception; current research packet tables are not authoritative |

New config or trace messages start in a `.proto` file. Do not introduce another hand-written wire
format. Changes to either legacy exception must update every consumer and include compatibility
tests; migration to protobuf remains architectural debt.

## CLI Architecture

The Rust CLI separates command handling from transports:

- `src/commands/` implements user-facing operations.
- `src/transport/` implements serial, TCP, and BLE connections plus frame handling.
- `src/protocol/` and generated prost types encode shared config and trace messages.
- `src/device.rs` owns named-device registration and discovery data.
- `src/main.rs` parses the command line and dispatches single-device or `--all` operations.

The CLI is the supported development and service interface. Standalone Python protocol utilities in
older research documents are historical unless a current workflow explicitly links to them.

## Verification Architecture

Host firmware tests are a standalone GoogleTest/CTest project under `firmware/test_app/`. They cover
shared codecs, generated messages, state managers, game behavior, and host multi-pod simulation.
They do not replace ESP-IDF builds or hardware verification.

CLI checks use Cargo. Firmware, transport, driver, OTA, and multi-pod changes require the strongest
applicable build and device checks in [`docs/TESTING.md`](../docs/TESTING.md). CI workflow files under
`.github/workflows/` are the executable CI definition.

## Authority Map

Use the following source when documents disagree:

| Subject | Authoritative source |
| --- | --- |
| Repository-wide contributor and verification rules | [`AGENTS.md`](../AGENTS.md) |
| Firmware coding and initialization rules | [`firmware/AGENTS.md`](../firmware/AGENTS.md) |
| CLI coding and test rules | [`tools/domes-cli/AGENTS.md`](../tools/domes-cli/AGENTS.md) |
| Current software boundaries | This document and the implementation it links |
| Runtime composition and initialization | `firmware/domes/main/main.cpp` |
| Compiled board selection and GPIO values | `firmware/domes/main/config.hpp`, verified against the board schematic |
| Physical NFF wiring | `hardware/nff-devboard/docs/schematic.pdf` |
| Config and trace message schemas | `firmware/common/proto/*.proto` |
| Current legacy OTA wire format | `firmware/common/protocol/otaProtocol.hpp` and CLI compatibility tests |
| Current ESP-NOW wire format | `firmware/domes/main/services/espNowProtocol.hpp` |
| NVS keys | `firmware/domes/main/infra/nvsConfig.hpp` and owning service headers |
| Flash size and partition layout | `firmware/domes/sdkconfig.defaults` and `firmware/domes/partitions.csv` |
| Build and verification commands | [`docs/TESTING.md`](../docs/TESTING.md) |
| Delivery and hardware-verification status | [`firmware/MILESTONES.md`](../firmware/MILESTONES.md) |
| Product goals and proposed production hardware | [`SYSTEM_ARCHITECTURE.md`](SYSTEM_ARCHITECTURE.md) |
| Historical rationale and future proposals | [`architecture/`](architecture/README.md) |

Generated files are never the design authority when their source schema or build input is present.
Test counts, binary sizes, and hardware results are observations: record the command, date, and
environment instead of treating copied numbers as permanent architecture.

## Change Discipline

An implementation change must update the owning source above and any user-facing summary that would
otherwise become false. A proposal becomes current architecture only after its implementation,
tests, and hardware status are linked from this document or the milestone tracker. Detailed research
documents remain non-normative even when parts of their design have been implemented.
