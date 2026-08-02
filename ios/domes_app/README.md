# DOMES Controller App

Flutter controller prototype for discovering and operating DOMES pods over BLE. The app contains
device connection, feature and LED controls, drill setup/results, OTA, settings, and multi-pod state.
The Rust CLI remains the primary development and service interface while mobile workflows are being
validated.

## Project Status

The application has substantial domain, protocol, provider, and UI implementations, but device BLE
behavior is not established by widget tests. Generated platform runners also do not imply that every
target has completed Bluetooth permission, packaging, or hardware validation.

See [`../../firmware/MILESTONES.md`](../../firmware/MILESTONES.md) for current delivery status and
[`../../docs/TESTING.md`](../../docs/TESTING.md) for verification requirements.

## Setup

Install Flutter with a Dart SDK compatible with [`pubspec.yaml`](pubspec.yaml), then run:

```bash
cd ios/domes_app
flutter doctor
flutter pub get
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
flutter analyze
flutter test
```

These checks cover models, providers, framing compatibility, and widgets. They do not replace a real
BLE scan, connect, command, disconnect, and OTA exercise.

## Architecture

```text
lib/
  application/providers/   Riverpod state and workflow coordination
  data/proto/generated/    Generated Dart protobuf bindings
  data/protocol/           Frame payload and command helpers
  data/transport/          BLE and transport abstractions
  domain/models/           Pods, drills, events, and results
  domain/repositories/     Pod operations and transport-backed implementation
  presentation/            Screens, widgets, and theme
```

Tests mirror these boundaries under [`test/`](test/). Keep Bluetooth APIs in the transport layer and
keep screens dependent on providers rather than direct GATT operations.

## Protocol Generation

[`../../firmware/common/proto/config.proto`](../../firmware/common/proto/config.proto) is the source
of truth for shared config messages. Do not edit files under `lib/data/proto/generated/` manually.

Install `protoc` and the Dart protobuf plugin, make `protoc-gen-dart` available on `PATH`, then run:

```bash
dart pub global activate protoc_plugin 25.0.0
export PATH="$PATH:$HOME/.pub-cache/bin"
../../tools/generate_protocols.sh dart
```

Review the generated diff, then run `flutter analyze` and `flutter test`. A protocol change also
requires the firmware and CLI checks documented in the repository verification matrix.

## Platform Notes

- Linux BLE depends on BlueZ and a compatible adapter.
- iOS builds require the appropriate Bluetooth usage descriptions and signing configuration before
  device deployment.
- Web Bluetooth availability depends on browser and platform support and is not a substitute for the
  native BLE validation path.
