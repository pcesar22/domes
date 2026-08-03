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
(cd tools/domes-cli && cargo fmt --check)
(cd tools/domes-cli && cargo clippy --locked --all-targets --all-features -- -D warnings)
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)
(cd tools/domes-cli && cargo run -- --help)
```

The debug binary is `target/debug/domes-cli`; use `cargo build --locked --release` for
`target/release/domes-cli`.

The examples below run from the repository root:

```bash
CLI=tools/domes-cli/target/debug/domes-cli
```

## Transports

| Transport | Select with | Current workflow support |
| --- | --- | --- |
| CP2102N-backed UART | `--port /dev/serial/by-id/...` | Config, diagnostics, trace, serial OTA |
| Build-gated WiFi TCP config server | `--wifi HOST:5000` | Config and diagnostics in `CONFIG_DOMES_WIFI_AUTO_CONNECT` builds |
| BLE GATT | `--ble NAME_OR_ADDRESS` | Config, diagnostics, BLE OTA |
| Device registry | `--target NAME` or `--all` | Fan-out across registered serial, TCP, and BLE targets |

`--port`, `--wifi`, `--ble`, and `--target` are repeatable and may be combined for explicit fan-out.
`--all` is exclusive and rejects those selectors instead of contacting the same physical endpoint
through an accidental mixture. `sniff` requires exactly one `--port`; `trace stream` accepts its one
host through `trace stream --wifi HOST` and rejects every global transport or multi-device selector.

Raw image transfer and trace control through a generic `--wifi ...` target are not routed by the
firmware TCP config server. GitHub update-check commands are config messages and are a separate
workflow. `trace stream --wifi HOST:5001` connects to the dedicated trace endpoint and accepts one
target per process.

The default firmware build omits the WiFi runtime feature and rejects attempts to set it. A
`CONFIG_DOMES_WIFI_AUTO_CONNECT` build exposes the feature, prefers stored credentials, and uses
compile-time secrets only to seed an unprovisioned first boot. On a build without that capability,
the CLI rejects WiFi mutations, update checks, and auto-update enable before sending an unsupported
command; auto-update disable remains available as deterministic cleanup. Automatic GitHub/WiFi
update is not a verified release path; configuring that service is not transfer and reboot
validation.

On the NFF DevKit, `/dev/ttyUSB*` is the CP2102N flash/config/OTA interface and `/dev/ttyACM*` is the
separate native USB console/JTAG interface. Kernel numbers can change; use a CP2102N
`/dev/serial/by-id/` link for registry entries and persistent commands.

BLE validation requires native Linux or a supported mobile host. Do not use WSL2 or Raspberry Pi
Bluetooth for validation-critical results.

## Discover And Inspect

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
$CLI --list-ports
$CLI --scan-ble
$CLI --port "$PORT" system info
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test
$CLI --port "$PORT" feature list
```

`system crash-dump` is a legacy command name. The current firmware returns a clean-restart snapshot
saved before `esp_restart()`; it does not retrieve a panic backtrace or ESP-IDF core dump. Current
format-2 records are CRC-protected and identify the pre-restart firmware and exact ELF SHA-256 along
with boot count, internal heap, and processed PCs. Legacy format-0 values are labeled as having
unverified semantics. Corrupt or unsupported records fail closed; use `system crash-dump --clear`
to explicitly remove an unreadable record.

Large BLE diagnostics are fragmented by current firmware and reassembled by the current CLI. A
large response timeout against older firmware/CLI combinations is a compatibility failure, not a
successful partial diagnostic.

## Control A Pod

```bash
$CLI --port "$PORT" feature enable esp-now
$CLI --port "$PORT" wifi status
$CLI --port "$PORT" led solid --color ff0000 --brightness 128
$CLI --port "$PORT" led breathing --color 0000ff --period 3000
$CLI --port "$PORT" led cycle --period 2000
$CLI --port "$PORT" led off
$CLI --port "$PORT" imu triage --enable
$CLI --port "$PORT" touch simulate --pad 1
```

On a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build, `wifi enable` and `wifi disable` connect and disconnect
the station client with stored credentials, while `wifi status` reports its supported feature
state. The default build omits that feature and the CLI reports that the capability is absent. These
commands do not provision credentials or prove the TCP server is reachable; use a successful
`--wifi HOST:5000` command to verify the transport.

