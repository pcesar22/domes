# Testing And Verification

This document owns the repository verification matrix. Component READMEs may link here, but should
not maintain separate, conflicting test requirements.

## Aggregate Local Check

Initialize submodules and install the toolchains used by the aggregate check: ESP-IDF v5.4.4, a
C++20 compiler and CMake, Rust 1.92.0/Cargo, Flutter 3.44.8/Dart, Python 3, `protoc`, Dart
`protoc_plugin` 25.0.0, Go, and ShellCheck.

```bash
git submodule update --init --recursive
dart pub global activate protoc_plugin 25.0.0
python3 -m pip install --user pyserial
python3 -m pip install --user pre-commit==4.6.1
```

On Debian/Ubuntu the CLI also needs `pkg-config`, `libudev-dev`, and `libdbus-1-dev`. Ensure
`$HOME/.pub-cache/bin` is on `PATH` before checking generated Dart bindings.

Run the repository verification entry point before publishing a broad change:

```bash
scripts/verify.sh
```

It checks generated bindings, host firmware tests, CLI format/lint/build/tests, host tooling, the
Flutter app, and the ESP-IDF firmware build. `scripts/verify.sh --quick` skips only the ESP-IDF
build; use it for iteration, not final firmware verification.

When the pinned IDF is installed outside the default path, set
`IDF_EXPORT_SCRIPT=/path/to/esp-idf/export.sh` for `scripts/verify.sh` and
`tools/firmware/flash_and_verify.sh`.

The coding-agent evaluation harness is under `tools/agent_eval/`. Its unit tests run as host tooling
in the aggregate check. Live model evaluations are opt-in because they consume model usage; follow
its README, retain model and effort metadata, and compare one variable at a time.

## Verification Matrix

| Change type | Required checks | Hardware expectation |
| --- | --- | --- |
| Documentation only | `python3 tools/docs/check_markdown_links.py` and relevant command syntax | None unless instructions changed a hardware workflow |
| Host firmware logic | Host unit tests | None when behavior is fully simulated |
| Firmware build/config | Host unit tests and ESP-IDF build | Flash when runtime behavior can change |
| Protocol or transport | Host tests, firmware build, CLI tests | Verify at least one real transport |
| CLI-only behavior | Format, strict Clippy, locked build, and all-target/all-feature tests | Verify against firmware when commands or transport behavior change |
| Flutter application | Locked dependency restore, fatal analysis, tests, and no-codesign iOS build | Verify BLE and device workflows on a supported host and physical pod |
| Driver, sensor, LED, audio, or haptic | Host tests and firmware build | Flash and exercise the affected peripheral |
| Multi-pod or ESP-NOW | Host simulation and all builds | Two-pod discovery and command/drill verification |
| OTA success path | Firmware, CLI, and Flutter protocol tests/builds | Transfer, expected-version boot, health/self-test, second reboot, and expected-version confirmation |
| OTA failure and rollback paths | Abort/digest tests and firmware build | Invalid-image rejection, interrupted-session recovery, and a separately forced failed-self-test rollback |

If required hardware is unavailable, record exactly which device-facing behavior remains
unverified. A successful build is not a hardware pass.

## Host Firmware Tests

The host suite uses GoogleTest and CTest; it does not use Unity or CMock.

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

Use `ctest -N` for the current discovered count. The suite covers frame and OTA codecs, protobuf
messages, feature and mode management, game behavior, multi-pod simulation, and drill/Perfetto
export. Dated verification snapshots belong in `firmware/MILESTONES.md`.

## Firmware Build

The repository-wide preferred path is `scripts/verify.sh`, which creates an isolated build directory
and fresh `SDKCONFIG`. For a firmware-only check while retaining the output path for inspection:

```bash
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
echo "Firmware output: $VERIFY_ROOT/build"
```

