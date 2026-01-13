# DOMES CLI

Command-line tool for configuring and updating DOMES firmware devices.

## Features

- **Feature Management** - Enable/disable runtime features (LED effects, BLE, WiFi, etc.)
- **RGB LED Patterns** - Control animated LED patterns (rainbow, comet, fire effects)
- **OTA Updates** - Flash firmware over USB serial or WiFi
- **WiFi Control** - Manage WiFi subsystem state

## Installation

### From Source

```bash
cd tools/domes-cli
cargo build --release

# Binary is at target/release/domes-cli
```

### Requirements

- Rust 1.70+ (install via [rustup](https://rustup.rs/))
- Linux: `libudev-dev` package for serial port access

## Usage

### Connection Options

```bash
# USB Serial (most common)
domes-cli --port /dev/ttyACM0 <command>

# WiFi/TCP (device must be on same network)
domes-cli --wifi 192.168.1.100:5000 <command>

# List available serial ports
domes-cli --list-ports
```

### Feature Management

```bash
# List all features and their states
domes-cli --port /dev/ttyACM0 feature list

# Enable a feature
domes-cli --port /dev/ttyACM0 feature enable led-effects

# Disable a feature
domes-cli --port /dev/ttyACM0 feature disable ble
```

Available features: `led-effects`, `ble`, `wifi`, `esp-now`, `touch`, `haptic`, `audio`

### RGB LED Patterns

```bash
# List available patterns
domes-cli --port /dev/ttyACM0 rgb list

# Check current pattern status
domes-cli --port /dev/ttyACM0 rgb status

# Set patterns with shortcuts
domes-cli --port /dev/ttyACM0 rgb rainbow --speed 50 --brightness 128
domes-cli --port /dev/ttyACM0 rgb comet --color cyan --speed 30
domes-cli --port /dev/ttyACM0 rgb fire --brightness 200

# Set pattern with full control
domes-cli --port /dev/ttyACM0 rgb set solid --color ff0000 --brightness 255

# Turn off LEDs
domes-cli --port /dev/ttyACM0 rgb off
```

**Available Patterns:**

| Pattern | Description |
|---------|-------------|
| `off` | Turn off all LEDs |
| `solid` | Solid color on all LEDs |
| `rainbow-chase` | Rainbow colors rotating around the ring |
| `comet-tail` | Bright dot with fading trail |
| `sparkle-fire` | Random sparkling with warm fire colors |

**Color Options:**

- Named colors: `red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `orange`, `pink`, `white`
- Hex values: `ff0000`, `00ff00`, `#0000ff`

### WiFi Control

```bash
# Check WiFi status
domes-cli --port /dev/ttyACM0 wifi status

# Enable/disable WiFi
domes-cli --port /dev/ttyACM0 wifi enable
domes-cli --port /dev/ttyACM0 wifi disable
```

### OTA Firmware Updates

```bash
# Flash firmware over USB serial
domes-cli --port /dev/ttyACM0 ota flash firmware.bin --version v1.2.3

# Flash firmware over WiFi
domes-cli --wifi 192.168.1.100:5000 ota flash firmware.bin
```

## Protocol

The CLI communicates with the device using a binary frame protocol:

```
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

- **Start bytes**: `0xAA 0x55`
- **Length**: 2 bytes, little-endian (Type + Payload size)
- **Type**: 1 byte message type
- **Payload**: 0-1024 bytes (protobuf-encoded)
- **CRC32**: 4 bytes, little-endian (IEEE 802.3 polynomial)

Message types are defined in `firmware/common/proto/config.proto`.

## Troubleshooting

### Serial Port Permission Denied

Add your user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Device Not Found

1. Check USB cable is data-capable (not charge-only)
2. Verify device appears: `ls /dev/ttyACM*`
3. Try unplugging and replugging

### Command Timeout

1. Ensure firmware is running (not stuck in bootloader)
2. Check baud rate matches (115200)
3. Try resetting the device

### WiFi Connection Failed

1. Ensure device and host are on the same network
2. Check device IP address is correct
3. Verify port 5000 is not blocked by firewall

## Development

### Building

```bash
cargo build          # Debug build
cargo build --release # Release build
cargo test           # Run tests
```

### Adding New Commands

1. Add message types to `firmware/common/proto/config.proto`
2. Regenerate nanopb: `cd firmware/common && python3 ../third_party/nanopb/generator/nanopb_generator.py proto/config.proto`
3. Create command module in `src/commands/`
4. Add serialization in `src/protocol/mod.rs`
5. Register command in `src/main.rs`

### Project Structure

```
src/
├── main.rs           # CLI entry point, argument parsing
├── commands/         # Command implementations
│   ├── feature.rs    # Feature toggle commands
│   ├── rgb.rs        # RGB LED pattern commands
│   ├── wifi.rs       # WiFi control commands
│   └── ota.rs        # OTA update commands
├── transport/        # Communication layer
│   ├── frame.rs      # Frame encoding/decoding
│   ├── serial.rs     # USB serial transport
│   └── tcp.rs        # TCP/WiFi transport
├── protocol/         # Protocol serialization
│   └── mod.rs        # Message types, protobuf helpers
├── proto.rs          # Protobuf type extensions
└── build.rs          # Prost code generation
```

## License

See repository root for license information.