`feature list` contains only features supported by the running build. Its LED, touch, audio,
haptic, ESP-NOW, and BLE-advertising entries are enforced by the corresponding runtime paths; an
accepted state change is still not physical proof of the peripheral output.

Use subcommand help for accepted enum values and options:

```bash
$CLI system --help
$CLI ota --help
$CLI trace --help
$CLI espnow --help
```

## OTA

Build a retained application with a fresh configuration, then use the exact resulting image with a
serial or BLE target:

```bash
OTA_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$OTA_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$OTA_ROOT/sdkconfig" build)
FIRMWARE_BIN="$OTA_ROOT/build/domes.bin"
EXPECTED_VERSION=$(
  . ~/esp/esp-idf/export.sh >/dev/null 2>&1
  python -m esptool image_info --version 2 "$FIRMWARE_BIN" |
    sed -n 's/^App version: //p'
)
test -n "$EXPECTED_VERSION"

$CLI --port "$PORT" ota flash \
  "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"

$CLI --ble "DOMES-Pod-01" ota flash \
  "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
```

The CLI requires a parser-valid ASCII version of at most 31 bytes, and the receiver verifies that it
is byte-for-byte identical to the version embedded in the uploaded ESP image. Do not replace the
extracted value with an example or a manually maintained release label.

After transfer, reconnect and verify `system info` reports the expected version and that
`system health` plus `system self-test` pass. Reboot once more by power cycling or resetting through
the programming port, then repeat those checks; the second boot confirms that the new image was
accepted rather than merely booted while pending rollback verification. The common serial/BLE
receiver verifies the CLI-provided SHA-256 digest and embedded application version before it accepts
the image and selects the new boot partition.

A successful update does not verify forced rollback. That path needs a purpose-built image or fault
injection that fails the post-OTA self-test, followed by evidence that the previous version booted.

## Tracing And Sniffing

```bash
$CLI --port "$PORT" trace start
$CLI --port "$PORT" trace status
$CLI --port "$PORT" system health
$CLI --port "$PORT" trace stop
$CLI --port "$PORT" trace dump \
  --output trace.json --names tools/trace/trace_names.json

$CLI trace stream --wifi 192.168.1.100:5001
$CLI --port "$PORT" sniff --filter config,trace --json
```

