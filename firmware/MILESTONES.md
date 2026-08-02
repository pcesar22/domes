# DOMES Delivery Milestones

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

A build or unit test does not establish hardware behavior. The 2026-08-02 review used two attached
NFF ESP32-S3 N8R8 pods over their CP2102N UART interfaces and an Intel AX210 BLE adapter. Command
acceptance is not physical proof of light, sound, vibration, touch, or motion behavior.
Hardware rows are dated observations of the image exercised during the review; they do not
automatically validate later working-tree edits. Rebuild, reflash, and repeat the applicable checks
on the final pull-request head before promoting any pending release gate.

## Current Capability Matrix

| Capability | Implemented | Host verified | Hardware status | Notes |
| --- | --- | --- | --- | --- |
| ESP-IDF application, tasks, NVS, watchdog | Yes | Yes | Verified on both attached pods | Both pods flashed, booted, passed ten on-device self-tests, and reported healthy task and heap state. |
| 16-pixel LED ring | Yes | Yes | Control path verified; visual output not confirmed | Solid, breathing, cycle, and off commands round-tripped on both pods. Current NFF firmware drives RGB LEDs on GPIO16; RGBW remains a production target. |
| Capacitive touch, four pads | Yes | Yes | Simulated input verified; physical touch not confirmed | Pad simulation for GPIO1, GPIO2, GPIO4, and GPIO6 was accepted on both pods. |
| LIS2DW12 IMU and tap detection | Yes | Yes | Initialization/control verified; physical taps not confirmed | Both self-tests found the peripheral and triage mode toggled; a physical tap was not exercised. |
| UART config protocol | Yes | Yes | Verified on both attached pods | Framed protobuf messages use UART0 GPIO43/GPIO44 through the CP2102N bridge. Each pod completed 100 fresh CLI connections without rebooting. |
| Build-gated WiFi/TCP config protocol | Yes | Yes | Not exercised in this review | The default profile intentionally omits WiFi. Device verification requires a `CONFIG_DOMES_WIFI_AUTO_CONNECT` image and stored credentials. |
| BLE config and OTA transport | Yes | Yes for framing and reassembly | Config and diagnostics verified on both pods; OTA verified on Pod 2 | Native Linux with Intel AX210 discovered each pod and completed sequential info, health, self-test, memory, and feature commands. Pod 2 also completed the BLE OTA acceptance sequence below. |
| Serial OTA | Yes | Yes | Verified on Pod 1; also used to update both pods during the review | Full-image transfer, exact embedded-version matching, first- and second-boot acceptance, health/self-test, truncated-image rejection, interrupted-session recovery, and subsequent update recovery passed. |
| BLE OTA | Yes | Yes for host protocol and fragmentation | Verified on Pod 2 | Full-image transfer, UART/BLE reconnection, exact embedded-version matching, first- and second-boot acceptance, health/self-test, truncated-image rejection, interrupted-session recovery, and subsequent update recovery passed. |
| Raw WiFi/TCP OTA transfer | No | Yes, rejection path | Not applicable | The TCP config server does not dispatch OTA frame types, and the CLI rejects this unsupported route before connecting. |
| ESP-NOW discovery, benchmark, and two-pod drill | Yes | Yes, including simulation | Verified on both attached pods | Three fresh enable/disable lifecycles completed 300/300 benchmark rounds from each pod with zero failures. A separate trace-backed ten-round simulated drill completed with one peer per pod and zero radio send failures. |
| Game engine finite-state machine | Yes | Yes | Trace-backed simulated drill verified; physical input pending | Arm, touch or timeout, then feedback. |
| DRV2605L haptic output | Yes | Partial | Not verified | Configured conservatively for the schematic's LD0832AA-0099F at fixed 235 Hz open-loop drive; confirm the populated part and physical output. |
| MAX98357A audio output | Driver exists | Partial | Not verified | Basic I2S/DMA path exists; sample storage and CLI volume workflow remain incomplete. |
| Tracing and Perfetto export | Yes | Yes | Two-pod capture, dump, and merge verified | The final source candidate produced 384 and 467 device events, 885 merged events, and zero drops. The merge groups local timelines by capture start; no truthful cross-clock correlation exists yet. |
| Diagnostics and memory profiling | Yes | Yes | Verified over UART and BLE on both pods | Includes system health, ESP-NOW statistics, self-test, and bounded memory samples. |
| Protocol sniffer | Partial | Yes | Normal live topology not verified | Config IDs derive from generated protobuf enums. The passive reader cannot share the command UART and still needs a non-resetting mirrored/capture workflow. |
| Restart snapshot retrieval | Yes | Yes for format and CLI contract | Verified on both pods | Format-2 records are CRC protected and bind boot count, firmware version, internal heap, processed PCs, and the exact pre-restart ELF SHA-256. Both records resolved against retained ELFs; corrupt-record failure and explicit clearing also passed. This remains distinct from a panic core dump. |
| Panic core-dump storage | Yes | Build/config verified | Deliberate panic and decode not verified | The 8 MB partition table reserves `0x20000` bytes and ESP-IDF ELF flash dumps are enabled. Retrieval uses ESP-IDF tooling and the exact matching ELF, not `domes-cli system crash-dump`. |
| Flutter application | Substantial prototype | Yes, analysis and unit/widget tests | BLE/device app workflow not verified | Providers, protocol, BLE fragmentation, OTA abort, and UI flows exist; generated bindings match the schema. The Rust CLI remains the primary service tool. |

