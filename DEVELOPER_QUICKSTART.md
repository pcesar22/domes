# Developer Quickstart

This guide takes a new contributor from clone to a locally verified DOMES change. Project status,
architecture, and testing each have a separate owner; links below are intentional.

## Read First

1. [`docs/README.md`](docs/README.md): documentation ownership and navigation.
2. [`firmware/MILESTONES.md`](firmware/MILESTONES.md): implemented and hardware-verified status.
3. [`firmware/AGENTS.md`](firmware/AGENTS.md): firmware coding and runtime constraints.
4. [`docs/TESTING.md`](docs/TESTING.md): required verification by change type.
5. [`research/SOFTWARE_ARCHITECTURE.md`](research/SOFTWARE_ARCHITECTURE.md): software boundaries.

Read [`research/SYSTEM_ARCHITECTURE.md`](research/SYSTEM_ARCHITECTURE.md) and the detailed design
references only when the task needs that context.

## Prerequisites

- Native Linux for BLE and validation-critical multi-device work
- ESP-IDF v5.x with the ESP32-S3 tools installed
- CMake and a C++20 host compiler
- Rust stable and Cargo
- Python 3; install `pyserial` for serial helper scripts
- Protobuf compiler, pkg-config, libudev, and D-Bus development headers for the CLI

On Debian or Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake protobuf-compiler pkg-config libudev-dev libdbus-1-dev python3-pip
python3 -m pip install --user pyserial
```

Initialize the repository:

```bash
git clone https://github.com/pcesar22/domes.git
cd domes
git submodule update --init --recursive
```

## Build Firmware

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
```

The checked-in partition table currently targets the 8 MB development layout. A successful build
must fit the smallest `0x1E0000` OTA app partition in `firmware/domes/partitions.csv`.

## Run Host Tests

```bash
cd firmware/test_app
mkdir -p build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Use `ctest -N` for the current discovered count. See [`docs/TESTING.md`](docs/TESTING.md) for suite
scope and when host tests are insufficient.

## Build The CLI

```bash
cd tools/domes-cli
cargo fmt --check
cargo clippy --all-targets --all-features
cargo build
cargo test
cargo run -- --help
```

The CLI is the supported host interface for serial, TCP, BLE, OTA, tracing, diagnostics, and
multi-device commands. Do not recreate its protocol in one-off Python scripts.

## Verify A Connected Pod

List attached serial devices:

```bash
tools/domes-cli/target/debug/domes-cli --list-ports
```

Build, flash, and check boot output:

```bash
. ~/esp/esp-idf/export.sh
tools/firmware/flash_and_verify.sh \
  firmware/domes /dev/ttyACM0 "DOMES"
```

Exercise the runtime protocol:

```bash
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 system self-test
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 system info
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 feature list
```

No device result may be inferred from a successful host build. Record unavailable hardware and the
specific behavior that remains unverified.

## Current Software Boundaries

| Layer | Location | Responsibility |
| --- | --- | --- |
| Schemas | `firmware/common/proto/` | Config and trace message definitions |
| Shared wire support | `firmware/common/protocol/` | Frame codec and legacy OTA transfer codec |
| Firmware entry/composition | `firmware/domes/main/main.cpp` | Initialization and dependency wiring |
| Drivers/interfaces | `firmware/domes/main/{drivers,interfaces}/` | Hardware access and test seams |
| Services/state | `firmware/domes/main/{services,config,game}/` | Pod behavior and state machines |
| Transports | `firmware/domes/main/transport/` | USB, TCP, BLE, and ESP-NOW transport paths |
| CLI | `tools/domes-cli/src/` | Host transports, commands, and multi-device fan-out |
| Host simulation | `firmware/test_app/sim/` | Deterministic pod and drill simulation |

## Protocol Rules

Config and trace messages originate in `firmware/common/proto/*.proto` and are generated for
nanopb, prost, and Dart consumers. Add new messages to the relevant schema first; never duplicate a
generated enum in application code.

The serial/TCP/BLE frame is:

```text
[0xAA][0x55][length little-endian][type][protobuf payload][CRC32 little-endian]
```

`length` covers the type byte plus payload, and CRC32 covers the same bytes. The existing OTA data
transfer codec under `firmware/common/protocol/otaProtocol.hpp` predates the protobuf rule and is a
documented exception. Keep the firmware and Rust implementations compatible until it is migrated.

## Hardware Configuration

The active board and compiled pin mapping live in `firmware/domes/main/config.hpp`. The current
default is `BOARD_NFF_DEVBOARD`. [`docs/PIN_REFERENCE.md`](docs/PIN_REFERENCE.md) explains the active
mapping and planned production values; the schematic remains authoritative for physical nets.

Do not copy pin tables into new documents.

## Multi-Pod Workflow

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli devices list
domes-cli --target pod1 --target pod2 feature list
domes-cli --all feature enable esp-now
domes-cli --all espnow status
```

Multi-device ESP-NOW validation needs at least two pods. Stable Linux symlinks and BLE platform
requirements are documented in [`.codex/PLATFORM.md`](.codex/PLATFORM.md).

## Before Submitting A Change

- Inspect the worktree and keep unrelated edits out of the change.
- Update an authoritative source instead of adding another duplicate description.
- Run the checks required by [`docs/TESTING.md`](docs/TESTING.md).
- Search tracked documentation for commands, pins, status claims, or names that changed.
- Update `firmware/MILESTONES.md` only when verification justifies a status change.
- State hardware, BLE, or multi-device checks that could not be run.
