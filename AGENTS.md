# DOMES Project - Codex Instructions

## Verification

After any code implementation, verify the change end to end. Unit tests alone are not sufficient
for firmware, protocol, transport, or hardware-facing behavior.

Use the strongest feasible verification for the files touched, and state clearly when hardware,
ESP-IDF, BLE, or device access prevents a check.

### Level 1: Unit Tests

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

### Level 2: Build Affected Components

Firmware builds must use ESP-IDF v5.4.4, matching CI and `firmware/domes/dependencies.lock`.

```bash
# Firmware-only clean build (run from the repository root)
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
FIRMWARE_BIN="$VERIFY_ROOT/build/domes.bin"
EXPECTED_VERSION=$(
  . ~/esp/esp-idf/export.sh >/dev/null 2>&1
  python -m esptool image_info --version 2 "$FIRMWARE_BIN" |
    sed -n 's/^App version: //p'
)
test -n "$EXPECTED_VERSION"

# Host CLI, when modified
(cd tools/domes-cli && cargo fmt --check)
(cd tools/domes-cli && cargo clippy --locked --all-targets --all-features -- -D warnings)
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)

# Flutter app, when modified
(cd ios/domes_app && flutter pub get --enforce-lockfile)
(cd ios/domes_app && flutter analyze --fatal-infos --fatal-warnings)
(cd ios/domes_app && flutter test)
```

### Level 3: Hardware or End-to-End Verification