## Verification Snapshot

| Check | Most recent result | Scope |
| --- | --- | --- |
| Host firmware tests | 267/267 passed in a fresh build on 2026-08-02 | Protocol, services, simulation, and selected drivers on the final pre-hardware source revision. |
| ESP-IDF firmware build | Exact ESP-IDF v5.4.4 clean build passed on 2026-08-02 | The 1,433,120-byte source candidate fit the `0x1E0000` OTA slot with 532,960 bytes free; rollback was enabled in the resolved configuration. |
| Rust CLI | Format, strict Clippy, debug/release locked builds, and 95/95 tests passed on 2026-08-02 | The count is 85 unit tests plus 10 CLI integration tests. |
| Flutter app | Pinned Flutter 3.44.8 analysis, 161 tests, and a Linux release build passed locally on 2026-08-02 | The local toolchain matches CI. Native iOS build remains unavailable on Linux. |
| Protocol generation | Nanopb and Dart drift checks passed on 2026-08-02 | `tools/generate_protocols.sh --check all`. |
| Attached hardware | Two NFF N8R8 pods exercised on 2026-08-02 | Erase plus merged-factory programming, normal multi-image programming, UART/BLE diagnostics, feature/mode control, registry fan-out, serial/BLE OTA and recovery, forced rollback, repeated two-way ESP-NOW benchmarks, and a trace-backed drill passed. Physical output/input remains listed below. |

Use `ctest -N` for the live test count. Do not copy a count into another document.

## Two-Board CLI Readiness Snapshot

| Review target | Stable hardware identity | Runtime identity |
| --- | --- | --- |
| Pod 1 | CP2102N serial `5edf3f45576def11a245cea7c169b110` | Pod ID 1; WiFi MAC `94:a9:90:0a:eb:c0`; BLE `94:A9:90:0A:EB:C2` |
| Pod 2 | CP2102N serial `002a9f8e536def119f38c1a7c169b110` | Pod ID 2; WiFi MAC `94:a9:90:0a:ea:50`; BLE `94:A9:90:0A:EA:52` |

Use the CP2102N serial-number links under `/dev/serial/by-id/`; kernel `ttyUSB` numbers are not
device identity.

