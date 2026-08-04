# 11 - System Modes And Power Management

> **Document status: Retired decision record.** The original mixed implementation plan was removed
> on 2026-08-02 because it combined current `ModeManager` behavior with unimplemented power,
> transition, persistence, and orchestration designs. Git history preserves the proposal.

## Adopted Boundary

The firmware has a per-pod `ModeManager` that owns validated mode transitions, applies
mode-controlled feature masks through `FeatureManager`, tracks activity, and enforces bounded
TRIAGE, ERROR, and GAME timeouts. Config commands expose the current mode and explicit transition
requests. Explicit actions count as activity; passive inspection does not.

Mode enum values are protocol-owned. Transition rules, masks, and timeouts are implementation-owned
and must not be copied into this historical record.

## Current Authorities

| Concern | Current source |
| --- | --- |
| Mode states, transition API, and timeouts | [`modeManager.hpp`](../../firmware/domes/main/config/modeManager.hpp) |
| Transition rules and feature-mask policy | [`modeManager.cpp`](../../firmware/domes/main/config/modeManager.cpp) |
| Mode and feature protocol enums | [`config.proto`](../../firmware/common/proto/config.proto) |
| Command activity and mode responses | [`configCommandHandler.cpp`](../../firmware/domes/main/config/configCommandHandler.cpp) |
| Runtime construction and transport integration | [`main.cpp`](../../firmware/domes/main/main.cpp) |
| Host behavior tests | [`test_mode_manager.cpp`](../../firmware/test_app/main/test_mode_manager.cpp) |
| As-built software boundaries | [`SOFTWARE_ARCHITECTURE.md`](../SOFTWARE_ARCHITECTURE.md) |
| Delivery and hardware evidence | [`PROGRAM_STATUS.md`](../../PROGRAM_STATUS.md) |

## Not Adopted As Written

The retired proposal's automatic BLE-connect, long-press, and hardware-fault transitions are not
current behavior. Neither are battery-aware sleep states, dynamic service polling, persisted
`last_mode` or `mode_trans` keys, or a firmware-resident phone-selected master and general drill
interpreter. These remain separate product decisions and must not be inferred from the existence of
`ModeManager`.

Multi-pod targets and the distinction between the direct-BLE app workflow and future firmware
orchestration are tracked in
[`12-multi-pod-orchestration.md`](12-multi-pod-orchestration.md). Use
[`docs/TESTING.md`](../../docs/TESTING.md) for verification requirements.
