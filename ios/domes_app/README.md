# DOMES Controller App

Flutter controller prototype for discovering and operating DOMES pods over BLE. The app contains
device connection, feature and LED controls, drill setup/results, OTA, settings, and multi-pod state.
The Rust CLI remains the primary development and service interface while mobile workflows are being
validated.

## Project Status

The application has substantial domain, protocol, provider, and UI implementations, but device BLE
behavior is not established by widget tests. Generated platform runners also do not imply that every
target has completed Bluetooth permission, packaging, or hardware validation.

Physical drill input is implemented as the unsolicited `0x50` `TouchEventNotification`. The BLE
transport separates that bare-protobuf notification from command responses, the repository maps it
to its connected pod, and the drill accepts it only while that pod is the active target. Touch
simulation is restricted to explicit `sim-pod-*` targets. This path still requires an on-device app
drill before it is hardware verified.

See [`../../PROGRAM_STATUS.md`](../../PROGRAM_STATUS.md) for current delivery status and
[`../../docs/TESTING.md`](../../docs/TESTING.md) for verification requirements.

## Setup

Install Flutter 3.44.8, matching CI and `scripts/verify.sh`, then activate the protobuf generator
with `dart pub global activate protoc_plugin 25.0.0`. Run:

```bash
cd ios/domes_app
flutter doctor
flutter pub get --enforce-lockfile
```

List available targets and start the app on a selected device:

```bash
flutter devices
flutter run -d <device-id>
```

BLE validation requires a supported host or mobile target, a working Bluetooth adapter, and a
physical pod advertising the DOMES GATT service. Native Linux is required for repository
validation-critical desktop BLE work; see [`../../.codex/PLATFORM.md`](../../.codex/PLATFORM.md).

## Checks

```bash
flutter pub get --enforce-lockfile
flutter analyze --fatal-infos --fatal-warnings
flutter test
# On native Linux with the Flutter desktop prerequisites:
flutter build linux --release
# On macOS with Xcode:
flutter build ios --release --no-codesign
```

These checks cover models, providers, command/notification demultiplexing, touch-event routing,
drill lifecycle and race handling, framing compatibility, OTA contracts, and widgets. They do not
replace a real BLE scan, connect, physical-touch drill, command, disconnect, and OTA exercise.

## Architecture

```text
lib/
  application/providers/   Riverpod state and workflow coordination
  data/proto/generated/    Generated Dart protobuf bindings
  data/protocol/           Frame payload and command helpers
  data/transport/          BLE and transport abstractions
  domain/models/           Pods, drill configuration, and results
  domain/repositories/     Pod operations and transport-backed implementation
  presentation/            Screens, widgets, and theme
```

Tests mirror these boundaries under [`test/`](test/). Keep Bluetooth APIs in the transport layer and
keep screens dependent on providers rather than direct GATT operations.

The device-originated touch notification is a bare protobuf, not a status-wrapped response. Its
message ID and payload come from `config.proto`; do not consume it through a command-response waiter.

## Protocol Generation

[`../../firmware/common/proto/config.proto`](../../firmware/common/proto/config.proto) is the source
of truth for shared config messages. Do not edit files under `lib/data/proto/generated/` manually.

Install `protoc` and the Dart protobuf plugin, make `protoc-gen-dart` available on `PATH`, then run:

```bash
dart pub global activate protoc_plugin 25.0.0
export PATH="$PATH:$HOME/.pub-cache/bin"
../../tools/generate_protocols.sh dart
```

Review the generated diff, then run `flutter analyze --fatal-infos --fatal-warnings` and
`flutter test`. A protocol change also requires the firmware and CLI checks documented in the
repository verification matrix.

OTA chunk transfer is a bounded fixed-binary exception implemented in
`lib/data/protocol/ota_protocol.dart` and mirrored by the firmware C++ and Rust CLI implementations.
Changes to that wire format require compatibility updates and tests in all three consumers.
The OTA screen deliberately has no example version default: the operator must enter the parser-valid,
at-most-31-byte version embedded in the selected image, and firmware rejects a byte mismatch before
selecting the image for boot.

## Platform Notes

- Linux BLE depends on BlueZ and a compatible adapter.
- iOS builds require the appropriate Bluetooth usage descriptions and signing configuration before
  device deployment.
- Web Bluetooth availability depends on browser and platform support and is not a substitute for the
  native BLE validation path.