Open dump output in [Perfetto](https://ui.perfetto.dev). The sniffer is a passive serial reader: it
does not proxy commands and cannot share an exclusively opened UART with a command-producing CLI
process. Use it only with traffic mirrored or produced on the port it opens.

Trace merge is a separate Python tool, not a CLI subcommand. Export one file per pod, then run:

```bash
python3 tools/trace/trace_merge.py \
  --pod /tmp/pod1.json --pod-name pod1 \
  --pod /tmp/pod2.json --pod-name pod2 \
  --names tools/trace/trace_names.json \
  --align zero \
  --output /tmp/domes-merged.json
```

Zero alignment groups each pod's local trace by its capture start; it does not correlate pod clocks.
The only other mode, `raw`, retains original local timestamps. Neither mode creates cross-pod timing
or synchronization evidence.

## ESP-NOW

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
wait_for_disabled() {
  for attempt in {1..20}; do
    status1=$($CLI --port "$PORT1" espnow status)
    status2=$($CLI --port "$PORT2" espnow status)
    if grep -Eq 'State:[[:space:]]+disabled' <<< "$status1" &&
       grep -Eq 'State:[[:space:]]+disabled' <<< "$status2"; then
      return 0
    fi
    sleep 1
  done
  printf '%s\n%s\n' "$status1" "$status2"
  return 1
}
$CLI --port "$PORT1" feature disable esp-now
$CLI --port "$PORT2" feature disable esp-now
wait_for_disabled
$CLI --port "$PORT1" espnow sim-mode off
$CLI --port "$PORT2" espnow sim-mode off
$CLI --port "$PORT1" feature enable esp-now
$CLI --port "$PORT2" feature enable esp-now
peers_ready=false
for attempt in {1..30}; do
  status1=$($CLI --port "$PORT1" espnow status)
  status2=$($CLI --port "$PORT2" espnow status)
  state1=$(awk '/State:/ {print $2; exit}' <<< "$status1")
  state2=$(awk '/State:/ {print $2; exit}' <<< "$status2")
  if [[ "$state1:$state2" == "master:slave" ||
        "$state1:$state2" == "slave:master" ]] &&
     grep -Eq 'Peers:[[:space:]]+1' <<< "$status1" &&
     grep -Eq 'Peers:[[:space:]]+1' <<< "$status2"; then
    peers_ready=true
    break
  fi
  sleep 1
done
if [[ "$peers_ready" != true ]]; then
  printf '%s\n%s\n' "$status1" "$status2"
  exit 1
fi
if [[ "$state1:$state2" == "master:slave" ]]; then
  MASTER_PORT=$PORT1
  SLAVE_PORT=$PORT2
elif [[ "$state1:$state2" == "slave:master" ]]; then
  MASTER_PORT=$PORT2
  SLAVE_PORT=$PORT1
else
  printf '%s\n%s\n' "$status1" "$status2"
  exit 1
fi
$CLI --port "$SLAVE_PORT" espnow bench --rounds 100
$CLI --port "$MASTER_PORT" espnow bench --rounds 100
$CLI --port "$PORT1" feature disable esp-now
$CLI --port "$PORT2" feature disable esp-now
wait_for_disabled
```

Status on one pod does not prove peer communication. A valid result requires at least two physical
pods, complementary roles, one peer on each, and packet or benchmark evidence. Derive `SLAVE_PORT`
and `MASTER_PORT` from status and benchmark the slave first immediately after roles become visible;
this exercises the master's startup receive path. Run trace-backed simulated drills in a fresh
lifecycle after both benchmark pods report `disabled`; do not enable simulation during the latency
benchmark. `stopping` means the old lifecycle is still unwinding and must not be treated as ready for
another enable cycle.

## Device Registry And Fan-Out

Sorted USB serial numbers do not encode firmware pod IDs. Query `system info` and assign registry
names deliberately for boards that already contain identity.

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
$CLI devices add pod1 serial "$PORT1"
$CLI devices add pod2 serial "$PORT2"
$CLI devices list
$CLI devices scan

$CLI --target pod1 --target pod2 feature list
$CLI --all led solid --color 00ff00
```

Registry entries are stored in `~/.domes/devices.toml`. Multi-device output is labeled by target.
Long-running trace streaming is intentionally single-target; start one process per pod when multiple
streams are needed.

The registry rejects a second name for the same normalized serial, BLE, or TCP address. Prefer
stable serial paths or BLE MACs for destructive fan-out: a BLE advertising name is not assumed to
be identical to that device's MAC. Re-adding the same registry name updates that entry.

`devices scan` reports local serial ports and BLE advertisements. It does not discover WiFi/mDNS
targets; add TCP targets explicitly with `devices add`.

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

OTA occupies message types `0x01-0x05`, trace occupies `0x10-0x1B`, and config command
requests/responses occupy `0x20-0x4F` with reserved gaps. `0x50` is the unsolicited,
device-originated touch notification and is not a command request.

The OTA chunk-transfer structs are a bounded fixed-binary exception mirrored from
`firmware/common/protocol/otaProtocol.hpp`; the Flutter app has a third implementation in
`ios/domes_app/lib/data/protocol/ota_protocol.dart`. Keep the C++, Rust, and Dart implementations
wire-compatible, and do not add a new handwritten host protocol. See
[`../../firmware/common/proto/README.md`](../../firmware/common/proto/README.md) for cross-language
generation rules.

Most config command responses wrap the response protobuf as `[Status:u8][Protobuf payload]`. List
and diagnostic responses without command status, plus the unsolicited touch notification, contain
the protobuf directly. This per-message envelope must match the firmware sender.

## Verification Limits

Automated tests cover framing and strict response decoding, selector preflight, registry identity,
sniffer output, OTA failure handling, trace integrity, multi-target JSON, and process exit status.
They do not replace a firmware build or device test. Protocol, transport, OTA, BLE, and multi-device
changes require the corresponding end-to-end check in
[`../../docs/TESTING.md`](../../docs/TESTING.md).
