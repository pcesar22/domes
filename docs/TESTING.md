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
its README, retain model and effort metadata, and compare one repository or model variable at a
time.

## Verification Matrix

| Change type | Required checks | Hardware expectation |
| --- | --- | --- |
| Documentation only | `python3 tools/docs/check_markdown_links.py` and relevant command syntax | None unless instructions changed a hardware workflow |
| Host firmware logic | Host unit tests | None when behavior is fully simulated |
| Firmware build/config | Host unit tests and ESP-IDF build | Flash when runtime behavior can change |
| Protocol or transport | Host tests, firmware build, CLI tests | Verify every affected real transport; for a shared-only contract change, verify at least one representative real transport |
| CLI-only behavior | Format, strict Clippy, locked build, and all-target/all-feature tests | Verify against firmware when commands or transport behavior change |
| Flutter application | Locked dependency restore, fatal analysis, tests, Linux release build, and no-codesign iOS build | Verify BLE and device workflows on a supported host and physical pod |
| Driver, sensor, LED, audio, or haptic | Host tests and firmware build | Flash and exercise the affected peripheral |
| Multi-pod or ESP-NOW | Host simulation and all builds | Two-pod discovery, simulation-off bidirectional benchmarks, then a separate simulated drill and trace capture |
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
6. Verify request/response behavior on every affected device transport. For a change confined to the
   shared frame or message contract, exercise at least one representative real transport.

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
(`/dev/ttyACM*`) is a separate console/JTAG interface. Over framed UART, the helper verifies the
exact built firmware version through `system info` and requires `system health` plus the complete
`system self-test` to pass; attach native USB separately when console logs are required.

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

Record the boot count and version before each clean `esp_restart()` and retain that build's ELF plus
`project_description.json`. The hardware workflow uses
`tools/firmware/verify_restart_snapshot.sh` to confirm that the stored count and version match the
pre-restart build, the format is 2, the recorded ELF SHA-256 matches that exact ELF, the internal
heap is plausible, and the processed backtrace resolves. Format-2 records are CRC-protected. Legacy
format-0 records are display-only because their heap and backtrace semantics are unverified; corrupt
or unsupported records fail closed, and `system crash-dump --clear` can remove an unreadable record.
A reset through ROM or the reset pin does not run the shutdown handler and must not be described as
producing a new clean-restart snapshot.

The normal success path does not prove rollback. Forced failed-self-test rollback requires a
purpose-built failing image or fault injection, then evidence that the bootloader selected the
previous image. Record it as unverified unless that destructive path was deliberately exercised.

For multi-pod trace inspection, capture the pods during the same ESP-NOW session and merge with
`--align zero`. This groups local timelines by capture start and does not correlate pod clocks.
`--align raw` preserves each file's local timestamps. Those are the only supported alignment modes;
neither creates synchronized timing evidence.

Keep the ESP-NOW latency and drill checks in separate lifecycles. With simulation off, enable both
pods, wait for complementary master/slave roles and exactly one peer each, benchmark the slave first
and then the master, and disable both. Clear and restart trace capture, enable simulation, start a
fresh ESP-NOW lifecycle, and wait for the complete drill before collecting traces.
After a disable request, `stopping` means a previous lifecycle is still unwinding. Wait until both
pods report the exact `disabled` state before re-enabling; cached role or peer data is not readiness.

Trace control and dump commands use serial or BLE. In a provisioned
`CONFIG_DOMES_WIFI_AUTO_CONNECT` build, live streaming uses its dedicated TCP port 5001; it is
separate from config port 5000 and does not make generic trace control valid over `--wifi`.

Flash coredumps and clean-restart snapshots are separate diagnostics. The active profile reserves a
`coredump` partition and enables ESP-IDF ELF dumps; retrieve and decode those with the exact matching
`domes.elf`. `domes-cli system crash-dump` reads only the NVS clean-restart snapshot.

## Continuous Integration

| Workflow | Purpose | Trigger scope |
| --- | --- | --- |
| [`firmware-ci.yml`](../.github/workflows/firmware-ci.yml) | Aggregate Software CI: ESP-IDF build/package validation, host tests, CLI checks, host tooling, protocol drift, and Flutter checks, exposed through `CI Gate` | Unfiltered `pull_request`, merge queue entry, and push to `main` |
| [`flutter-ci.yml`](../.github/workflows/flutter-ci.yml) | Reusable generated-binding, analysis, Flutter test, Linux release-build, and no-codesign iOS release-build jobs called by Software CI | `workflow_call` only |
| [`firmware-hw-test.yml`](../.github/workflows/firmware-hw-test.yml) | Self-hosted device checks | `hw-test` label and subsequent synchronize/reopen events while labeled, or manual run |
| [`firmware-release.yml`](../.github/workflows/firmware-release.yml) | Tag validation, the complete reusable Software CI gate, then OTA app, merged factory image, exact ELF/build identity, and checksums | Stable `vMAJOR.MINOR.PATCH` tags on `main` |

The Software CI workflow has no pull-request path filter, so documentation-only and code changes use
the same aggregate gate. The repository ruleset or branch-protection configuration must require the
exact `CI Gate` check name for that result to block merges; workflow configuration alone does not
enforce it. Resolve base-branch conflicts before relying on a pull-request result because GitHub
cannot build the pull-request merge revision while it is conflicted.

Release tags must use the stable `vMAJOR.MINOR.PATCH` form and be at most 31 bytes so the exact tag
fits the ESP-IDF application descriptor and OTA wire field. Prerelease identifiers and build
metadata are intentionally rejected. Release metadata names the OTA application `domes-<tag>.bin`;
the merged factory image is a separate artifact.

Software/release CI uses fresh build directories and SDKCONFIG files. Hardware CI builds
separate normal, versioned-OTA, and purpose-built failed-self-test images, also with isolated
SDKCONFIG files, and asserts that `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is active before it performs
the destructive rollback sequence. The failure image is test-only and must never be published as a
release artifact. The hardware workflow also verifies direct and registry-backed multi-device
fan-out and exercises truncated and interrupted recovery over serial and BLE before accepted OTA.

Ask before applying the `hw-test` pull-request label because it consumes attached lab hardware.
Manual hardware dispatch requires exactly two selected devices and accepts a comma-separated `ports` input;
use CP2102N `/dev/serial/by-id/` paths on a runner with stable device identities. The workflow does
not provision the machine: an online Linux x64 self-hosted runner, Actions Runner 2.327.1 or newer,
ESP-IDF v5.4.4, Rust 1.92.0, native BLE, and two attached NFF pods are prerequisites. Do not apply
the label when no qualifying runner is online; the job will remain queued and provides no evidence.

The release OTA image is not a blank-device installer. Use the separately published merged factory
image, or `idf.py flash` from a matching checkout, for initial programming.
