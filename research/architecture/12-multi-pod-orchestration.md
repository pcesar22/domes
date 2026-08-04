# 12 - Multi-Pod Orchestration

> **Document status: Target design, partially implemented.** Current firmware supports two-pod
> discovery, deterministic MAC-based roles, peer diagnostics, a latency benchmark, and a fixed
> drill. App-selected masters, a general drill interpreter, six-pod operation, and synchronized pod
> clocks remain product targets.

## Current Authorities

| Concern | Authority |
| --- | --- |
| As-built software boundary | [`../SOFTWARE_ARCHITECTURE.md`](../SOFTWARE_ARCHITECTURE.md) |
| ESP-NOW service lifecycle | [`../../firmware/domes/main/services/espNowService.hpp`](../../firmware/domes/main/services/espNowService.hpp) and [`espNowService.cpp`](../../firmware/domes/main/services/espNowService.cpp) |
| Peer wire contract | [`../../firmware/domes/main/services/espNowProtocol.hpp`](../../firmware/domes/main/services/espNowProtocol.hpp) |
| Per-pod game state | [`../../firmware/domes/main/game/gameEngine.hpp`](../../firmware/domes/main/game/gameEngine.hpp) |
| Direct-BLE app drill | [`../../ios/domes_app/lib/application/providers/drill_provider.dart`](../../ios/domes_app/lib/application/providers/drill_provider.dart) |
| Multi-device CLI | [`../../tools/domes-cli/README.md`](../../tools/domes-cli/README.md) |
| Dated delivery evidence | [`../../PROGRAM_STATUS.md`](../../PROGRAM_STATUS.md) |
| Required verification | [`../../docs/TESTING.md`](../../docs/TESTING.md) |

## Discovery

The firmware discovers nearby peers, selects a peer for the current game lifecycle, and derives
master/slave roles from the station MAC addresses. The role is runtime state; there is no persistent
preferred-master NVS key. Pods do retain operational configuration, identity, counters, and
diagnostic data, so they are not generally stateless.

## Current Drill

The selected master runs the fixed firmware drill and alternates local and peer rounds. Each target
pod measures its reaction interval locally. Peer results carry the active round's correlation token
so a delayed hit or timeout cannot satisfy another round. Simulation can inject a bounded touch for
automated drill validation, but simulation must be disabled for radio latency benchmarks.

The Flutter application currently orchestrates its drill through direct BLE connections to the
selected pods. A BLE connection does not promote that pod to firmware ESP-NOW master, and the app is
not an ESP-NOW relay.

## Host Orchestration

`domes-cli` owns host device registration and fan-out. A registry name, stable serial path, firmware
pod ID, BLE address, and WiFi MAC are distinct identities and must not be inferred from one another.

## Protocol Boundaries

- Host config and diagnostics use protobuf messages over UART, BLE, or build-gated TCP config.
- Raw application-image transfer is a separate bounded contract supported over serial and BLE, not
  through the TCP config server.
- Pod-to-pod discovery and game events use the internal ESP-NOW protocol.

The owning source files define all message values and validation rules. This target record does not
reserve packet IDs or introduce host protocol types.

## Decisions Retained For Product Design

- Keep reaction timing local to the target pod.
- Keep per-pod mode and game state independent from host registry state.
- Make role selection, selected peer, active round, and cancellation explicit lifecycle state.
- Require exact event-to-round correlation and ignore stale asynchronous results.
- Separate simulation-backed drill validation from simulation-off latency measurement.
- Treat discovery, connectivity, packet exchange, drill completion, and physical interaction as
  distinct readiness claims.
- Keep drill history and player/session data out of the firmware unless a bounded persistence
  requirement is defined.

## Drill Setup

The current firmware uses its compiled fixed sequence; it does not accept the original proposal's
general drill program from a phone. A future setup flow must validate the complete definition before
changing pod state and must define cancellation and partial-join behavior.

## Product Targets

The intended product may allow an app to choose one pod as an orchestration master, distribute a
validated drill definition, run with six or more pods, tolerate peer loss, and collect results. That
requires explicit work in all of these areas:

1. A versioned drill schema and interpreter with resource bounds.
2. App-to-master selection and authorization semantics.
3. Join, leave, retry, cancellation, and partial-result behavior for more than two pods.
4. A synchronization marker and measured error bounds before comparing timestamps across pods.
5. Hardware evidence for latency, recovery, interference, and physical input at the target scale.

Sub-millisecond delivery and millisecond-level cross-pod synchronization are product targets, not
current measurements. Capture-start trace grouping does not synchronize device clocks.

## Historical Material Removed

The original proposal mixed current and future behavior, including a nonexistent persistent-master
setting, phone-selected runtime roles, generic command classes, copied packet layouts, speculative
timing guarantees, and cloud persistence sketches. The useful ownership and lifecycle decisions
remain above; Git history retains the rest.
