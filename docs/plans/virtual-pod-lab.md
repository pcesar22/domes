# App Virtual Pod Lab

The phone app can launch a deterministic two- or six-pod virtual lab from the drill setup screen.
This is an app model for exercising discovery, repository commands, provider lifecycles, and
pod-owned touch notifications without physical hardware. It is not an RF emulator, a production
simulator, a timing prediction, or evidence about physical pod behavior.

## Architecture

- `PodEnvironment.appVirtualModel` explicitly distinguishes virtual identities from physical pods.
  Physical connection requests still use the BLE connector and cannot fall back to this model.
- `VirtualPodTransport` handles the app's current repository commands using the committed generated
  protobuf types and emits protobuf touch notifications on the unsolicited-frame stream.
- `VirtualPodLabNotifier` owns virtual identities, connections, transports, and restart/stop
  cleanup. `MultiPodNotifier` retains the existing connection-generation and subscription checks.
- `DrillNotifier` routes every target, including virtual targets, through `PodRepository`; the old
  direct command and touch shortcuts are removed. A running lab supplies its seed while the
  physical-pod default remains unseeded. `AppClock` makes timers and timestamps injectable, and
  `DeterministicAppClock` advances only when requested by a test.

Stable identities are `app-virtual-pod-01` through `app-virtual-pod-06`. Stopping, restarting, or
disposing the lab disconnects its transports and invalidates the associated provider generations.

## Reproducible verification

From `ios/domes_app`, with Flutter 3.44.8 and the committed lockfile:

```bash
flutter pub get --enforce-lockfile
dart format --output=none --set-exit-if-changed \
  lib/domain lib/data/transport lib/application/providers \
  lib/presentation/screens/drill_setup_screen.dart \
  lib/presentation/screens/drill_active_screen.dart test
flutter analyze --fatal-infos --fatal-warnings
flutter test
```

The exact commit and check results belong in the pull request's technical evidence. App tests are
software evidence only; they do not establish BLE, RF, hardware, or predictive behavior.
