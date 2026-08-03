# 05 - Game Engine Decisions

> **Document status: Design proposal, partially implemented.** The per-pod game state machine is
> implemented. The original general drill interpreter, primitive catalog, packet mapping, shared
> clock, and scoring architecture remain proposals and are not current production classes.

## Current Authorities

| Concern | Authority |
| --- | --- |
| Per-pod game state and behavior | [`../../firmware/domes/main/game/gameEngine.hpp`](../../firmware/domes/main/game/gameEngine.hpp) and [`gameEngine.cpp`](../../firmware/domes/main/game/gameEngine.cpp) |
| Current two-pod fixed drill | [`../../firmware/domes/main/services/espNowService.hpp`](../../firmware/domes/main/services/espNowService.hpp) and [`espNowService.cpp`](../../firmware/domes/main/services/espNowService.cpp) |
| Current ESP-NOW messages | [`../../firmware/domes/main/services/espNowProtocol.hpp`](../../firmware/domes/main/services/espNowProtocol.hpp) |
| System-mode ownership | [`../../firmware/domes/main/config/modeManager.hpp`](../../firmware/domes/main/config/modeManager.hpp) |
| Direct-BLE app orchestration | [`../../ios/domes_app/lib/application/providers/drill_provider.dart`](../../ios/domes_app/lib/application/providers/drill_provider.dart) |
| Host verification | [`../../firmware/test_app/README.md`](../../firmware/test_app/README.md) |
| Hardware evidence requirements | [`../../docs/TESTING.md`](../../docs/TESTING.md) |

## Game State Machine

Each pod owns one `GameEngine`. Its implemented cycle is:

```text
Ready -> Armed -> Triggered -> Feedback -> Ready
             \-> Feedback -> Ready  (timeout)
```

Arming records a local monotonic timestamp. The game task polls touch input, reports a hit with a
local reaction interval or a miss after timeout, invokes configured feedback callbacks, and returns
to ready after feedback. Explicit disarm returns any state to ready.

State transitions are synchronized because protocol commands and the game task can run on different
cores. Application callbacks run outside the state lock so they may safely query or disarm the
engine. Pod identity tags events, but identity does not select an ESP-NOW master.

The engine is intentionally local. It does not own device discovery, host registration, drill
history, network role assignment, or a cross-pod clock.

## Decisions Retained

- Measure reaction time on the target pod with one local monotonic clock.
- Keep per-pod state independent so transport orchestration cannot silently become game state.
- Reject a new arm request while a round is already active.
- Make timeout, forced disarm, and feedback completion explicit transitions.
- Dispatch hardware feedback through injected callbacks so deterministic behavior can be host
  tested without claiming physical output.
- Correlate ESP-NOW arm and result traffic at the service boundary with an active-round token.
- Keep trace events around arm, hit, miss, feedback, and reaction time for post-run inspection.

## Current Orchestration Boundaries

The firmware's `EspNowService` owns the current discovery, deterministic role selection, peer-game
commands, and fixed two-pod drill. The Flutter app implements a different direct-BLE workflow and
owns its active-target and stale-completion protection in app providers. Neither path is the general
drill interpreter described by the original proposal.

## Drill Types

The firmware currently contains one fixed two-pod drill, while the Flutter application has a direct
BLE drill workflow. The original solo, reaction, sequence, and custom drill catalog was not
implemented as a general firmware API. Treat additional drill types as product work requiring an
owned schema, resource bounds, cancellation rules, and verification.

## Future Product Work

- A versioned drill definition and interpreter with explicit validation and cancellation semantics.
- App-directed pod selection and multi-pod scheduling beyond the fixed two-pod firmware drill.
- Result aggregation, persistence, player/session metadata, and export.
- A truthful synchronization protocol before comparing timestamps from different pods.
- Product decisions for reusable sound and haptic primitives after their hardware workflows are
  complete.

Future orchestration must compose the existing per-pod engine rather than replace its local timing
and cancellation guarantees. New host-facing commands belong in protobuf schemas; current peer
message details remain owned by `espNowProtocol.hpp`.

## Historical Material Removed

The original document contained packet IDs that conflict with the current ESP-NOW protocol,
nonexistent production classes, speculative primitive APIs, and build instructions that bypassed
the repository verification workflow. Those details were removed; Git history retains the proposal.
