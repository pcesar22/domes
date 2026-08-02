# DOMES CLI Guidelines

## Project Overview

This Rust CLI communicates with DOMES firmware over CP2102N-backed UART serial, Bluetooth Low
Energy, and build-gated WiFi TCP.

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

Config and trace protocol definitions come from `firmware/common/proto/*.proto`. The CLI uses
prost-generated types from `build.rs`.

Never hand-roll a new host protocol enum or message struct. The existing OTA transfer structs are a
bounded fixed-binary exception mirrored from `firmware/common/protocol/otaProtocol.hpp` and the
Flutter app's `lib/data/protocol/ota_protocol.dart`; keep all three implementations wire-compatible
until migrated.

Frame format:

```text
[0xAA 0x55][Len:u16le][MsgType:u8][Payload][CRC32:u32le]
```

Most config command responses place a status byte before the protobuf payload:
`[Status:u8][Protobuf response]`. List and diagnostic responses without a command status contain the
protobuf directly, as do unsolicited notifications. Do not strip or invent a status byte without
checking the paired firmware sender.

Message ranges:

| Range | Protocol |
| --- | --- |
| 0x01-0x05 | OTA |
| 0x10-0x1B | Trace |
| 0x20-0x4F | Config, feature, system, and diagnostic command requests/responses (with reserved gaps) |
| 0x50 | Unsolicited device-originated touch notification; bare protobuf, not a request |

## Adding Commands

1. Add or update protobuf messages first unless modifying the existing OTA exception.
2. Run `cargo build --locked` to regenerate prost types.
3. Add the command implementation in `src/commands/`.
4. Add the subcommand in `src/main.rs`.
5. Export it from `src/commands/mod.rs`.
6. Add tests for protocol encoding, command behavior, or parsing when practical.

## Testing

```bash
cargo build --locked
cargo test --locked --all-targets --all-features

# Serial
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
cargo run -- --port "$PORT1" feature list

# WiFi
cargo run -- --wifi 192.168.1.100:5000 feature list

# BLE, native Linux only
cargo run -- --scan-ble
cargo run -- --ble "DOMES-Pod-01" feature list
cargo run -- --ble "DOMES-Pod-01" led solid --color ff0000

# Multi-device
cargo run -- --port "$PORT1" --port "$PORT2" feature list
cargo run -- --all feature list
cargo run -- devices scan  # serial and BLE discovery; no WiFi/mDNS discovery
cargo run -- devices list
```

The default firmware build omits and rejects the WiFi feature. `wifi enable`/`disable` is
operational only in `CONFIG_DOMES_WIFI_AUTO_CONNECT` builds with stored credentials; verify the
actual TCP path with a successful `--wifi HOST:5000` command. `feature list` contains only features
supported by the running build.

## Common Issues

| Issue | Fix |
| --- | --- |
| WiFi connection refused | Verify same network and TCP server on port 5000 |
| WSL2 cannot reach WiFi devices | Use native Linux or the host OS network path |
| Serial permission denied | Check the active `uaccess` ACL and owning group; add the user to `dialout` or `uucp` only when that is the device's actual group |
| Protobuf changes missing | Run `cargo clean && cargo build --locked` after confirming `Cargo.lock` is current |
| BLE no adapter | Use native Linux and enable adapter with `bluetoothctl power on` |
| BLE timeout | Confirm recent firmware, advertising, and try reconnecting |

## Verification Expectations

For CLI-only changes, run `cargo fmt --check`,
`cargo clippy --locked --all-targets --all-features -- -D warnings`, `cargo build --locked`, and
`cargo test --locked --all-targets --all-features`. For protocol or transport changes, also verify
against firmware or a simulator when available. For BLE behavior, do not treat WSL2 results as
valid. The repository-wide matrix lives in `docs/TESTING.md`.

For OTA changes, CLI success is not the end-to-end pass. Reconnect after transfer, verify the
expected version, health, and self-test, reboot again, and repeat. Exercise invalid/interrupted
recovery and forced failed-self-test rollback separately when those paths changed.