The supported and reproducible firmware toolchain is ESP-IDF v5.4.4, matching the CI container and
component dependency lock. Local validation must record `idf.py --version`; another 5.x release is
not equivalent evidence. A build must fit the smallest app partition defined in
[`firmware/domes/partitions.csv`](../firmware/domes/partitions.csv).

Do not use an existing ignored `firmware/domes/sdkconfig` as release evidence. It can override
changed defaults even when the source diff is correct. Software CI, release CI, the hardware
workflow, `scripts/verify.sh`, and `tools/firmware/flash_and_verify.sh` use isolated SDKCONFIG files.

## CLI Checks

```bash
(cd tools/domes-cli && cargo fmt --check)
(cd tools/domes-cli && cargo clippy --locked --all-targets --all-features -- -D warnings)
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo build --locked --release)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)
```

The debug binary is `tools/domes-cli/target/debug/domes-cli`; interactive examples and the local
flash helper use that path. The release command independently verifies optimized compilation and
produces `tools/domes-cli/target/release/domes-cli` for CI or an explicitly release-mode workflow.
Do not build one profile and then invoke the other accidentally. Use `cargo run -- --help` and
subcommand `--help` output to validate documented command syntax.

## Protocol Changes

For changes under `firmware/common/proto/` or shared framing:

1. Update the `.proto` source first for config or trace messages.
2. Run the repository protocol-generation command documented in `firmware/common/proto/README.md`;
   an ordinary firmware build only compiles the committed nanopb output.
3. Build and test `tools/domes-cli` to regenerate and compile prost output.
4. Regenerate Flutter protobuf output when the app consumes the changed schema and confirm the
   generated files have no unexplained diff.
5. Run the host frame/protobuf tests.
6. Verify request/response behavior over serial, TCP, or BLE on a device.

The OTA transfer protocol is a current legacy exception implemented in
[`firmware/common/protocol/otaProtocol.hpp`](../firmware/common/protocol/otaProtocol.hpp),
[`tools/domes-cli/src/commands/ota.rs`](../tools/domes-cli/src/commands/ota.rs), and
[`ios/domes_app/lib/data/protocol/ota_protocol.dart`](../ios/domes_app/lib/data/protocol/ota_protocol.dart).
Do not add another hand-written protocol family; keep all three implementations and their Rust/Dart
compatibility tests wire-compatible until OTA is migrated to protobuf. The internal ESP-NOW peer
protocol is a second bounded exception, mirrored by the host simulator. It is not a host config
transport contract.

## Flutter Checks

```bash
(cd ios/domes_app && flutter pub get --enforce-lockfile)
(cd ios/domes_app && flutter analyze --fatal-infos --fatal-warnings)
(cd ios/domes_app && flutter test)
# On native Linux with the Flutter desktop build prerequisites:
(cd ios/domes_app && flutter build linux --release)
# On macOS with Xcode:
(cd ios/domes_app && flutter build ios --release --no-codesign)
```

These checks do not validate Bluetooth permissions, discovery, connection lifecycle, OTA, or a
physical pod. A device drill pass must demonstrate that a physical touch notification completes only
the currently active pod's round, an inactive pod touch is ignored, timeout remains functional, and
stop/disconnect does not advance a stale round. Run those workflows on native Linux or a supported
mobile target with real hardware.

## Hardware Verification

Single-device firmware flash and framed-runtime check:

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh \
  firmware/domes "$PORT"
```

On the NFF DevKit, the CP2102N bridge (`/dev/ttyUSB*`, preferably its `/dev/serial/by-id/` link)
carries flashing, framed UART commands, and serial OTA. Native ESP32-S3 USB Serial/JTAG
(`/dev/ttyACM*`) is a separate console/JTAG interface. The helper verifies `system info` over the
framed UART after flashing; attach native USB separately when console logs are required.

Then use `domes-cli` for the affected behavior. Examples:

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --port "$PORT" system self-test
$CLI --port "$PORT" feature list
$CLI --port "$PORT" led solid --color ff0000
$CLI --port "$PORT" espnow status
```

