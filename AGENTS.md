# DOMES Project - Codex Instructions

## Verification

After any code implementation, verify the change end to end. Unit tests alone are not sufficient
for firmware, protocol, transport, or hardware-facing behavior.

Use the strongest feasible verification for the files touched, and state clearly when hardware,
ESP-IDF, BLE, or device access prevents a check.

### Level 1: Unit Tests

```bash
cd firmware/test_app
mkdir -p build
cd build
cmake ..
make
ctest --output-on-failure
```

### Level 2: Build Affected Components

```bash
# Firmware
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build

# Host CLI, when modified
cd tools/domes-cli
cargo build
cargo test
```

### Level 3: Hardware or End-to-End Verification

For firmware changes, build, flash, and then verify the actual behavior with `domes-cli`,
serial logs, BLE/WiFi/serial transport, or visual confirmation.

```bash
. ~/esp/esp-idf/export.sh
.codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh firmware/domes /dev/ttyACM0 "DOMES"
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
| OTA updates | Flash, run `domes-cli ota flash`, verify reboot |
| Hardware CI | Add the `hw-test` label to the PR after asking the user |

Do not claim a task is complete when build fails, tests fail, firmware does not flash, or required
hardware behavior was not verified. If hardware is unavailable, say exactly what remains unverified.

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
git worktree add .worktrees/<name> -b codex/<type>/<description>
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`.

Always ask before creating or publishing a PR. Never use `.claude/worktrees/` for new Codex work.

## Protocol Buffers

All message serialization between firmware, CLI, and app layers must use Protocol Buffers.

- Never hand-roll protocol definitions.
- Never duplicate enums or message types in C++, Rust, or Dart.
- All protocol definitions come from `firmware/common/proto/*.proto`.
- If creating a message, add it to the relevant `.proto` file first.

Source of truth:

| File | Purpose |
| --- | --- |
| `firmware/common/proto/config.proto` | Runtime config, feature, system, LED, touch messages |
| `firmware/common/proto/config.options` | nanopb size constraints |
| `firmware/common/proto/trace.proto` | Trace protocol messages |
| `tools/domes-cli/build.rs` | prost generation for the Rust CLI |
| `ios/domes_app/scripts/generate_proto.sh` | Dart protobuf generation |

Generation paths:

- Firmware: nanopb generates `*.pb.h` and `*.pb.c`.
- CLI: prost generates Rust types at build time.
- Flutter app: generated Dart types live under `ios/domes_app/lib/data/proto/generated/`.

## Runtime Config Protocol

Binary protocol over USB serial, WiFi TCP, or BLE GATT:

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

Payloads are protobuf-encoded. Transports are USB-CDC serial, TCP port 5000, and BLE GATT
notifications.

Key files:

| File | Purpose |
| --- | --- |
| `firmware/domes/main/config/configCommandHandler.hpp` | Config command handler |
| `firmware/domes/main/config/featureManager.hpp` | Feature state management |
| `firmware/domes/main/transport/bleOtaService.hpp` | BLE GATT service |
| `tools/domes-cli/src/transport/ble.rs` | CLI BLE transport |
| `tools/domes-cli/src/transport/frame.rs` | CLI frame codec |

## OTA Updates

`domes-cli` is the supported OTA path for serial, WiFi/TCP, BLE, and multi-device updates.

```bash
domes-cli --port /dev/ttyACM0 ota flash firmware/domes/build/domes.bin --version v1.0.0
domes-cli --wifi 192.168.1.100:5000 ota flash firmware/domes/build/domes.bin
domes-cli --all ota flash firmware/domes/build/domes.bin --version v1.0.0
```

Key files:

| File | Purpose |
| --- | --- |
| `tools/domes-cli/src/commands/ota.rs` | CLI OTA implementation |
| `firmware/common/protocol/otaProtocol.hpp` | OTA frame payload definitions |
| `firmware/domes/main/transport/serialOtaReceiver.hpp` | Device-side serial OTA |
| `firmware/domes/main/services/otaManager.hpp` | HTTPS/GitHub OTA service |

## Tracing

Use the firmware trace macros and dump traces through `domes-cli`.

```cpp
#include "trace/traceApi.hpp"

void myFunction() {
    TRACE_SCOPE(TRACE_ID("MyModule.Function"), domes::trace::TraceCategory::kGame);
    TRACE_INSTANT(TRACE_ID("MyModule.Event"), domes::trace::TraceCategory::kGame);
    TRACE_COUNTER(TRACE_ID("MyModule.Value"), someValue, domes::trace::TraceCategory::kGame);
}
```

```bash
domes-cli --port /dev/ttyACM0 trace dump -o trace.json --names tools/trace/trace_names.json
```

Open the resulting JSON in `https://ui.perfetto.dev`.

## Multi-Device Testing

Two NFF devboards usually appear as `/dev/ttyACM0` and `/dev/ttyACM1`. Stable symlinks are defined
in `tools/udev/99-domes-pods.rules`.

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli devices list
domes-cli devices scan
domes-cli --all feature list
domes-cli --all led solid --color ff0000
```

For ESP-NOW testing:

```bash
domes-cli --all feature enable esp-now
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 30
```

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
4. TCP config server before Serial OTA, so logs are visible.
5. Serial OTA last, because it can take over USB-CDC.

After `initSerialOta()`, USB-CDC becomes the OTA/config transport and serial logs may stop or
interleave with binary protocol data. Debug init issues before serial OTA init or temporarily delay
that setup.

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
| `research/SYSTEM_ARCHITECTURE.md` | Hardware architecture and specs |
| `research/architecture/` | Deep design docs |
| `docs/PIN_REFERENCE.md` | GPIO pin mappings |
| `tools/domes-cli/AGENTS.md` | CLI guidance |
| `hardware/AGENTS.md` | Hardware component sourcing guidance |
| `.codex/PLATFORM.md` | Platform, BLE, udev, and multi-device setup |
| `.codex/skills/` | Codex skills and reusable runbooks |
