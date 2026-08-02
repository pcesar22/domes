# DOMES CLI

`domes-cli` is the primary service and development interface for DOMES pods. It supports runtime
configuration, diagnostics, tracing, OTA, and multi-device commands over the transports implemented
by each workflow.

The executable's `--help` output owns command syntax. This guide owns setup, transport constraints,
and representative workflows.

## Prerequisites And Build

On Debian or Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y protobuf-compiler pkg-config libudev-dev libdbus-1-dev
```

Build and verify:

```bash
cd tools/domes-cli
cargo fmt --check
cargo clippy --all-targets --all-features
cargo build
cargo test
cargo run -- --help
```

The debug binary is `target/debug/domes-cli`; use `cargo build --release` for
`target/release/domes-cli`.

## Transports

| Transport | Select with | Current workflow support |
| --- | --- | --- |
| USB CDC serial | `--port /dev/ttyACM0` | Config, diagnostics, trace, serial OTA |
| WiFi TCP config server | `--wifi HOST:5000` | Config and diagnostics |
| BLE GATT | `--ble NAME_OR_ADDRESS` | Config, diagnostics, BLE OTA |
| Device registry | `--target NAME` or `--all` | Fan-out across registered serial, TCP, and BLE targets |

Raw image transfer through `--wifi ... ota flash` is not currently supported by the firmware TCP
server. GitHub update-check commands are config messages and are a separate workflow. Trace live
streaming connects to its dedicated WiFi endpoint and accepts one target per process.

BLE validation requires native Linux or a supported mobile host. Do not use WSL2 or Raspberry Pi
Bluetooth for validation-critical results.

## Discover And Inspect

```bash
domes-cli --list-ports
domes-cli --scan-ble
domes-cli --port /dev/ttyACM0 system info
domes-cli --port /dev/ttyACM0 system health
domes-cli --port /dev/ttyACM0 system self-test
domes-cli --port /dev/ttyACM0 feature list
```

`system crash-dump` is a legacy command name. The current firmware returns a clean-restart snapshot
saved before `esp_restart()`; it does not retrieve a panic backtrace or ESP-IDF core dump.

## Control A Pod

```bash
domes-cli --port /dev/ttyACM0 feature enable esp-now
domes-cli --port /dev/ttyACM0 wifi status
domes-cli --port /dev/ttyACM0 led solid --color ff0000 --brightness 128
domes-cli --port /dev/ttyACM0 led breathing --color 0000ff --period 3000
domes-cli --port /dev/ttyACM0 led cycle --period 2000
domes-cli --port /dev/ttyACM0 led off
domes-cli --port /dev/ttyACM0 imu triage --enable
domes-cli --port /dev/ttyACM0 touch simulate --pad 1
```

Use subcommand help for accepted enum values and options:

```bash
domes-cli system --help
domes-cli ota --help
domes-cli trace --help
domes-cli espnow --help
```

## OTA

Build the firmware first, then use a serial or BLE target:

```bash
domes-cli --port /dev/ttyACM0 ota flash \
  firmware/domes/build/domes.bin --version v1.0.0

domes-cli --ble "DOMES-Pod-01" ota flash \
  firmware/domes/build/domes.bin --version v1.0.0
```

After transfer, reconnect and verify `system info` reports the expected version and that the pod
completed a healthy boot. The serial receiver verifies the CLI-provided SHA-256 digest before it
accepts the image and selects the new boot partition.

## Tracing And Sniffing

```bash
domes-cli --port /dev/ttyACM0 trace start
domes-cli --port /dev/ttyACM0 trace status
domes-cli --port /dev/ttyACM0 trace dump \
  --output trace.json --names tools/trace/trace_names.json
domes-cli --port /dev/ttyACM0 trace stop

domes-cli trace stream --wifi 192.168.1.100:5001
domes-cli --port /dev/ttyACM0 sniff --filter config,trace --json
```

Open dump output in [Perfetto](https://ui.perfetto.dev). Merge multiple pod dumps with
`tools/trace/trace_merge.py`; see [`../README.md`](../README.md).

## ESP-NOW

```bash
domes-cli --port /dev/ttyACM0 espnow status
domes-cli --port /dev/ttyACM0 espnow bench --rounds 100
domes-cli --port /dev/ttyACM0 espnow sim-mode on
```

Status on one pod does not prove peer communication. A valid result requires at least two physical
pods, peer discovery, and packet or benchmark evidence.

## Device Registry And Fan-Out

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli devices list
domes-cli devices scan

domes-cli --target pod1 --target pod2 feature list
domes-cli --all led solid --color 00ff00
```

Registry entries are stored in `~/.domes/devices.toml`. Multi-device output is labeled by target.
Long-running trace streaming is intentionally single-target; start one process per pod when multiple
streams are needed.

## Command Groups

| Group | Purpose |
| --- | --- |
| `feature`, `wifi`, `led`, `imu`, `touch` | Runtime feature and peripheral control |
| `system` | Mode, identity, health, restart snapshot, memory, and self-test |
| `ota` | Serial/BLE image transfer and GitHub update configuration |
| `trace` | Record, inspect, dump, and stream performance events |
| `espnow` | Peer status, latency benchmark, and simulation controls |
| `devices` | Persistent registry and discovery |
| `sniff` | Capture and decode framed config, trace, or OTA traffic |

## Protocol Ownership

Config and trace message IDs and payloads come from `firmware/common/proto/*.proto`. `build.rs`
generates Rust prost types whenever the CLI builds. The shared frame is:

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

The OTA chunk-transfer structs are a bounded fixed-binary exception mirrored from
`firmware/common/protocol/otaProtocol.hpp`. Do not add a new handwritten host protocol. See
[`../../firmware/common/proto/README.md`](../../firmware/common/proto/README.md) for cross-language
generation rules.

## Verification Limits

Unit tests cover framing, sniffer decoding, and selected command helpers. They do not replace a
firmware build or device test. Protocol, transport, OTA, BLE, and multi-device changes require the
corresponding end-to-end check in [`../../docs/TESTING.md`](../../docs/TESTING.md).
