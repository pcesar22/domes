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
- ESP-IDF v5.4.4 with the ESP32-S3 tools installed; this must match CI for reproducible builds
- CMake and a C++20 host compiler
- Rust 1.92.0 and Cargo, matching CLI CI
- Flutter 3.44.8 with Dart `protoc_plugin` 25.0.0 for app and binding checks
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

Confirm the active environment before building:

```bash
. ~/esp/esp-idf/export.sh
idf.py --version  # ESP-IDF v5.4.4
```

```bash
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
```

The checked-in partition table currently targets the 8 MB development layout. A successful build
must fit the smallest `0x1E0000` OTA app partition in `firmware/domes/partitions.csv`.
An ignored project-local `firmware/domes/sdkconfig` can override changed defaults; use the isolated
command above or `scripts/verify.sh` for final evidence.

## Run Host Tests

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

Use `ctest -N` for the current discovered count. See [`docs/TESTING.md`](docs/TESTING.md) for suite
scope and when host tests are insufficient.

## Build The CLI

```bash
(cd tools/domes-cli && cargo fmt --check)
(cd tools/domes-cli && cargo clippy --locked --all-targets --all-features -- -D warnings)
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)
(cd tools/domes-cli && cargo run -- --help)
```

The CLI is the supported host interface for serial, TCP, BLE, OTA, tracing, diagnostics, and
multi-device commands. Do not recreate its protocol in one-off Python scripts.

## Verify A Connected Pod

List attached serial devices:

```bash
tools/domes-cli/target/debug/domes-cli --list-ports
find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
```

The NFF DevKit CP2102N interface (`/dev/ttyUSB*`) is used for flashing, framed UART commands, and
serial OTA. Native ESP32-S3 USB Serial/JTAG (`/dev/ttyACM*`) is a separate console/JTAG connection.
Use `/dev/serial/by-id/` for persistent targets because kernel numbers can change.

Build, flash, and verify the framed runtime UART:

```bash
tools/firmware/flash_and_verify.sh \
  firmware/domes "$PORT"
```

The helper verifies the exact built firmware version through `domes-cli system info`, then requires
`system health` and the complete `system self-test` to pass. Attach native USB separately and use
`tools/firmware/monitor_serial.py` when console boot logs are required.

Exercise the runtime protocol:

```bash
tools/domes-cli/target/debug/domes-cli --port "$PORT" system self-test
tools/domes-cli/target/debug/domes-cli --port "$PORT" system info
tools/domes-cli/target/debug/domes-cli --port "$PORT" feature list
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
| Transports | `firmware/domes/main/transport/` | UART, TCP, BLE, and ESP-NOW transport paths |
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
documented exception. Keep the C++ firmware, Rust CLI, and Dart app implementations compatible
until it is migrated.

Most config command responses place `[Status:u8]` before the response protobuf. List and diagnostic
responses that do not return a command status contain only the protobuf. Check the paired handler and
decoder before changing either envelope.

## Hardware Configuration

The sole supported NFF board profile and compiled pin mapping live directly in
`firmware/domes/main/config.hpp`. [`docs/PIN_REFERENCE.md`](docs/PIN_REFERENCE.md) explains that
mapping and planned production values; the schematic remains authoritative for physical nets.
There is no board selector or production profile. The checked-in partition table is the 8 MB NFF
development layout.

Do not copy pin tables into new documents.

## Multi-Pod Workflow

The sorted port order is not a firmware pod ID. Query `system info` and choose registry names or pod
IDs deliberately when boards already contain identity.

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
CLI=tools/domes-cli/target/debug/domes-cli
$CLI devices add pod1 serial "$PORT1"
$CLI devices add pod2 serial "$PORT2"
$CLI devices list
$CLI --target pod1 --target pod2 feature list
```

Multi-device ESP-NOW validation needs at least two pods. Serial-number device links and BLE platform
requirements are documented in [`.codex/PLATFORM.md`](.codex/PLATFORM.md). Follow the complete
[`$domes-esp32-firmware` ESP-NOW runbook](.codex/skills/domes-esp32-firmware/references/runbooks.md#esp-now-integration-test);
single-status or sleep-based checks are not sufficient evidence.

## Before Submitting A Change

- Inspect the worktree and keep unrelated edits out of the change.
- Update an authoritative source instead of adding another duplicate description.
- Run the checks required by [`docs/TESTING.md`](docs/TESTING.md).
- Search tracked documentation for commands, pins, status claims, or names that changed.
- Update `firmware/MILESTONES.md` only when verification justifies a status change.
- State hardware, BLE, or multi-device checks that could not be run.
