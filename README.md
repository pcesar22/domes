# DOMES

**Distributed Open-source Motion & Exercise System**

[![Firmware CI](https://github.com/pcesar22/domes/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/pcesar22/domes/actions/workflows/firmware-ci.yml)
[![Flutter CI](https://github.com/pcesar22/domes/actions/workflows/flutter-ci.yml/badge.svg)](https://github.com/pcesar22/domes/actions/workflows/flutter-ci.yml)

DOMES is an ESP32-S3 reaction-training pod system. A pod combines an addressable LED ring,
capacitive touch, an accelerometer, audio, and haptic hardware. Pods coordinate over ESP-NOW;
development and configuration use USB serial, WiFi/TCP, or BLE through `domes-cli`.

## Project Status

The current development platform is the NFF carrier board with an ESP32-S3 DevKit. Firmware,
serial/TCP/BLE configuration, serial/BLE OTA, tracing, touch, IMU, the game state machine, and
two-pod ESP-NOW drills are implemented. The haptic driver is configured for the target LRA, but
haptic/audio hardware verification remains open. The integrated production PCB remains planned.

See [`firmware/MILESTONES.md`](firmware/MILESTONES.md) for delivery evidence and remaining work.
That file owns status; architecture documents describe design and must not be used as completion
claims.

The repository does not currently contain a license file. Public visibility alone does not define
reuse or contribution terms; license selection remains a project-owner decision.

## Quick Start

Prerequisites:

- ESP-IDF v5.x
- CMake and a C++20 host compiler
- Rust for the CLI
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
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
```

Run host firmware tests:

```bash
cd firmware/test_app
mkdir -p build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Build and test the CLI:

```bash
cd tools/domes-cli
cargo build
cargo test
```

The complete verification matrix, including hardware expectations, is in
[`docs/TESTING.md`](docs/TESTING.md).

## CLI Examples

```bash
# Discover and inspect
domes-cli --list-ports
domes-cli --scan-ble
domes-cli --port /dev/ttyACM0 system info
domes-cli --port /dev/ttyACM0 feature list

# LED patterns
domes-cli --port /dev/ttyACM0 led solid --color ff0000
domes-cli --port /dev/ttyACM0 led breathing --color 0000ff --period 3000
domes-cli --port /dev/ttyACM0 led cycle --period 2000
domes-cli --port /dev/ttyACM0 led off

# Other transports
domes-cli --wifi 192.168.1.100:5000 feature list
domes-cli --ble "DOMES-Pod-01" feature list

# OTA and tracing
domes-cli --port /dev/ttyACM0 ota flash firmware/domes/build/domes.bin --version v1.0.0
domes-cli --port /dev/ttyACM0 trace dump -o trace.json \
  --names tools/trace/trace_names.json
```

Multi-device operations use repeated transport flags or the registry:

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli --target pod1 --target pod2 feature list
domes-cli --all led solid --color 00ff00
```

See [`tools/domes-cli/README.md`](tools/domes-cli/README.md) for the current command surface. The
executable's `--help` output is authoritative for syntax.

## Architecture At A Glance

```text
Phone / host tools
        |
        | BLE, WiFi/TCP, or USB serial
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

## Repository Layout

```text
firmware/
  common/           Shared schemas, framing, OTA codec, and utilities
  domes/            ESP-IDF application
  test_app/         GoogleTest host suite and multi-pod simulation
hardware/           Board design, BOM, and bring-up material
ios/domes_app/      Flutter application prototype
tools/domes-cli/    Rust device CLI
tools/trace/        Multi-device trace utilities and name mapping
docs/               Current documentation map, testing, and pin reference
research/           System/software decisions and design references
```

## Documentation

Start with [`docs/README.md`](docs/README.md). It defines document ownership and the reading order.

| Document | Purpose |
| --- | --- |
| [`DEVELOPER_QUICKSTART.md`](DEVELOPER_QUICKSTART.md) | First local build and change workflow |
| [`docs/TESTING.md`](docs/TESTING.md) | Verification matrix and CI/hardware expectations |
| [`firmware/MILESTONES.md`](firmware/MILESTONES.md) | Implemented, verified, and pending work |
| [`docs/PIN_REFERENCE.md`](docs/PIN_REFERENCE.md) | Compiled and planned GPIO mappings |
| [`research/SOFTWARE_ARCHITECTURE.md`](research/SOFTWARE_ARCHITECTURE.md) | Software boundaries and decisions |
| [`research/SYSTEM_ARCHITECTURE.md`](research/SYSTEM_ARCHITECTURE.md) | Hardware and network system design |
| [`research/architecture/README.md`](research/architecture/README.md) | Detailed design-document lifecycle |

AI-specific operating instructions live in `AGENTS.md` and the scoped `AGENTS.md` files. They defer
project facts to the same sources listed above.
