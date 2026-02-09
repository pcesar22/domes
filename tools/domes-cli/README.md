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

# Bluetooth Low Energy (requires native Linux, not WSL2)
domes-cli --ble "DOMES-Pod" <command>        # Connect by device name
domes-cli --ble "94:A9:90:0A:EA:52" <command> # Connect by MAC address

# Discovery
domes-cli --list-ports                        # List serial ports
domes-cli --scan-ble                          # Scan for BLE devices
```

## Multi-Device Usage

### Device Registry

```bash
# Register devices
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1

# List registered devices
domes-cli devices list

# Remove a device
domes-cli devices remove pod1

# Scan for all connected DOMES devices
domes-cli devices scan
```

### Targeting Multiple Devices

```bash
# By registry name
domes-cli --target pod1 --target pod2 feature list

# All registered devices
domes-cli --all feature list

# Multiple ports directly
domes-cli --port /dev/ttyACM0 --port /dev/ttyACM1 led solid --color ff0000

# Mix transports
domes-cli --port /dev/ttyACM0 --wifi 192.168.1.100:5000 system info
```

### Multi-Device Output

When targeting multiple devices, output is labeled per device:

```
--- pod1 ---
[pod1] Features:
[pod1] NAME             STATUS
[pod1] led-effects      enabled
[pod1] wifi             disabled

--- pod2 ---
[pod2] Features:
[pod2] NAME             STATUS
[pod2] led-effects      enabled
[pod2] wifi             enabled
```

### Multi-Device OTA

```bash
# Flash all registered devices
domes-cli --all ota flash firmware/domes/build/domes.bin --version v1.0.0
```

### Device Registry File

Devices are stored in `~/.domes/devices.toml`:

```toml
[devices.pod1]
transport = "serial"
address = "/dev/ttyACM0"

[devices.pod2]
transport = "serial"
address = "/dev/ttyACM1"
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

### LED Pattern Control

```bash
# Get current LED pattern
domes-cli --port /dev/ttyACM0 led get

# Turn LEDs off
domes-cli --port /dev/ttyACM0 led off

# Solid color (hex RGB)
domes-cli --port /dev/ttyACM0 led solid --color ff0000        # Red
domes-cli --port /dev/ttyACM0 led solid --color 00ff00        # Green

# Breathing effect
domes-cli --port /dev/ttyACM0 led breathing --color 0000ff --period 3000

# Color cycle (rainbow)
domes-cli --port /dev/ttyACM0 led cycle --period 2000

# Set brightness (0-255)
domes-cli --port /dev/ttyACM0 led solid --color ffffff --brightness 128
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

The CLI communicates using a binary frame protocol over serial, TCP, or BLE:

```
[0xAA][0x55][LenLE16][MsgType][Payload][CRC32LE]
```

Message payloads use Protocol Buffers (prost) for serialization, matching the firmware's nanopb encoding.

### BLE Transport

- **Service UUID**: `12345678-1234-5678-1234-56789abcdef0`
- **Data Characteristic** (Write): `...def1` - Send frames to device
- **Status Characteristic** (Notify): `...def2` - Receive responses

The CLI uses [btleplug](https://github.com/deviceplug/btleplug) for cross-platform BLE support. BLE requires native Linux (not WSL2).

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
