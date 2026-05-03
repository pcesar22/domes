# DOMES CLI Guidelines

## Project Overview

This Rust CLI communicates with DOMES firmware over USB serial, WiFi TCP, and Bluetooth Low Energy.

## Architecture

```text
src/
  main.rs           CLI argument parsing with clap
  device.rs         Multi-device registry and targeting
  proto.rs          Generated protobuf module wiring
  protocol/         Frame encoding and protobuf helpers
  transport/        Transport trait plus serial, TCP, BLE, frame codec
  commands/         Feature, WiFi, LED, OTA, trace, system, touch, IMU commands
```

## Protocol Rules

All protocol definitions come from `firmware/common/proto/*.proto`. The CLI uses prost-generated
types from `build.rs`.

Never hand-roll protocol enums or message structs.

Frame format:

```text
[0xAA 0x55][Len:u16le][MsgType:u8][Payload][CRC32:u32le]
```

Message ranges:

| Range | Protocol |
| --- | --- |
| 0x01-0x05 | OTA |
| 0x10-0x17 | Trace |
| 0x20-0x29 | Config and feature commands |

## Adding Commands

1. Add or update protobuf messages first.
2. Run `cargo build` to regenerate prost types.
3. Add the command implementation in `src/commands/`.
4. Add the subcommand in `src/main.rs`.
5. Export it from `src/commands/mod.rs`.
6. Add tests for protocol encoding, command behavior, or parsing when practical.

## Testing

```bash
cargo build
cargo test

# Serial
cargo run -- --port /dev/ttyACM0 feature list

# WiFi
cargo run -- --wifi 192.168.1.100:5000 feature list

# BLE, native Linux only
cargo run -- --scan-ble
cargo run -- --ble "DOMES-Pod" feature list
cargo run -- --ble "DOMES-Pod" led solid --color ff0000

# Multi-device
cargo run -- --port /dev/ttyACM0 --port /dev/ttyACM1 feature list
cargo run -- --all feature list
cargo run -- devices scan
cargo run -- devices list
```

## Common Issues

| Issue | Fix |
| --- | --- |
| WiFi connection refused | Verify same network and TCP server on port 5000 |
| WSL2 cannot reach WiFi devices | Use native Linux or the host OS network path |
| Serial permission denied | Add user to `dialout`, then log out and back in |
| Protobuf changes missing | Run `cargo clean && cargo build` |
| BLE no adapter | Use native Linux and enable adapter with `bluetoothctl power on` |
| BLE timeout | Confirm recent firmware, advertising, and try reconnecting |

## Verification Expectations

For CLI-only changes, run `cargo test`. For protocol or transport changes, also verify against
firmware or simulator when available. For BLE behavior, do not treat WSL2 results as valid.
