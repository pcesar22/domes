# DOMES Repository Guide

## Start Here

Inspect the worktree before editing and preserve user changes. Search the repository before asking
for project facts. Apply the nearest nested `AGENTS.md` in addition to this file.

Use progressive disclosure:

| Work area | Scoped instructions or workflow |
| --- | --- |
| `firmware/` | `firmware/AGENTS.md`, `firmware/README.md`, and `docs/TESTING.md` |
| ESP32 crashes or stepping | `docs/DEBUGGING.md` |
| `tools/domes-cli/` | `tools/domes-cli/AGENTS.md` |
| `ios/domes_app/` | `ios/domes_app/AGENTS.md` |
| `hardware/` | `hardware/AGENTS.md` |
| Program phases, gates, or project status | `PROGRAM_STATUS.md` and `docs/PRODUCT_REALIZATION_FRAMEWORK.md` |
| Platform, BLE, USB, or multi-device setup | `docs/PLATFORM.md` |
| Host capability discovery | `scripts/doctor.sh` (use `--json` for automation) |
| Detailed verification commands | `docs/TESTING.md` |

## Truth And Authority

Current code, generated artifacts, tests, and current-status documents describe the as-built
system. `research/SYSTEM_ARCHITECTURE.md` is the product target, not proof of current behavior.
`research/architecture/` contains historical and proposed records whose lifecycle is indexed in
`research/architecture/README.md`.

When sources disagree, inspect the implementation and tests, identify the conflict, and avoid
silently treating a proposal as shipped behavior. Useful authority routes:

| Topic | Authority |
| --- | --- |
| Firmware coding and architecture | `firmware/AGENTS.md` |
| Product-realization phases and transitions | `docs/PRODUCT_REALIZATION_FRAMEWORK.md` |
| Product definition and launch hypotheses | `research/PRODUCT_DEFINITION.md` |
| Current delivery status | `PROGRAM_STATUS.md` |
| Verification | `docs/TESTING.md` |
| GPIO mappings | `docs/PIN_REFERENCE.md` and active `firmware/domes/main/config.hpp` |
| Host protocol schemas | `firmware/common/proto/*.proto` |
| Flutter app architecture | `ios/domes_app/README.md` and `ios/domes_app/AGENTS.md` |
| Platform and device access | `docs/PLATFORM.md` |

## Verification Contract

After implementation, use the strongest feasible verification for the affected components. Unit
tests alone are insufficient for firmware, protocol, transport, or hardware-facing behavior. Use
`scripts/verify.sh` for a repository-wide software check and `docs/TESTING.md` for the component and
hardware matrix.

Firmware builds must use ESP-IDF v5.4.4 and a fresh build directory with an isolated `SDKCONFIG`.
An ignored `firmware/domes/sdkconfig` can contain stale options and is not release evidence.

Do not claim completion when required builds or tests fail. If hardware, ESP-IDF, BLE, or device
access is unavailable, state exactly what remains unverified. In particular:

- An accepted command is not physical confirmation of LEDs, haptics, touch, sensors, or audio.
- A successful OTA boot is not proof of the separately forced failed-self-test rollback path.
- Multi-device and ESP-NOW behavior requires the required number of physical pods.
- ESP-NOW evidence requires a fresh exact `disabled` lifecycle, complementary roles with one peer
  each, simulation-off benchmarks in both directions, and a separate traced simulation drill;
  `stopping` is transitional, not ready.
- Hardware CI runs through the `hw-test` pull request label or manual workflow dispatch and
  requires selected physical boards plus a compatible runner.

## Host Protocol Contract

New host-facing config and trace messages must use Protocol Buffers. Define them first in
`firmware/common/proto/*.proto`; never hand-roll or duplicate protobuf enums or messages in C++,
Rust, or Dart. Run `tools/generate_protocols.sh` after schema changes because a firmware build does
not refresh all committed generated outputs.

The bounded fixed-binary exceptions are OTA chunk-transfer structs, compact internal trace events,
and internal ESP-NOW peer packets mirrored by the host simulator. Keep every consumer wire
compatible until migration and do not extend those exceptions to a new protocol family.

The shared frame is `[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]`. Most config responses carry
`[Status:u8][protobuf]`; list/diagnostic responses without command status and unsolicited
notifications carry a bare protobuf. Firmware and every host decoder must agree on the envelope.

UART0 framed config and OTA use the NFF DevKit CP2102N bridge (`/dev/ttyUSB*`, preferably its
`/dev/serial/by-id/usb-Silicon_Labs_CP2102N_*` link). Native ESP32-S3 USB Serial/JTAG
(`/dev/ttyACM*`) is for console logs and JTAG; keep logs off UART0 so they cannot corrupt frames.

WiFi/TCP verification requires a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build and stored credentials; the
default build omits that runtime feature and the CLI does not provision a clean board. Raw TCP OTA
and generic trace commands are unsupported. Serial and BLE are the supported CLI image-transfer
paths.

## Documentation Map

| Document | Purpose |
| --- | --- |
| `docs/README.md` | Documentation index |
| `docs/PRODUCT_REALIZATION_FRAMEWORK.md` | Product lifecycle and phase entry/exit rules |
| `docs/TESTING.md` | Software and hardware verification procedures |
| `PROGRAM_STATUS.md` | CEO status, phases, gates, workstreams, hardware releases, and decisions |
| `hardware/NEXT_ITERATION_REQUEST.md` | Current hardware-definition and component-selection work package |
| `research/PRODUCT_DEFINITION.md` | Customer, value, launch, and requirements hypotheses |
| `research/SYSTEM_ARCHITECTURE.md` | Product target, not as-built status |
| `research/architecture/README.md` | Historical/proposed record lifecycle |
| `docs/PLATFORM.md` | Host, BLE, USB, udev, and multi-device setup |
| `docs/FIRMWARE_RUNBOOKS.md` | Firmware hardware verification procedures |
| `docs/DEBUGGING.md` | ESP32-S3 GDB, OpenOCD, and panic-dump procedures |
