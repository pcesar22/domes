# DOMES Firmware Milestones

This document owns delivery status. It records what is implemented, what has passed host
verification, and what has been exercised on hardware. Product targets belong in
[`research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md); verification commands and
evidence requirements belong in [`docs/TESTING.md`](../docs/TESTING.md).

**Status reviewed:** 2026-08-02

## Status Vocabulary

| State | Meaning |
| --- | --- |
| Implemented | The production path exists in the repository and builds. |
| Host verified | Automated host tests cover the behavior or its protocol contract. |
| Hardware verified | The behavior was exercised on a named physical platform. |
| Blocked | A known dependency or defect prevents the next verification step. |

A build or unit test does not establish hardware behavior. Historical hardware results remain
listed, but this review could not repeat them because no DOMES serial device was attached.

## Current Capability Matrix

| Capability | Implemented | Host verified | Hardware status | Notes |
| --- | --- | --- | --- | --- |
| ESP-IDF application, tasks, NVS, watchdog | Yes | Yes | Recorded on two NFF pods | Initialization order is documented in `firmware/AGENTS.md`. |
| 16-pixel LED ring | Yes | Yes | Recorded on NFF pods | Current NFF firmware drives the populated LEDs in RGB mode on GPIO16. RGBW remains a production target. |
| Capacitive touch, four pads | Yes | Yes | Recorded on NFF pods | GPIO1, GPIO2, GPIO4, and GPIO6. |
| LIS2DW12 IMU and tap detection | Yes | Yes | Recorded on NFF pods | I2C address `0x19`, interrupt on GPIO5. |
| Serial config protocol | Yes | Yes | Recorded on NFF pods | Framed protobuf messages over USB CDC. |
| TCP config protocol | Yes | Yes | Recorded on NFF pods | WiFi TCP server on port 5000. |
| BLE config and OTA transport | Yes | Partial | Recorded on NFF pods | Native Linux is required for validation-critical BLE tests. |
| Serial OTA | Yes | Partial | Recorded on NFF pods | Receiver verifies the declared size and SHA-256 digest before selecting the boot partition; corruption handling still needs device verification. |
| Raw WiFi/TCP OTA transfer | No | Yes, rejection path | Not applicable | The TCP config server does not dispatch OTA frame types, and the CLI rejects this unsupported route before connecting. |
| ESP-NOW discovery and two-pod drill | Yes | Yes, including simulation | Recorded on two pods | Current service uses deterministic MAC-based roles and a fixed drill. |
| Game engine finite-state machine | Yes | Yes | Recorded on two pods | Arm, touch or timeout, then feedback. |
| DRV2605L haptic output | Yes | Partial | Not verified | Configured conservatively for the schematic's LD0832AA-0099F at fixed 235 Hz open-loop drive; confirm the populated part and physical output. |
| MAX98357A audio output | Driver exists | Partial | Not verified | Basic I2S/DMA path exists; sample storage and CLI volume workflow remain incomplete. |
| Tracing and Perfetto export | Yes | Yes | Recorded on NFF pods | CLI supports capture, dump, merge, and live stream workflows. |
| Diagnostics and memory profiling | Yes | Yes | Not reverified | Includes system health, ESP-NOW statistics, and memory samples. |
| Protocol sniffer | Yes | Yes | Not reverified | Config IDs derive from generated protobuf enums through the current message range. |
| Restart snapshot retrieval | Yes | Partial | Not reverified | The legacy `crash-dump` command returns a clean `esp_restart()` snapshot, not a panic core dump. |
| Panic core-dump retrieval | No | No | Not verified | Flash core dumps are configured, but the partition table has no coredump partition or retrieval path. |
| Flutter application | Substantial prototype | Yes, analysis and widget/unit tests | BLE/device behavior not verified | Providers, protocol, BLE, OTA, and UI flows exist; committed config bindings match the schema. The Rust CLI remains the primary service tool. |

## Verification Snapshot

| Check | Most recent result | Scope |
| --- | --- | --- |
| Host firmware tests | 217 tests passed on 2026-08-02 | Protocol, services, simulation, and selected drivers. |
| ESP-IDF firmware build | ESP-IDF 5.4 build passed on 2026-08-02 | `domes.bin` is `0x14c790` bytes with 31% of the smallest app partition free. |
| Rust CLI | Format, Clippy, release build, and 26 tests passed on 2026-08-02 | Clippy reports existing non-fatal warnings. |
| Flutter app | Analysis and 113 tests passed on 2026-08-02 | Generated Dart bindings match `config.proto`. |
| Protocol generation | Nanopb and Dart drift checks passed on 2026-08-02 | `tools/generate_protocols.sh --check all`. |
| Attached hardware | Unavailable on 2026-08-02 | No `/dev/ttyACM*` devices were present. |

Use `ctest -N` for the live test count. Do not copy a count into another document.

## Milestone History And Remaining Work

### M1: Project Skeleton

**Status:** Complete

- [x] ESP-IDF application and component layout
- [x] ESP32-S3 configuration and USB console
- [x] Development partition table with two OTA app slots
- [x] Board selection and compiled pin mapping in `main/config.hpp`
- [x] Reproducible firmware build

### M2: LED And Touch Bring-Up

**Status:** Complete for the current NFF board

- [x] LED driver interface and RMT implementation
- [x] LED effects and runtime control
- [x] Four-pad capacitive touch driver
- [x] Baseline calibration and threshold detection
- [x] Host coverage for core behavior
- [x] Recorded NFF hardware validation

The current production driver creates an RMT LED device. Earlier DevKit experiments involving an
SPI backend are historical and are not a supported backend in the present source tree.

### M3: Runtime Infrastructure

**Status:** Complete

- [x] Managed FreeRTOS task structure
- [x] Logging and watchdog helpers
- [x] NVS configuration and pod identity
- [x] Runtime feature management
- [x] Shared synchronization and error-handling helpers

### M4: Sensor And Feedback Peripherals

**Status:** Partial

- [x] Shared I2C bus and scan support
- [x] LIS2DW12 driver and tap service
- [x] DRV2605L driver implementation
- [x] Configure the DRV2605L for the BOM/schematic LD0832AA-0099F using a bounded rated-voltage and resonance profile
- [ ] Verify haptic effects on NFF hardware
- [x] MAX98357A I2S/DMA driver implementation
- [ ] Complete sample storage and the host volume-control workflow
- [ ] Verify audio playback on NFF hardware

### M5: Host Communication And OTA

**Status:** Partial

- [x] Shared config frame codec
- [x] Protobuf config and trace schemas
- [x] USB CDC config transport
- [x] TCP config transport
- [x] BLE GATT config and OTA transport
- [x] Serial OTA receiver and CLI sender
- [x] Verify the advertised OTA SHA-256 digest before accepting an image
- [x] Reject unsupported raw WiFi/TCP OTA before connection or file transfer
- [ ] Add receiver-level OTA tests covering corruption and digest mismatch

Config and trace payloads are protobuf-encoded. OTA and the internal ESP-NOW peer protocol are
bounded legacy fixed-binary exceptions; their C++ and Rust or simulator definitions must be changed
together until they are migrated.

### M6: Multi-Pod Runtime

**Status:** Two-pod implementation complete; product-scale validation pending

- [x] ESP-NOW discovery, beacon, unicast, and heartbeat paths
- [x] Deterministic role assignment
- [x] Ten-round two-pod drill loop
- [x] Game engine state machine
- [x] Multi-pod simulation tests
- [x] Recorded two-pod hardware exercise
- [ ] Validate synchronization and recovery with six physical pods
- [ ] Implement the app-directed orchestration described as a product target

### M7: Observability

**Status:** Partial

- [x] System health and ESP-NOW diagnostics
- [x] Trace recording, dump, live stream, and multi-device correlation
- [x] ESP-NOW latency benchmark
- [x] Synchronization instrumentation
- [x] Frame sniffer and capture output
- [x] Memory profiler
- [x] Clean-restart snapshot stored in NVS and exposed by the legacy `crash-dump` command
- [x] Extend the sniffer mapping and config filter through the full current message range
- [x] Bound memory-profile responses to the shared frame payload limit
- [ ] Add a coredump partition and implement real panic/core-dump retrieval

### M8: NFF Integration And Production Readiness

**Status:** In progress

- [x] Compile the active NFF GPIO map
- [x] Record LED, touch, IMU, serial, WiFi, BLE, and two-pod ESP-NOW validation
- [ ] Repeat the full bring-up checklist and retain dated evidence
- [x] Configure a conservative fixed-frequency profile for the schematic's LD0832AA-0099F LRA
- [ ] Verify LRA haptics on the populated NFF hardware
- [ ] Verify audio playback and volume control
- [ ] Exercise serial and BLE OTA with digest/corruption checks
- [x] Remove unsupported raw WiFi/TCP OTA from the CLI contract
- [ ] Validate six-pod synchronization and failure recovery
- [ ] Define the production RGBW and 16 MB board profile separately from the 8 MB NFF profile

## Active NFF Pin Summary

The schematic/netlist owns physical connectivity. `firmware/domes/main/config.hpp` owns the compiled
board mapping. [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) explains how the NFF header
positions map to ESP32 GPIO numbers.

| Function | Active NFF GPIO |
| --- | --- |
| LED data | 16 |
| I2C SDA / SCL | 8 / 9 |
| IMU interrupt | 5 |
| I2S BCLK / LRCLK / data | 12 / 11 / 13 |
| Audio shutdown | 7 |
| Touch pads | 1 / 2 / 4 / 6 |

## Release Gates

A capability is ready to mark complete only when all applicable gates pass:

1. Protocol and source ownership are documented.
2. Focused automated tests pass.
3. The firmware and affected host tools build.
4. Hardware-facing behavior is exercised on the named board and transport.
5. Evidence and any unavailable checks are recorded in the pull request.

The firmware binary must fit the smallest configured OTA app slot; the current development limit is
`0x1E0000` bytes, not the historical one-megabyte target.

## Known Blockers And Decisions Needed

| Item | Required decision or work |
| --- | --- |
| Haptic hardware | Confirm the populated actuator is LD0832AA-0099F and record physical output at the configured voltage and frequency. |
| OTA hardware | Exercise serial and BLE transfer, digest mismatch, reboot, and rollback behavior on a pod. |
| Panic diagnostics | Allocate coredump storage and define a real retrieval format; do not relabel clean restart data. |
| Six-pod demo | Manufacture additional pods, then run synchronization and recovery tests. |
| Production profile | Separate target RGBW/16 MB settings from the current NFF RGB/8 MB build. |
| Repository license | Select and add a license before describing distribution or contribution terms as settled. |
