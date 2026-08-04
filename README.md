# DOMES

**Distributed Open-source Motion & Exercise System**

[![Software CI](https://github.com/pcesar22/domes/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/pcesar22/domes/actions/workflows/firmware-ci.yml)

DOMES is an ESP32-S3 reaction-training pod system. A pod combines an addressable LED ring,
capacitive touch, an accelerometer, audio, and haptic hardware. Pods coordinate over ESP-NOW;
development and configuration use UART serial, BLE, or optional WiFi/TCP through `domes-cli`.

## Project Status

The current development platform is the NFF carrier board with an ESP32-S3 DevKit. Firmware,
serial/BLE configuration, optional WiFi/TCP configuration, serial/BLE OTA, tracing, touch, IMU, the
game state machine, and two-pod ESP-NOW drills are implemented. The haptic driver is configured for
the target LRA, but haptic/audio hardware verification remains open. The integrated production PCB
remains planned.

The 2026-08-02 two-board review exercised erased-board factory and normal programming, repeated UART
and BLE commands, device-registry fan-out, serial and BLE OTA success/recovery, forced rollback,
CRC- and ELF-bound restart snapshots, repeated lossless ESP-NOW lifecycles, and a trace-backed
two-pod drill. Physical LED, touch, IMU, audio, and haptic observations remain separate release
checks; command acceptance and initialization tests are not physical proof. The dated evidence and
remaining boundaries are maintained in the integrated program ledger rather than duplicated here.

See [`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](docs/PRODUCT_REALIZATION_FRAMEWORK.md) for the path from
development boards through EVT, DVT, PVT, and open product release. See
[`PROGRAM_STATUS.md`](PROGRAM_STATUS.md) for the active phase, delivery evidence, and next
gate. That file owns status; architecture documents describe design and must not be used as
completion claims.

The repository does not currently contain a license file. Public visibility alone does not define
reuse or contribution terms; license selection remains a project-owner decision.

## Quick Start

Prerequisites:

- ESP-IDF v5.4.4 (the CI and dependency-lock version)
- CMake and a C++20 host compiler
- Rust 1.92.0 for the CLI, matching CI
- Python 3 with `pyserial` for serial helper scripts
- `protobuf-compiler`, `pkg-config`, `libudev-dev`, and `libdbus-1-dev` for the CLI

Clone and initialize dependencies:

```bash
git clone https://github.com/pcesar22/domes.git
cd domes
git submodule update --init --recursive
```

Build firmware:

```bash
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
FIRMWARE_BIN="$VERIFY_ROOT/build/domes.bin"
EXPECTED_VERSION=$(
  . ~/esp/esp-idf/export.sh >/dev/null 2>&1
  python -m esptool image_info --version 2 "$FIRMWARE_BIN" |
    sed -n 's/^App version: //p'
)
test -n "$EXPECTED_VERSION"
```

Use `scripts/verify.sh` for the complete final check. A pre-existing ignored
`firmware/domes/sdkconfig` may contain stale options and is not release evidence.

Run host firmware tests:

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

Build and test the CLI:

```bash
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)
```

The complete verification matrix, including hardware expectations, is in
[`docs/TESTING.md`](docs/TESTING.md).

Every pull request runs the aggregate Software CI workflow. Its `CI Gate` covers firmware, host
tests, CLI, host tooling, protocol drift, and the reusable Flutter checks. Repository rules must
name `CI Gate` as a required check before GitHub will enforce it as a merge condition.

## CLI Examples

```bash
# Select the first serial-number-stable NFF CP2102N port
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
CLI=tools/domes-cli/target/debug/domes-cli

# Discover and inspect
$CLI --list-ports
$CLI --scan-ble
$CLI --port "$PORT" system info
$CLI --port "$PORT" feature list

# LED patterns
$CLI --port "$PORT" led solid --color ff0000
$CLI --port "$PORT" led breathing --color 0000ff --period 3000
$CLI --port "$PORT" led cycle --period 2000
$CLI --port "$PORT" led off

# Other transports
$CLI --wifi 192.168.1.100:5000 feature list
$CLI --ble "DOMES-Pod-01" feature list

# OTA and tracing
$CLI --port "$PORT" ota flash "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
$CLI --port "$PORT" trace start
$CLI --port "$PORT" system health
$CLI --port "$PORT" trace stop
$CLI --port "$PORT" trace dump -o trace.json \
  --names tools/trace/trace_names.json
```

The WiFi example requires a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build and stored credentials. The
default profile omits the WiFi runtime feature; enabled development builds prefer stored credentials
and use compile-time secrets only to seed an unprovisioned first boot. The CLI does not provision a
clean board, and raw TCP OTA is not supported.

Multi-device operations use repeated transport flags or the registry:

The sorted USB serial-number order below is discovery order, not a firmware pod ID.

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
CLI=tools/domes-cli/target/debug/domes-cli
$CLI devices add pod1 serial "$PORT1"
$CLI devices add pod2 serial "$PORT2"
$CLI --target pod1 --target pod2 feature list
$CLI --all led solid --color 00ff00
```

See [`tools/domes-cli/README.md`](tools/domes-cli/README.md) for the current command surface. The
executable's `--help` output is authoritative for syntax.

## Architecture At A Glance

```text
Phone / host tools
        |
        | BLE, optional WiFi/TCP, or CP2102N-backed UART serial
        v
Config and trace framing + protobuf payloads
        |
        v
Per-pod services and game state machine
        |
        +---- ESP-NOW ---- peer pods
        |
        v
LED, touch, IMU, haptic, and audio drivers
```

Current ownership boundaries:

| Area | Source |
| --- | --- |
| Firmware application | [`firmware/domes/main/`](firmware/domes/main/) |
| Shared framing and OTA codec | [`firmware/common/protocol/`](firmware/common/protocol/) |
| Config and trace schemas | [`firmware/common/proto/`](firmware/common/proto/) |
| Host CLI | [`tools/domes-cli/`](tools/domes-cli/) |
| Flutter application | [`ios/domes_app/`](ios/domes_app/) |
| Host simulations and tests | [`firmware/test_app/`](firmware/test_app/) |
| Current hardware mapping | [`firmware/domes/main/config.hpp`](firmware/domes/main/config.hpp) |
| Hardware design files | [`hardware/`](hardware/) |

Config and trace payloads are protobuf-encoded. OTA transfer messages and the internal ESP-NOW peer
protocol are bounded fixed-binary exceptions; mirrored definitions must remain wire-compatible until
they are migrated.

Config command request/response message types occupy `0x20-0x4F` with reserved gaps. Type `0x50`
is the unsolicited device-originated touch notification used by the Flutter BLE drill path; it is
not a command request and carries a bare protobuf payload.

On the NFF DevKit, CP2102N `/dev/ttyUSB*` is the flash/config/serial-OTA interface and native USB
`/dev/ttyACM*` is the separate console/JTAG interface. Prefer `/dev/serial/by-id/` links for persistent
CLI targets. Most config responses carry `[Status:u8][Protobuf payload]`; list and diagnostic
responses without a command status, plus unsolicited notifications, carry the protobuf directly.

## Repository Layout

```text
firmware/
  common/           Shared schemas, framing, OTA codec, and utilities
  domes/            ESP-IDF application
  test_app/         GoogleTest host suite and multi-pod simulation
hardware/           Board design, BOM, and bring-up material
ios/domes_app/      Flutter application prototype
tools/domes-cli/    Rust device CLI
tools/              CLI, protocol generation, verification, evaluation, and trace tooling
docs/               Current documentation map, testing, and pin reference
research/           System/software decisions and design references
```

## Documentation

Start with [`docs/README.md`](docs/README.md). It defines document ownership and the reading order.

| Document | Purpose |
| --- | --- |
| [`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](docs/PRODUCT_REALIZATION_FRAMEWORK.md) | Product lifecycle, phase entry/exit, and status reporting |
| [`DEVELOPER_QUICKSTART.md`](DEVELOPER_QUICKSTART.md) | First local build and change workflow |
| [`docs/TESTING.md`](docs/TESTING.md) | Verification matrix and CI/hardware expectations |
| [`PROGRAM_STATUS.md`](PROGRAM_STATUS.md) | CEO status, phases, gates, workstreams, hardware releases, evidence, and decisions |
| [`hardware/NEXT_ITERATION_REQUEST.md`](hardware/NEXT_ITERATION_REQUEST.md) | Current product-hardware definition and component-selection request |
| [`research/PRODUCT_DEFINITION.md`](research/PRODUCT_DEFINITION.md) | Customer, product, and launch hypotheses |
| [`docs/PIN_REFERENCE.md`](docs/PIN_REFERENCE.md) | Compiled and planned GPIO mappings |
| [`research/SOFTWARE_ARCHITECTURE.md`](research/SOFTWARE_ARCHITECTURE.md) | Software boundaries and decisions |
| [`research/SYSTEM_ARCHITECTURE.md`](research/SYSTEM_ARCHITECTURE.md) | Product hardware and network targets |
| [`research/architecture/README.md`](research/architecture/README.md) | Detailed design-document lifecycle |

AI-specific operating instructions live in `AGENTS.md` and the scoped `AGENTS.md` files. They defer
project facts to the same sources listed above.
