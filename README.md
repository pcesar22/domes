# DOMES

**Distributed Open-source Motion & Exercise System**

[![Build Status](https://github.com/pcesar22/domes/actions/workflows/firmware.yml/badge.svg)](https://github.com/pcesar22/domes/actions/workflows/firmware.yml)
[![CLI Build](https://github.com/pcesar22/domes/actions/workflows/cli.yml/badge.svg)](https://github.com/pcesar22/domes/actions/workflows/cli.yml)

A wireless reaction training pod system for athletic training, physical therapy, and fitness. Each pod features RGB LEDs, audio, haptic feedback, and touch sensing, communicating via ESP-NOW for sub-2ms latency synchronized drills.

```
    ┌─────────┐     ESP-NOW      ┌─────────┐
    │  Pod 1  │◄────(< 2ms)─────►│  Pod 2  │
    │  LED    │                  │  LED    │
    │  Touch  │                  │  Touch  │
    └────┬────┘                  └────┬────┘
         │                            │
         │ BLE                        │ BLE
         ▼                            ▼
    ┌─────────────────────────────────────┐
    │          Phone / Controller         │
    └─────────────────────────────────────┘
```

## Features

| Feature | Status | Description |
|---------|--------|-------------|
| LED Effects | Done | 16x RGBW LED ring with patterns and animations |
| Touch Sensing | Done | Capacitive touch pads for user input |
| BLE Communication | Done | Bluetooth Low Energy for phone/host connectivity |
| ESP-NOW Sync | Done | Sub-2ms latency pod-to-pod communication |
| WiFi Transport | Done | TCP-based config and OTA over WiFi |
| OTA Updates | Done | Over-the-air firmware updates via serial, WiFi, or BLE |
| Haptic Feedback | Planned | LRA motor for tactile response |
| Audio Output | Planned | I2S speaker for audio cues |
| Game Engine | Planned | Drill patterns and scoring |

## Quick Start

### Prerequisites

- [ESP-IDF v5.2+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- [Rust](https://rustup.rs/) (for the CLI tool)
- Python 3.8+

### Build & Flash Firmware

```bash
git clone https://github.com/pcesar22/domes.git
cd domes

# Initialize submodules
git submodule update --init --recursive

# Set up ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Build and flash
cd firmware/domes
idf.py set-target esp32s3
idf.py build flash monitor
```

### Install the CLI Tool

```bash
cd tools/domes-cli
cargo install --path .

# Or run directly
cargo build --release
./target/release/domes-cli --help
```

## CLI Usage

The `domes-cli` tool communicates with pods over USB serial, WiFi, or BLE.

```bash
# List available features
domes-cli --port /dev/ttyACM0 feature list

# Enable/disable features
domes-cli --port /dev/ttyACM0 feature enable led-effects
domes-cli --port /dev/ttyACM0 feature disable wifi

# LED control
domes-cli --port /dev/ttyACM0 led set --color "#FF0000"
domes-cli --port /dev/ttyACM0 led pattern rainbow

# WiFi transport
domes-cli --wifi 192.168.1.100:5000 feature list

# BLE transport (Linux only)
domes-cli --scan-ble                           # Discover devices
domes-cli --ble "DOMES-Pod-01" feature list    # Connect by name

# OTA firmware update
domes-cli --port /dev/ttyACM0 ota flash firmware.bin --version v1.0.0
```

### Multi-Pod Operations

```bash
# Scan and discover all connected pods
domes-cli devices scan

# Register devices
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1

# Commands across multiple devices
domes-cli --all feature list
domes-cli --target pod1 --target pod2 led solid --color ff0000

# Set unique pod IDs (persisted to NVS)
domes-cli --port /dev/ttyACM0 system set-pod-id 1
domes-cli --port /dev/ttyACM1 system set-pod-id 2

# BLE: auto-connect to all DOMES pods
domes-cli --connect-all-ble feature list

# OTA all devices
domes-cli --all ota flash firmware.bin --version v1.0.0
```

## Hardware Platforms

| Platform | Description | Use Case |
|----------|-------------|----------|
| [ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html) | Off-the-shelf dev board | Basic firmware development |
| [NFF Development Board](hardware/nff-devboard/) | Custom board with LED ring, IMU, haptics | Full feature development |
| Production PCB | Final form-factor (planned) | Enclosed pod units |

## Project Structure

```
domes/
├── firmware/
│   ├── domes/                 # Main firmware application
│   │   └── main/
│   │       ├── drivers/       # LED, touch, audio drivers
│   │       ├── services/      # WiFi, OTA, LED service
│   │       ├── transport/     # USB-CDC, TCP, BLE transports
│   │       ├── config/        # Feature manager, command handler
│   │       └── trace/         # Performance tracing
│   ├── common/                # Shared code
│   │   └── proto/             # Protocol Buffer definitions
│   └── test_app/              # Unit tests (Google Test)
├── hardware/
│   └── nff-devboard/          # Development board files
├── tools/
│   ├── domes-cli/             # Rust CLI tool
│   └── trace/                 # Trace visualization tools
├── docs/                      # Additional documentation
└── research/                  # Architecture docs
```

## Documentation

| Document | Description |
|----------|-------------|
| [Developer Quickstart](DEVELOPER_QUICKSTART.md) | Get productive in 30 minutes |
| [Firmware Guidelines](firmware/GUIDELINES.md) | Coding standards and memory rules |
| [Pin Reference](docs/PIN_REFERENCE.md) | Hardware pin mappings |
| [Tools README](tools/README.md) | CLI and test script usage |
| [System Architecture](research/SYSTEM_ARCHITECTURE.md) | Hardware design decisions |

## Architecture

### Communication Protocol

All host-to-device communication uses a framed binary protocol with Protocol Buffer payloads:

```
Frame: [0xAA][0x55][Length:2][Type:1][Payload:N][CRC32:4]
```

Protocol definitions are in `firmware/common/proto/config.proto`.

### Transport Layer

| Transport | Port/Interface | Use Case |
|-----------|----------------|----------|
| USB-CDC | /dev/ttyACM0 | Development, wired OTA |
| TCP | Port 5000 | WiFi-based config/OTA |
| BLE GATT | Custom service | Phone connectivity, wireless OTA |

## Performance Tracing

The firmware includes a lightweight tracing framework for profiling:

```bash
# Dump traces from device
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json

# Visualize at https://ui.perfetto.dev
```

## Development

### Run Unit Tests

```bash
cd firmware/test_app
mkdir -p build && cd build
cmake .. && make -j$(nproc)
ctest --output-on-failure
```

### Code Style

- C++20 for firmware (ESP-IDF v5.x)
- Rust for CLI tool
- Pre-commit hooks for formatting (`.pre-commit-config.yaml`)

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Run tests and ensure firmware builds
4. Submit a pull request

See [Developer Quickstart](DEVELOPER_QUICKSTART.md) for detailed guidelines.

## License

[MIT License](LICENSE) (pending)

---

Built with [ESP-IDF](https://github.com/espressif/esp-idf) and [Claude Code](https://claude.com/claude-code)
