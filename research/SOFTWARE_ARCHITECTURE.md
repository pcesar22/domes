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
| Flutter app | BLE device control, direct multi-pod drill orchestration, OTA UI, and generated protocol consumption | `ios/domes_app/` |

The firmware does not own host device registration or multi-device CLI fan-out. Each pod owns its
own feature, mode, game, and trace state.

## Firmware Composition

[`firmware/domes/main/main.cpp`](../firmware/domes/main/main.cpp) is the composition root. It creates
long-lived drivers and services, wires their dependencies, starts managed tasks, and preserves the
initialization order required by WiFi, BLE, ESP-NOW, native USB console, and UART config/OTA.

The current runtime is organized as follows:

| Layer | Current implementation |
| --- | --- |
| Board configuration | `main/config.hpp` supplies the sole supported NFF profile's compiled pins and peripheral constants |
| Driver contracts | `main/interfaces/` contains hardware-facing interfaces used by services and host tests |
| Concrete drivers | `main/drivers/` implements LED, touch, IMU, haptic, and audio hardware access |
| Services | `main/services/` owns LED animation, touch/IMU polling, audio, build-gated WiFi, OTA, and ESP-NOW behavior |
| Device state | `FeatureManager` exposes and applies only build-supported runtime features; `ModeManager` controls system mode and its mode-owned feature mask |
| Game state | `GameEngine` runs the per-pod arm, touch-or-timeout, and feedback state machine |
| Transports | CP2102N-backed UART exposes config/trace/OTA; build-gated TCP exposes config only; BLE exposes config/trace/OTA; ESP-NOW provides pod-to-pod discovery and the current fixed drill |
| Infrastructure | NVS, task management, watchdog, diagnostics, shutdown snapshots, and memory profiling live under `main/infra/` |
| Observability | The trace recorder, command handler, Perfetto export data, and optional TCP stream live under `main/trace/` |

`FeatureManager`, `ModeManager`, `GameEngine`, and the trace recorder are per-pod state. The current
ESP-NOW service assigns roles from pod MAC addresses and runs a fixed master/slave drill. The
peer protocol accepts only the exact packed size for a known message, verifies that the claimed
sender matches the radio callback source, and correlates arm/touch/timeout traffic with a non-zero
per-round token. The phone-selected master and general `DrillInterpreter` described in research
documents are target designs, not current production classes.

ESP-NOW role status is lifecycle-scoped. `master` or `slave` is exposed only after the corresponding
game loop can service peer and benchmark traffic. A disable may briefly report `stopping` while that
loop unwinds; callers must wait for exact `disabled` before starting another lifecycle. Benchmark
admission rechecks the enabled feature, selected peer, active game loop, and transport under its
start lock, and the master's join-settle window accepts only selected-peer ping/pong traffic.

The active feature contract is operational rather than descriptive: LED, touch, audio, haptic,
ESP-NOW, and BLE advertising state gates the corresponding runtime path. WiFi appears in feature
responses only for `CONFIG_DOMES_WIFI_AUTO_CONNECT` builds; those builds connect and disconnect the
station client with stored credentials, and mode transitions preserve that client state. The
default build omits WiFi from `feature list` and rejects attempts to set it.

## Protocol Boundaries

UART, TCP, and BLE config traffic use the common frame defined by
[`frameCodec.hpp`](../firmware/common/protocol/frameCodec.hpp):

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

Length and CRC cover `Type + Payload`.

OTA uses message types `0x01-0x05`, trace uses `0x10-0x1B`, and config command requests/responses
use `0x20-0x4F` with reserved gaps. `0x50` is the unsolicited device-originated
`TouchEventNotification`, not a request. Its bare protobuf payload lets the Flutter BLE workflow
consume a physical touch without polling or simulating one.

Most config command responses wrap the response protobuf as `[Status:u8][Protobuf payload]`.
List and diagnostic responses without command status, plus unsolicited notifications, contain the
protobuf directly. The owning firmware sender and host decoder define this envelope per message.

On the active NFF DevKit, UART0 reaches the host through CP2102N (`/dev/ttyUSB*`; persistent identity
under `/dev/serial/by-id/`). Native ESP32-S3 USB Serial/JTAG (`/dev/ttyACM*`) carries console and JTAG
only, keeping text logs out of the framed UART stream.

| Protocol family | Source of truth | Consumers | Status |
| --- | --- | --- | --- |
| Runtime config, system commands, and touch notification | `firmware/common/proto/config.proto` | nanopb firmware, prost CLI, generated Dart | Current protobuf contract; requests/responses end at `0x4F`, unsolicited touch is `0x50` |
| Trace control and metadata | `firmware/common/proto/trace.proto` | nanopb firmware, prost CLI | Current protobuf contract; fixed-size synchronization/application events remain binary records; scheduler/ISR/queue IDs are reserved |
| OTA chunk transfer | `firmware/common/protocol/otaProtocol.hpp` plus the matching Rust and Dart implementations | Firmware, CLI, and Flutter app | Legacy fixed-binary exception; keep all consumers wire-compatible until migrated |
| ESP-NOW game/discovery | `firmware/domes/main/services/espNowProtocol.hpp` | Pod firmware | Legacy packed-struct exception with exact-size, source-MAC, and per-round-token validation; current research packet tables are not authoritative |

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

