# DOMES CLI

Command-line interface for DOMES firmware runtime configuration and updates.

## Installation

```bash
cd tools/domes-cli
cargo build --release
```

The binary will be at `target/release/domes-cli`.

## Usage

### Connection Options

```bash
# Serial (USB)
domes-cli --port /dev/ttyACM0 <command>

# WiFi (TCP)
domes-cli --wifi 192.168.1.100:5000 <command>

# List available serial ports
domes-cli --list-ports
```

### Feature Management

```bash
# List all features and their status
domes-cli --port /dev/ttyACM0 feature list

# Enable a feature
domes-cli --port /dev/ttyACM0 feature enable wifi

# Disable a feature
domes-cli --port /dev/ttyACM0 feature disable ble
```

Available features: `led-effects`, `ble`, `wifi`, `esp-now`, `touch`, `haptic`, `audio`

### WiFi Control

```bash
# Show WiFi status
domes-cli --port /dev/ttyACM0 wifi status

# Enable WiFi
domes-cli --port /dev/ttyACM0 wifi enable

# Disable WiFi
domes-cli --port /dev/ttyACM0 wifi disable
```

### OTA Firmware Updates

```bash
# Flash firmware over serial
domes-cli --port /dev/ttyACM0 ota flash firmware.bin --version v1.2.3

# Flash firmware over WiFi
domes-cli --wifi 192.168.1.100:5000 ota flash firmware.bin
```

### Performance Tracing

```bash
# Start trace recording
domes-cli --port /dev/ttyACM0 trace start

# Stop trace recording
domes-cli --port /dev/ttyACM0 trace stop

# Get trace status
domes-cli --port /dev/ttyACM0 trace status

# Clear trace buffer
domes-cli --port /dev/ttyACM0 trace clear

# Dump traces to JSON file (Perfetto compatible)
domes-cli --port /dev/ttyACM0 trace dump -o trace.json
```

Open the trace file in [Perfetto UI](https://ui.perfetto.dev) for visualization.

## Protocol

The CLI communicates using a binary frame protocol:

```
[0xAA][0x55][LenLE16][MsgType][Payload][CRC32LE]
```

Message payloads use Protocol Buffers (prost) for serialization, matching the firmware's nanopb encoding.

## Development

```bash
# Build
cargo build

# Run tests
cargo test

# Check formatting
cargo fmt --check

# Lint
cargo clippy
```
