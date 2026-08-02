# Testing And Verification

This document owns the repository verification matrix. Component READMEs may link here, but should
not maintain separate, conflicting test requirements.

## Aggregate Local Check

Run the repository verification entry point before publishing a broad change:

```bash
scripts/verify.sh
```

It checks generated bindings, host firmware tests, CLI format/lint/build/tests, host tooling, the
Flutter app, and the ESP-IDF firmware build. `scripts/verify.sh --quick` skips only the ESP-IDF
build; use it for iteration, not final firmware verification.

## Verification Matrix

| Change type | Required checks | Hardware expectation |
| --- | --- | --- |
| Documentation only | Link/path checks and relevant command syntax | None unless instructions changed a hardware workflow |
| Host firmware logic | Host unit tests | None when behavior is fully simulated |
| Firmware build/config | Host unit tests and ESP-IDF build | Flash when runtime behavior can change |
| Protocol or transport | Host tests, firmware build, CLI tests | Verify at least one real transport |
| CLI-only behavior | `cargo fmt --check`, `cargo clippy`, `cargo test` | Verify against firmware when commands or transport behavior change |
| Flutter application | `flutter analyze` and `flutter test` | Verify BLE and device workflows on a supported host and physical pod |
| Driver, sensor, LED, audio, or haptic | Host tests and firmware build | Flash and exercise the affected peripheral |
| Multi-pod or ESP-NOW | Host simulation and all builds | Two-pod discovery and command/drill verification |
| OTA | Firmware and CLI tests/builds | Transfer, reboot, version, and rollback/boot validation |

If required hardware is unavailable, record exactly which device-facing behavior remains
unverified. A successful build is not a hardware pass.

## Host Firmware Tests

The host suite uses GoogleTest and CTest; it does not use Unity or CMock.

```bash
cd firmware/test_app
mkdir -p build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Use `ctest -N` for the current discovered count. The suite covers frame and OTA codecs, protobuf
messages, feature and mode management, game behavior, multi-pod simulation, and drill/Perfetto
export. Dated verification snapshots belong in `firmware/MILESTONES.md`.

## Firmware Build

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
```

The project supports ESP-IDF v5.x. CI is pinned in
[`.github/workflows/firmware-ci.yml`](../.github/workflows/firmware-ci.yml); local validation should
record the exact `idf.py --version` used. A build must fit the smallest app partition defined in
[`firmware/domes/partitions.csv`](../firmware/domes/partitions.csv).

## CLI Checks

```bash
cd tools/domes-cli
cargo fmt --check
cargo clippy --all-targets --all-features
cargo build
cargo test
```

Use `cargo run -- --help` and subcommand `--help` output to validate documented command syntax.

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
[`firmware/common/protocol/otaProtocol.hpp`](../firmware/common/protocol/otaProtocol.hpp) and mirrored
by the CLI. Do not add another hand-written protocol family; keep this pair wire-compatible until
OTA is migrated to protobuf. The internal ESP-NOW peer protocol is a second bounded exception,
mirrored by the host simulator. It is not a host config transport contract.

## Flutter Checks

```bash
cd ios/domes_app
flutter pub get
flutter analyze
flutter test
```

These checks do not validate Bluetooth permissions, discovery, connection lifecycle, OTA, or a
physical pod. Run those workflows on native Linux or a supported mobile target with real hardware.

## Hardware Verification

Single-device firmware flash and boot check:

```bash
. ~/esp/esp-idf/export.sh
tools/firmware/flash_and_verify.sh \
  firmware/domes /dev/ttyACM0 "DOMES"
```

Then use `domes-cli` for the affected behavior. Examples:

```bash
domes-cli --port /dev/ttyACM0 system self-test
domes-cli --port /dev/ttyACM0 feature list
domes-cli --port /dev/ttyACM0 led solid --color ff0000
domes-cli --port /dev/ttyACM0 espnow status
```

LED behavior needs visual confirmation. Touch, IMU, haptic, and audio need the corresponding
physical stimulus or output confirmation. Multi-pod and ESP-NOW behavior needs at least two pods.

## Continuous Integration

| Workflow | Purpose | Trigger scope |
| --- | --- | --- |
| [`firmware-ci.yml`](../.github/workflows/firmware-ci.yml) | ESP-IDF build, host tests, CLI checks, trace-tool tests, and script syntax | Firmware, host tooling, CLI, protocol, or workflow changes on `main` and `develop` |
| [`flutter-ci.yml`](../.github/workflows/flutter-ci.yml) | Generated bindings, analysis, and Flutter tests | App, config schema, generator, or workflow changes on `main` and `develop` |
| [`firmware-hw-test.yml`](../.github/workflows/firmware-hw-test.yml) | Self-hosted device checks | `hw-test` label, selected pushes, manual run |
| [`firmware-release.yml`](../.github/workflows/firmware-release.yml) | Nanopb drift check and versioned firmware artifacts | Release tags |

Ask before applying the `hw-test` pull-request label because it consumes attached lab hardware.