`devices scan` discovers local serial ports and BLE advertisements, not WiFi/mDNS targets. The
serial sniffer is a passive reader rather than a proxy and cannot share one exclusively opened port
with a command-producing process. Multi-pod trace grouping is performed by the separate
`tools/trace/trace_merge.py` utility after per-pod export. Its supported readiness workflow groups
local timelines by capture start; its other mode preserves raw local timestamps. The firmware does
not currently emit a truthful shared cross-clock marker, so neither output is synchronized timing
evidence.

Raw WiFi/TCP image transfer and generic trace control are not routed by the TCP config server. The
dedicated trace stream uses its own TCP endpoint. The WiFi client and TCP config server are compiled
only with `CONFIG_DOMES_WIFI_AUTO_CONNECT`; provisioned credentials take precedence, while
compile-time secrets seed NVS only on an unprovisioned first boot. The GitHub update client requires
the exact `domes-<tag>.bin` asset and a 64-hex-character application digest from release metadata.
Automatic WiFi field update is not a verified provisioning or release flow.

## Flutter Architecture

The Flutter app is a direct BLE controller, not the firmware's ESP-NOW master. Its layers mirror the
protocol boundary:

- `data/transport/ble_frame_channel.dart` owns frame reassembly, one command response waiter, and
  separation of unsolicited message types.
- `data/transport/ble_transport.dart` owns GATT discovery, writes, notifications, and connection
  state; device-originated `0x50` touch frames are published separately from command responses.
- `domain/repositories/pod_repository_impl.dart` parses the bare-protobuf touch notification and
  exposes a typed touch-event stream.
- `application/providers/multi_pod_provider.dart` owns each BLE connection and associates its touch
  stream with that pod's address.
- `application/providers/drill_provider.dart` selects the active direct-BLE target, applies mode and
  LED commands, accepts only the active pod's physical touch, and protects stopped or superseded
  rounds from stale asynchronous completions.

The production UI currently uses the separate single-pod connection provider and does not call
`MultiPodNotifier.connectPod`, so the internal multi-pod drill path has no end-to-end user flow that
populates its roster. The reaction, sequence, and speed labels also share one random-target execution
path; they are not three implemented drill semantics. App reaction scoring uses phone wall time even
though the touch notification carries a pod-local timestamp, and no cross-clock correlation exists.

Touch simulation is limited to explicit simulated pod addresses. The OTA chunk protocol remains a
bounded fixed-binary exception mirrored with firmware and the Rust CLI. Unit/widget tests cover the
app layers, but a physical app drill and mobile OTA remain hardware-verification gates in
[`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md). The intended user workflow and recovery policy live
in [`PRODUCT_DEFINITION.md`](PRODUCT_DEFINITION.md); they are requirements inputs, not current app
claims.

## Board Profile

The NFF carrier profile compiled directly in `config.hpp`, plus the checked-in 8 MB partition
table, is the only supported target. There is no board-selection mechanism or production profile.
A production 16 MB pin/config/partition profile must be implemented explicitly and verified before
it is treated as a build target.

The CLI is the supported development and service interface. Standalone Python protocol utilities in
older research documents are historical unless a current workflow explicitly links to them.

The 8 MB partition profile also reserves flash for ESP-IDF ELF panic dumps. Those dumps are decoded
with ESP-IDF and the exact matching application ELF. The legacy CLI `system crash-dump` response is
a separate NVS clean-restart snapshot. Its current format is CRC-protected and records the exact ELF
SHA-256, firmware version, boot count, internal heap, and processed PCs. Legacy records are
display-only with explicitly unverified field semantics; corrupt records fail closed and can be
removed with the explicit CLI clear option.

## Verification Architecture

Host firmware tests are a standalone GoogleTest/CTest project under `firmware/test_app/`. They cover
shared codecs, generated messages and notifications, state managers, OTA/release state, the
ESP-NOW packed contract, game behavior, trace integrity, and host multi-pod simulation. They do not
replace ESP-IDF builds or hardware verification.

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
| Active compiled board profile and GPIO values | `firmware/domes/main/config.hpp`, verified against the board schematic |
| Physical NFF wiring | `hardware/nff-devboard/docs/schematic.pdf` |
| Config and trace message schemas | `firmware/common/proto/*.proto` |
| Current legacy OTA wire format | `firmware/common/protocol/otaProtocol.hpp`, `tools/domes-cli/src/commands/ota.rs`, `ios/domes_app/lib/data/protocol/ota_protocol.dart`, and Rust/Dart compatibility tests |
| Current ESP-NOW wire format | `firmware/domes/main/services/espNowProtocol.hpp` |
| NVS keys | `firmware/domes/main/infra/nvsConfig.hpp` and owning service headers |
| Flash size and partition layout | `firmware/domes/sdkconfig.defaults` and `firmware/domes/partitions.csv` |
| Build and verification commands | [`docs/TESTING.md`](../docs/TESTING.md) |
| Delivery and hardware-verification status | [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) |
| Product goals and proposed production hardware | [`SYSTEM_ARCHITECTURE.md`](SYSTEM_ARCHITECTURE.md) |
| Historical rationale and future proposals | [`architecture/`](architecture/README.md) |

Generated files are never the design authority when their source schema or build input is present.
Test counts, binary sizes, and hardware results are observations: record the command, date, and
environment instead of treating copied numbers as permanent architecture.

## Change Discipline

An implementation change must update the owning source above and any user-facing summary that would
otherwise become false. A proposal becomes current architecture only after its implementation,
tests, and hardware status are linked from this document or the integrated program status. Detailed
research documents remain non-normative even when parts of their design have been implemented.