LED behavior needs visual confirmation. Touch, IMU, haptic, and audio need the corresponding
physical stimulus or output confirmation. Multi-pod and ESP-NOW behavior needs at least two pods.

For serial or BLE OTA, a successful upload is only the first step. Record all of the following:

1. The declared version was extracted from the exact image, was parser-valid and at most 31 ASCII
   bytes, and the CLI completed the transfer without an abort or device error.
2. After the automatic reboot, both the runtime transport and `system info` returned; the reported
   version matched the image and `system health` plus `system self-test` passed.
3. After one additional explicit reboot, the same version, health, and transport checks passed. This
   confirms the new image was accepted rather than merely booted once while pending verification.
4. Invalid-image rejection and an interrupted transfer left the device responsive to a subsequent
   command or update.

The normal success path does not prove rollback. Forced failed-self-test rollback requires a
purpose-built failing image or fault injection, then evidence that the bootloader selected the
previous image. Record it as unverified unless that destructive path was deliberately exercised.

For multi-pod trace inspection, capture the pods during the same ESP-NOW session and merge with
`--align zero`. This groups local timelines by capture start and does not correlate pod clocks.
`--align raw` preserves each file's local timestamps. Those are the only supported alignment modes;
neither creates synchronized timing evidence.

Flash coredumps and clean-restart snapshots are separate diagnostics. The active profile reserves a
`coredump` partition and enables ESP-IDF ELF dumps; retrieve and decode those with the exact matching
`domes.elf`. `domes-cli system crash-dump` reads only the NVS clean-restart snapshot.

## Continuous Integration

| Workflow | Purpose | Trigger scope |
| --- | --- | --- |
| [`firmware-ci.yml`](../.github/workflows/firmware-ci.yml) | Aggregate Software CI: ESP-IDF build/package validation, host tests, CLI checks, host tooling, protocol drift, and Flutter checks, exposed through `CI Gate` | Every pull request, merge queue entry, and push to `main` |
| [`flutter-ci.yml`](../.github/workflows/flutter-ci.yml) | Reusable generated-binding, analysis, Flutter test, and no-codesign iOS release-build jobs called by Software CI | `workflow_call` only |
| [`firmware-hw-test.yml`](../.github/workflows/firmware-hw-test.yml) | Self-hosted device checks | `hw-test` label and subsequent synchronize/reopen events while labeled, or manual run |
| [`firmware-release.yml`](../.github/workflows/firmware-release.yml) | Tag validation, the complete reusable Software CI gate, then OTA app and merged factory images with checksums | SemVer release tags on `main` |

Release tags must be parser-valid SemVer and at most 31 bytes so the exact tag fits the ESP-IDF
application descriptor and OTA wire field. Release metadata names the OTA application
`domes-<tag>.bin`; the merged factory image is a separate artifact.

Software/release CI uses isolated build directories and fresh SDKCONFIG files. Hardware CI builds
separate normal, versioned-OTA, and purpose-built failed-self-test images, also with isolated
SDKCONFIG files, and asserts that `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is active before it performs
the destructive rollback sequence. The failure image is test-only and must never be published as a
release artifact.

Ask before applying the `hw-test` pull-request label because it consumes attached lab hardware.
Manual hardware dispatch requires at least two devices and accepts a comma-separated `ports` input;
use CP2102N `/dev/serial/by-id/` paths on a runner with stable device identities. The workflow does
not provision the machine: an online Linux x64 self-hosted runner, Actions Runner 2.327.1 or newer,
ESP-IDF v5.4.4, Rust 1.92.0, native BLE, and two attached NFF pods are prerequisites. Do not apply
the label when no qualifying runner is online; the job will remain queued and provides no evidence.

The release OTA image is not a blank-device installer. Use the separately published merged factory
image, or `idf.py flash` from a matching checkout, for initial programming.