| Command group | 2026-08-02 result | Remaining evidence |
| --- | --- | --- |
| `system` | Info, mode, health, ten-test self-test, memory, restart snapshot, and pod-ID validation passed on both boards. Legal mode transitions succeeded and an invalid transition was rejected. | Deliberate panic/core-dump decode is separate from the clean-restart snapshot. |
| `feature` | List, status, disable, and enable round-tripped for LED, BLE advertising, touch, haptic, audio, and ESP-NOW as applicable to the active mode. | Physical peripheral observations remain separate. |
| `led` | Off, solid red/green/blue/white, breathing, cycle, and get-pattern commands round-tripped; invalid fields were rejected. | Visually confirm color order, brightness, animation, and all 16 devices. |
| `touch` / `imu` | All four simulated pads and IMU triage enable/disable passed. | Exercise all physical pads, orientation changes, and physical tap interrupt/feedback. |
| `devices` and multi-target dispatch | Add/list/scan/remove, named targets, `--all`, duplicate-address rejection, UART discovery, and BLE discovery passed. | WiFi/mDNS discovery is intentionally absent; BLE names and MACs require deliberate canonical registration before destructive fan-out. |
| BLE transport | Scan plus sequential system, health, self-test, memory, feature, and advertising-state checks passed for both addresses. Pod 2 completed a full BLE OTA plus UART/BLE checks on the first and second boots. | Flutter/mobile BLE workflow remains unverified. |
| ESP-NOW | Both pods repeatedly crossed exact `disabled` lifecycle boundaries, discovered one peer, selected complementary deterministic roles, and completed 300/300 benchmark packets from each pod with zero failures. | Six-pod timing and recovery remain unverified. |
| WiFi/TCP | Default-build feature/status commands returned `InvalidFeature`; raw OTA and generic trace preflight reject unsupported TCP routes before connecting. | Build with `CONFIG_DOMES_WIFI_AUTO_CONNECT`, provision credentials, and test config TCP port 5000 plus the dedicated trace endpoint. |
| OTA | Pod 1 serial OTA and Pod 2 BLE OTA passed full transfer, exact-version, first/second boot, rollback-availability, and health/self-test checks. Truncated and interrupted recovery passed on the respective transports. Merged-factory programming and a separately forced failed-self-test rollback also passed. | Release tags still require the same exact-artifact checks; a successful normal OTA is not rollback evidence. |
| Trace and sniffer | Host codecs, dump integrity, trace grouping, name generation, and passive sniffer tests pass. The latest two-board drill capture produced the required hit/send/receive/completion events with zero drops. | Passive sniffing cannot share an exclusively opened command UART; a non-resetting mirror topology remains undefined. |

The 100-round ESP-NOW benchmark measured command/acknowledgment round-trip latency, not one-way
radio latency:

| Direction | Received across three fresh sessions | Observed min | Per-session mean range | Observed max |
| --- | --- | --- | --- | --- |
| Pod 1 to Pod 2 | 300/300 | 2.782 ms | 3.923-4.740 ms | 19.928 ms |
| Pod 2 to Pod 1 | 300/300 | 2.771 ms | 3.852-4.162 ms | 12.745 ms |

These results establish lossless exchange across repeated fresh lifecycles on the reviewed boards;
they measure command/acknowledgment round trips rather than one-way radio latency and do not demonstrate
the product architecture's sub-millisecond latency target.

## Milestone History And Remaining Work

### M1: Project Skeleton

**Status:** Complete

- [x] ESP-IDF application and component layout
- [x] ESP32-S3 configuration and USB console
- [x] Development partition table with two OTA app slots
- [x] Compiled active-board pin mapping in `main/config.hpp`
- [x] Reproducible firmware build

### M2: LED And Touch Bring-Up

**Status:** Implementation complete; current physical confirmation pending

- [x] LED driver interface and RMT implementation
- [x] LED effects and runtime control
- [x] Four-pad capacitive touch driver
- [x] Baseline calibration and threshold detection
- [x] Host coverage for core behavior
- [ ] Visually confirm all LED patterns on both attached NFF boards
- [ ] Physically exercise each touch pad on both attached NFF boards

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
- [x] CP2102N-backed UART0 config transport
- [x] TCP config transport
- [x] BLE GATT config and OTA transport
- [x] Serial OTA receiver and CLI sender
- [x] Verify the advertised OTA SHA-256 digest before accepting an image
- [x] Reject unsupported raw WiFi/TCP OTA before connection or file transfer
- [x] Cover the shared OTA wire contract and host abort path in automated tests
- [x] Reject a truncated serial image and recover without changing the running image
- [x] Recover from an interrupted serial session after the 15-second inactivity timeout
- [x] Repeat BLE transfer, abort, and recovery checks on-device

Config and trace payloads are protobuf-encoded. OTA and the internal ESP-NOW peer protocol are
bounded legacy fixed-binary exceptions; their C++ and Rust or simulator definitions must be changed
together until they are migrated.

### M6: Multi-Pod Runtime

**Status:** Two-pod implementation, host simulation, and on-device drill verified

