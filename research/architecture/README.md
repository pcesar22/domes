# DOMES Detailed Architecture Records

> **Document status: Current lifecycle index.** This page classifies the detailed documents in this
> directory. Most are design history or proposals; entries explicitly marked current as-built
> references are maintained against the implementation.

## How To Use This Directory

Start with [`../SOFTWARE_ARCHITECTURE.md`](../SOFTWARE_ARCHITECTURE.md) for the as-built system and
its authority map. Use the files in this directory to recover rationale, compare proposed designs,
or plan future work.

Do not copy pin values, packet layouts, partition sizes, NVS keys, commands, paths, or test counts
from historical or proposed documents without checking the authoritative source. A document marked
"partially implemented" still contains future or superseded material.

Contributor rules live in [`../../AGENTS.md`](../../AGENTS.md), firmware-specific rules in
[`../../firmware/AGENTS.md`](../../firmware/AGENTS.md), and current verification commands in
[`../../docs/TESTING.md`](../../docs/TESTING.md).

## Lifecycle States

| State | Meaning |
| --- | --- |
| Current reference | Maintained as an index or as-built map and safe to navigate from |
| Retired decision record | Obsolete tutorial or structure content removed; the file retains only the decision and current replacement |
| Historical scaffold | Early setup or implementation guidance retained only for history |
| Design proposal | Intended behavior or structure that was not adopted as written |
| Partially implemented | Some concepts exist, but the document is not an accurate description of current behavior |
| Superseded reference | Lookup data replaced by source-controlled implementation or hardware artifacts |

## Document Index

| Document | Lifecycle state | Current relationship and replacement |
| --- | --- | --- |
| [`00-getting-started.md`](00-getting-started.md) | Retired decision record | Obsolete setup commands were removed; the file points to the maintained setup, testing, and bring-up documents. |
| [`01-project-structure.md`](01-project-structure.md) | Retired decision record | The unadopted tree and templates were removed; the file records the current layout authorities. |
| [`02-build-system.md`](02-build-system.md) | Historical scaffold | Describes unimplemented platform Kconfig and Linux-target workflows. Use checked-in CMake/Kconfig files and `docs/TESTING.md`. |
| [`03-driver-development.md`](03-driver-development.md) | Design proposal, partially implemented | Interface and dependency-injection concepts remain relevant; examples and paths require verification against `main/interfaces/` and `main/drivers/`. |
| [`04-communication.md`](04-communication.md) | Design proposal, partially implemented | Packet tables and BLE topology are not current contracts. Use protobuf schemas, frame codec, transport source, and `espNowProtocol.hpp`. |
| [`05-game-engine.md`](05-game-engine.md) | Design proposal, partially implemented | The per-pod FSM exists, but general drill orchestration and several primitives remain proposed. Use `gameEngine.*` and `espNowService.*`. |
| [`06-testing.md`](06-testing.md) | Historical scaffold | Unity/CMock instructions are obsolete. Host firmware tests use GoogleTest/CTest; use `docs/TESTING.md`. |
| [`07-debugging.md`](07-debugging.md) | Partially implemented, operational details stale | Some debugging rationale remains useful. Use project Codex skills, current trace source, and `domes-cli --help` for commands. |
| [`08-ota-updates.md`](08-ota-updates.md) | Design proposal, partially implemented | OTA exists, but the documented partitions, phone relay, and examples differ. Use `partitions.csv`, OTA source, CLI source, and `docs/TESTING.md`. |
| [`09-reference.md`](09-reference.md) | Superseded reference | Its pins, NVS keys, partitions, UUIDs, and error examples are not live data. Use the authority map in `SOFTWARE_ARCHITECTURE.md`. |
| [`10-host-simulation.md`](10-host-simulation.md) | Current reference | Describes the GoogleTest/CTest host project, deterministic multi-pod simulation, trace generator, and hardware boundary. |
| [`11-system-modes.md`](11-system-modes.md) | Design proposal, partially implemented | `ModeManager` and mode commands exist; power management and several transition triggers remain proposed. Use `modeManager.*` and `main.cpp`. |
| [`12-multi-pod-orchestration.md`](12-multi-pod-orchestration.md) | Target design, partially implemented | Current firmware supports discovery, MAC-based roles, and a fixed drill. Phone-selected master and general drill interpretation remain proposed. |
| [`trace-overhaul-architecture.md`](trace-overhaul-architecture.md) | Current reference | Describes the separate console/UART topology, trace protobuf and binary event boundary, retained dump snapshot, streaming, and local-only merge behavior. |

## Promotion And Retirement

A proposal is promoted only when implementation, automated checks, and any required hardware
verification are complete. Promotion means updating the as-built architecture and milestone tracker;
it does not make every example in the original proposal current.

When a proposal is abandoned or replaced, keep the file for rationale, change its banner to
"superseded," and link the replacement. Do not maintain a second live copy of protocol values,
pinouts, NVS schemas, or build configuration in this directory.
