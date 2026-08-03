# DOMES Flutter App Guidelines

These rules supplement the repository root `AGENTS.md`. Use `README.md` for the current app overview
and `../../docs/TESTING.md` for canonical verification commands and hardware evidence.

## Architecture Boundaries

Keep responsibilities aligned with the existing tree:

| Layer | Responsibility |
| --- | --- |
| `domain/models/` | Pod, drill, and result concepts |
| `domain/repositories/` | Pod-operation interfaces and the current transport-backed implementation |
| `data/proto/generated/` | Generated Dart protobuf bindings; never edit manually |
| `data/protocol/` | Frame payloads, response envelopes, and the bounded OTA wire format |
| `data/transport/` | BLE/GATT, byte framing, notification demultiplexing, and transport cleanup |
| `application/providers/` | Riverpod-owned state, connection lifecycles, and workflow orchestration |
| `presentation/` | Screens, widgets, navigation, and theme; consume providers instead of GATT APIs |

Keep new Bluetooth operations in the transport/application boundary. `PodDevice` currently carries
the direct-BLE handle needed by the connector; do not spread platform handles into unrelated
models or presentation code. Use the stable pod address as connection identity.

## Riverpod And Async State

Providers own mutable application state and external-resource lifetimes. Screens should `watch`
state and `read` notifiers for actions; they must not create parallel connection or drill state.
Keep provider dependencies explicit and injectable so `ProviderContainer` tests can replace device
work. Use `autoDispose` only when teardown is complete and losing state on the last listener is
intentional.

Existing providers are hand-written `StateNotifierProvider` definitions. If annotation-generated
providers are introduced, edit their source declarations, never generated `.g.dart` output, and run
the pinned `build_runner` workflow. Do not maintain hand-written and generated owners for the same
state.

Every asynchronous connection, command, timer, and drill completion must prove that it still
belongs to the active operation before publishing state:

- increment a generation/session token when connect, disconnect, stop, restart, or dispose
  supersedes prior work;
- after every relevant `await`, require both `mounted` and the original generation;
- cancel timers and stream subscriptions during teardown;
- disconnect a transport created by a completion that became stale;
- never let a previous round, pod, or connection update the replacement session.

Add race-focused tests whenever this lifecycle changes.

## BLE Contract

- Install the notification listener before enabling GATT notifications so an early value is not
  lost.
- Serialize request/response operations. Unsolicited touch notifications must bypass the command
  response waiter and retain the reporting pod identity.
- A partial write, timeout, unexpected response type, disconnect, or ambiguous response poisons the
  command channel; require disconnect/reconnect before another command.
- On disconnect or dispose, cancel notification and connection subscriptions, reset pending frame
  waiters, close owned streams, and publish deterministic disconnected state.
- Touch completes a drill round only while that pod is the active target. Simulation is permitted
  only for explicit `sim-pod-*` targets, never as a fallback for a physical pod.

Unit or widget tests with fake transports do not establish scan, GATT, notification, reconnection,
or physical touch behavior.

## Protocol And OTA

`../../firmware/common/proto/*.proto` is the source of truth for shared config and trace messages.
Do not edit `lib/data/proto/generated/` by hand. Run `../../tools/generate_protocols.sh dart`, review
the generated diff, and verify firmware and Rust consumers for cross-language schema changes.

Use generated `MsgType` values for config/system messages. Preserve the documented status envelope:
most command responses are `[status][protobuf]`, while list/diagnostic responses without command
status and unsolicited touch notifications are bare protobuf payloads.

OTA chunk transfer in `lib/data/protocol/ota_protocol.dart` is a bounded legacy fixed-binary
exception mirrored in firmware C++ and the Rust CLI. Any wire change requires compatibility updates
and tests in all three consumers. The supplied OTA version must be parser-valid, at most 31 ASCII
bytes, and byte-for-byte equal to the version embedded in the selected image. A normal OTA success
does not prove forced failed-self-test rollback.

## Verification

For every app change, run locked dependency restore, `dart format` on changed Dart files, fatal
analysis, and relevant unit/widget tests. Run the full Flutter test suite for provider, protocol,
transport, or cross-screen behavior. Protocol changes also require generated-binding drift checks
and the firmware/CLI checks in `../../docs/TESTING.md`.

Build Linux release on a supported native Linux host when desktop code or packaging is affected.
Build iOS release with `--no-codesign` on macOS when iOS code or packaging is affected; Linux cannot
validate that build.

Physical BLE, drill, or OTA completion requires a supported host/mobile target and a real pod. Test
scan, connect, command/notification routing, disconnect/reconnect, and the feature-specific action.
For drills, use a physical touch on the active pod. For OTA, verify the exact image version, health,
self-test, and post-reboot state; test forced rollback separately. If the required host or hardware
is unavailable, report the precise remaining checks as unverified.