- [x] ESP-NOW discovery, beacon, unicast, and heartbeat paths
- [x] Deterministic role assignment
- [x] Ten-round two-pod drill loop
- [x] Game engine state machine
- [x] Multi-pod simulation tests
- [x] Verify two-pod discovery and bidirectional 100-packet benchmarks with no loss
- [x] Implement direct-BLE app drill orchestration with device-originated physical-touch notifications
- [x] Re-run the fixed drill on the final source candidate with simulated hits and trace evidence
- [ ] Validate timing and recovery with six physical pods
- [ ] Implement the phone-selected ESP-NOW master and general drill interpreter product target

### M7: Observability

**Status:** Partial

- [x] System health and ESP-NOW diagnostics
- [x] Trace recording, dump, live stream, and multi-device post-processing tools
- [x] ESP-NOW latency benchmark
- [x] Mutex and semaphore trace instrumentation
- [x] Host frame-sniffer decoding, filtering, and capture output
- [ ] Define and verify a non-resetting live capture/mirror topology
- [x] Memory profiler
- [x] Clean-restart snapshot stored in NVS and exposed by the legacy `crash-dump` command
- [x] Bind format-2 restart snapshots to CRC-protected firmware, boot, heap, PC, and exact ELF identity
- [x] Verify exact-ELF symbolization on both pods and explicit recovery from an unreadable record
- [x] Extend the sniffer mapping and config filter through the full current message range
- [x] Bound memory-profile responses to the shared frame payload limit
- [x] Reserve flash coredump storage and enable ESP-IDF ELF core dumps
- [ ] Deliberately trigger, retrieve, and decode a panic dump with the matching ELF

FreeRTOS task-health introspection is enabled, but scheduler, ISR, and queue event hooks are not
wired into the recorder. Their protocol IDs are reserved and are not a delivered trace capability.

Current multi-pod merge supports capture-start grouping (`zero`) and unshifted local timestamps
(`raw`) only. Neither mode correlates pod clocks. Do not claim cross-pod timing correlation until the
firmware emits and validates a truthful synchronization marker.

### M8: NFF Integration And Production Readiness

**Status:** In progress

- [x] Compile the active NFF GPIO map
- [x] Verify app flashing, UART/BLE diagnostics, feature/mode control, registry fan-out, and two-pod ESP-NOW discovery/benchmark on two attached pods
- [ ] Repeat the full bring-up checklist and retain dated evidence
- [x] Configure a conservative fixed-frequency profile for the schematic's LD0832AA-0099F LRA
- [ ] Verify LRA haptics on the populated NFF hardware
- [ ] Verify audio playback and volume control
- [x] Confirm serial OTA through full transfer, expected-version boot, health/self-test, and a second reboot
- [x] Exercise serial invalid-image rejection and interrupted-session recovery on hardware
- [x] Confirm BLE OTA through full transfer, UART/BLE reconnection, health/self-test, and a second reboot
- [x] Program and verify the merged factory image from address `0x0`
- [x] Force a post-OTA self-test failure and verify rollback to the previous image
- [x] Remove unsupported raw WiFi/TCP OTA from the CLI contract
- [ ] Validate six-pod timing and failure recovery
- [ ] Define the production RGBW and 16 MB board profile separately from the 8 MB NFF profile

## Active NFF Pin Authority

The schematic/netlist owns physical connectivity, and `firmware/domes/main/config.hpp` owns the
compiled board mapping. Use [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) for their maintained
reconciliation; this status ledger intentionally does not duplicate the pin table.

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
| Physical peripherals | Visually confirm LED patterns and physically exercise touch, IMU taps, haptic output, and audio output on both pods. Accepted CLI commands are not sufficient evidence. |
| Panic diagnostics | Deliberately capture and decode an ELF flash coredump with the exact matching firmware ELF; do not relabel clean restart data. |
| Hardware CI runner | Provision an online two-device Linux x64 runner using Actions Runner 2.327.1 or newer before applying `hw-test`. |
| Six-pod demo | Manufacture additional pods, then run timing and recovery tests. |
| Production profile | Separate target RGBW/16 MB settings from the current NFF RGB/8 MB build. |
| Repository license | Select and add a license before describing distribution or contribution terms as settled. |