For firmware changes, build, flash, and then verify the actual behavior with `domes-cli`,
serial logs, BLE/WiFi/serial transport, or visual confirmation.

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh firmware/domes "$PORT"
```

Feature-specific verification:

| Feature type | Verification method |
| --- | --- |
| Protocol/transport | Flash, run host CLI, verify communication |
| Config/runtime | Flash, use CLI to change settings, verify applied |
| LED/display | Flash, run LED commands, ask for visual confirmation |
| Sensors/input | Flash, trigger input, verify logs or CLI response |
| WiFi transport | `domes-cli --wifi <IP>:5000 feature list` |
| Serial transport | `domes-cli --port <PORT> feature list` |
| BLE transport | `domes-cli --ble "DOMES-Pod-XX" feature list` |
| Multi-device | `domes-cli --all feature list` |
| ESP-NOW | Flash both, enable ESP-NOW, monitor peer discovery |
| OTA updates | Transfer, verify version/health/self-test, reboot again, and repeat; test forced rollback separately |
| Hardware CI | Add the `hw-test` label to the PR after asking the user |

Do not claim a task is complete when build fails, tests fail, firmware does not flash, or required
hardware behavior was not verified. If hardware is unavailable, say exactly what remains unverified.
An accepted peripheral command is not physical confirmation, and a successful OTA boot is not proof
of the forced failed-self-test rollback path.

The WiFi/TCP check requires a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build and stored credentials; the
default build omits that runtime feature, and the CLI does not provision a clean board. Raw TCP OTA
and generic trace commands are unsupported.

For a repository-wide final check, prefer `scripts/verify.sh`. An ignored project-local
`firmware/domes/sdkconfig` can preserve stale options and is not release evidence; final firmware
verification must use a fresh build directory and `SDKCONFIG`, as above or through the verification
and flash helpers.

## Codex Workflows

Use `AGENTS.md` files as Codex-facing project instructions:

- Root guidance lives in this file.
- Firmware-specific rules live in `firmware/AGENTS.md`.
- Hardware sourcing rules live in `hardware/AGENTS.md`.
- CLI rules live in `tools/domes-cli/AGENTS.md`.

Use project-local skills under `.codex/skills/` for reusable workflows:

| Skill | Purpose |
| --- | --- |
| `$domes-esp32-firmware` | Build, flash, monitor, validate, and run hardware test runbooks |
| `$domes-debug-esp32` | ESP32-S3 GDB/OpenOCD debugging workflows |
| `$domes-github-workflow` | DOMES branch, commit, PR, and review standards |

The old Claude slash commands map to Codex runbooks in
`.codex/skills/domes-esp32-firmware/references/runbooks.md`.

## Git Workflow

Before editing, inspect the worktree and avoid overwriting user changes.

For substantial features or fixes started from `main`, prefer a dedicated worktree or branch. Use
`.worktrees/<name>` and `codex/<type>/<description>` for Codex-created worktrees when practical.
Documentation or agent-instruction conversions can stay in the current workspace when requested.

```bash
mkdir -p .worktrees
WORKTREE_NAME=transport-fix
BRANCH=codex/fix/transport-fix
git worktree add ".worktrees/$WORKTREE_NAME" -b "$BRANCH"
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`.

Always ask before creating or publishing a PR. Never use `.claude/worktrees/` for new Codex work.

## Protocol Buffers

New host-facing config and trace messages must use Protocol Buffers.

- Never hand-roll a new host protocol definition.
- Never duplicate protobuf enums or message types in C++, Rust, or Dart.
- All config and trace message definitions come from `firmware/common/proto/*.proto`.
- If creating a message, add it to the relevant `.proto` file first.

Bounded existing exceptions are the OTA chunk-transfer structs, trace recorder's compact internal
event records, and the internal ESP-NOW peer packets mirrored by the host simulator. Keep every
consumer of each exception wire-compatible until migrated; do not extend these exceptions to new
protocol families.

Source of truth:

| File | Purpose |
| --- | --- |
| `firmware/common/proto/config.proto` | Runtime config, feature, system, LED, touch messages |
| `firmware/common/proto/config.options` | nanopb size constraints |
| `firmware/common/proto/trace.proto` | Trace protocol messages |
| `tools/domes-cli/build.rs` | prost generation for the Rust CLI |
| `tools/generate_protocols.sh` | Committed nanopb and Dart generation/checking |

Generation paths:

- Firmware: nanopb generates `*.pb.h` and `*.pb.c`.
- CLI: prost generates Rust types at build time.
- Flutter app: generated Dart types live under `ios/domes_app/lib/data/proto/generated/`.

Run `tools/generate_protocols.sh` after schema changes; a firmware build alone does not regenerate
committed nanopb files.

## Runtime Config Protocol

Binary protocol over UART serial, WiFi TCP, or BLE GATT:

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

OTA occupies message types `0x01-0x05`, trace occupies `0x10-0x1B`, and config command
requests/responses occupy `0x20-0x4F` with reserved gaps. Type `0x50` is the unsolicited,
device-originated touch notification; it is not a request and carries a bare protobuf payload.

Request payloads are protobuf-encoded. Most config responses use a one-byte status envelope before
the response protobuf: `[Status:u8][Protobuf payload]`. List and diagnostic responses that do not
report a command status, plus unsolicited notifications, contain the protobuf directly. The owning
firmware handler and host decoder must agree on the envelope for each message.

The active NFF DevKit routes framed serial config and OTA over UART0 through its CP2102N bridge
(`/dev/ttyUSB*`; prefer `/dev/serial/by-id/usb-Silicon_Labs_CP2102N_*`). Native ESP32-S3 USB
Serial/JTAG is reserved for console logs and JTAG and commonly enumerates as `/dev/ttyACM*`. TCP
config uses port 5000; BLE responses use GATT notifications.

Key files:

| File | Purpose |
| --- | --- |
| `firmware/domes/main/config/configCommandHandler.hpp` | Config command handler |
| `firmware/domes/main/config/featureManager.hpp` | Feature state management |
| `firmware/domes/main/transport/bleOtaService.hpp` | BLE GATT service |
| `tools/domes-cli/src/transport/ble.rs` | CLI BLE transport |
| `tools/domes-cli/src/transport/frame.rs` | CLI frame codec |

## OTA Updates

`domes-cli` supports serial and BLE OTA. Raw WiFi/TCP image transfer is currently not routed by the
firmware TCP config server and must not be presented as verified.

```bash
domes-cli --port "$PORT" ota flash "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
domes-cli --all ota flash "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
```

Use `--all` for OTA only when every selected registry target is serial or BLE. WiFi targets reject
raw image transfer.

The OTA version is part of the integrity contract: it must be parser-valid, at most 31 ASCII bytes,
and byte-for-byte equal to the application version embedded in the selected image. Extract it from
that exact image; do not substitute a release example or an independently typed label.

After serial or BLE OTA, reconnect and verify the expected version, `system health`, and
`system self-test`; reboot once more and repeat those checks. Test invalid/interrupted recovery and
forced failed-self-test rollback as separate failure paths. A normal successful update does not
verify rollback.

Key files:

| File | Purpose |
| --- | --- |
| `firmware/common/protocol/otaProtocol.hpp` | OTA frame payload definitions |
| `tools/domes-cli/src/commands/ota.rs` | Rust CLI OTA implementation |
| `ios/domes_app/lib/data/protocol/ota_protocol.dart` | Flutter OTA implementation |
| `firmware/domes/main/transport/serialOtaReceiver.hpp` | Device-side serial OTA |
| `firmware/domes/main/services/otaManager.hpp` | HTTPS/GitHub OTA service |

## Tracing

Use the firmware trace macros and dump traces through `domes-cli`.

```cpp
#include "trace/traceApi.hpp"

void myFunction() {
    TRACE_SCOPE(TRACE_ID("MyModule.Function"), domes::trace::Category::kGame);
    TRACE_INSTANT(TRACE_ID("MyModule.Event"), domes::trace::Category::kGame);
    TRACE_COUNTER(TRACE_ID("MyModule.Value"), someValue, domes::trace::Category::kGame);
}
```

```bash
domes-cli --port "$PORT" trace start
domes-cli --port "$PORT" system health
domes-cli --port "$PORT" trace stop
domes-cli --port "$PORT" trace dump -o trace.json --names tools/trace/trace_names.json
```

Open the resulting JSON in `https://ui.perfetto.dev`.

For multi-pod inspection, dump one trace per pod and use
`tools/trace/trace_merge.py --align zero` to group each local timeline by its capture start. The
current firmware has no truthful cross-clock synchronization marker. The merge tool supports only
capture-start grouping (`zero`) and unshifted local timestamps (`raw`); neither is timing correlation
between pods.

## Multi-Device Testing

Two NFF DevKit CP2102N bridges usually appear as `/dev/ttyUSB0` and `/dev/ttyUSB1`. Kernel numbers
can change; register the serial-number-based links under `/dev/serial/by-id/`, not `ttyUSB` numbers.
`tools/udev/99-domes-pods.rules` supplies device access policy, not custom identity aliases.

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
domes-cli devices add pod1 serial "$PORT1"
domes-cli devices add pod2 serial "$PORT2"
domes-cli devices list
domes-cli devices scan
domes-cli --all feature list
domes-cli --all led solid --color ff0000
```

For ESP-NOW testing, follow the complete `$domes-esp32-firmware` integration runbook. It requires an
exact `disabled` state before every fresh lifecycle, complementary master/slave roles with one peer
each, a slave-first and master-second benchmark with simulation off, and a separate trace-backed
simulated drill. `stopping` is transitional and is not safe to re-enable. Status from one pod or a
sleep-based check is not communication evidence.

Native USB console monitoring is optional supporting evidence. It does not replace framed CLI
results, benchmark cardinality, or trace assertions. Use the CP2102N `/dev/serial/by-id/` paths for
`domes-cli`, flashing, and serial OTA.

## Platform Requirements

Full details: `.codex/PLATFORM.md`.

- Use native Linux for BLE and multi-device hardware testing.
- Do not use WSL2 for BLE.
- Do not use Raspberry Pi Bluetooth for validation-critical BLE work.
- Recommended BLE adapters: Intel AX200/AX210 or Realtek RTL8761B.

## Gotchas

Initialization order in `main.cpp` matters:

1. WiFi before TCP config server and BLE, for coexistence.
2. BLE OTA service early, because advertising starts automatically.
3. FeatureManager before TCP/Serial/BLE config handlers.
4. TCP config server before the UART config/OTA receiver.
5. UART config/OTA last, after the native USB console is available.

UART0 carries only framed protocol traffic through the NFF CP2102N bridge. Keep ESP-IDF console
output on native USB Serial/JTAG so logs cannot corrupt UART frames. If native USB is not connected,
use BLE diagnostics or attach the second USB connection before relying on boot logs.

Always search the codebase before asking for project facts:

```bash
rg "kWifiSsid|kWifiPassword" firmware/
rg "CONFIG_DOMES" firmware/domes
```

## Documentation Map

| Document | Purpose |
| --- | --- |
| `firmware/AGENTS.md` | Firmware coding standards and architecture rules |
| `firmware/MILESTONES.md` | Development phases and current status |
| `research/SYSTEM_ARCHITECTURE.md` | Product hardware and system target, not as-built status |
| `research/architecture/` | Historical and proposed design records, indexed by lifecycle |
| `docs/PIN_REFERENCE.md` | GPIO pin mappings |
| `tools/domes-cli/AGENTS.md` | CLI guidance |
| `hardware/AGENTS.md` | Hardware component sourcing guidance |
| `.codex/PLATFORM.md` | Platform, BLE, udev, and multi-device setup |
| `.codex/skills/` | Codex skills and reusable runbooks |
