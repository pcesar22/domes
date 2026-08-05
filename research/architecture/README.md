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
| Target design | Product direction that remains future work unless explicitly identified as implemented |
| Partially implemented | Some concepts exist, but the document is not an accurate description of current behavior |

## Document Index

| Document | Lifecycle state | Current relationship and replacement |
| --- | --- | --- |
| [`00-getting-started.md`](00-getting-started.md) | Retired decision record | Obsolete setup commands were removed; the file points to the maintained setup, testing, and bring-up documents. |
| [`01-project-structure.md`](01-project-structure.md) | Retired decision record | The unadopted tree and templates were removed; the file records the current layout authorities. |
| [`02-build-system.md`](02-build-system.md) | Retired decision record | Obsolete platform, Linux-target, partition, and flash guidance was removed; the file points to the maintained build authorities. |
| [`03-driver-development.md`](03-driver-development.md) | Retired decision record | The obsolete interface and implementation tutorial was removed; the page points to current driver, composition, and verification authorities. |
| [`04-communication.md`](04-communication.md) | Design proposal, partially implemented | Preserves adopted protocol-family and validation decisions while linking all live wire details to their owning sources. |
| [`05-game-engine.md`](05-game-engine.md) | Design proposal, partially implemented | Describes the implemented per-pod FSM boundary and separates it from the unimplemented general drill architecture. |
| [`06-testing.md`](06-testing.md) | Retired decision record | Obsolete Unity/CMock, coverage, device, and CI instructions were removed; the file points to the maintained verification matrix. |
| [`07-debugging.md`](07-debugging.md) | Retired decision record | Stale line breakpoints, device paths, coredump settings, and permission workarounds were removed; the file points to current runbooks. |
| [`08-ota-updates.md`](08-ota-updates.md) | Retired decision record | Obsolete partition, transport, and CLI examples were removed; the file points to the implemented OTA and programming contracts. |
| [`09-reference.md`](09-reference.md) | Retired decision record | Copied lookup tables were removed; the record redirects each subject to its source-controlled authority. |
| [`10-host-simulation.md`](10-host-simulation.md) | Current reference | Describes the GoogleTest/CTest host project, deterministic multi-pod simulation, trace generator, and hardware boundary. |
| [`11-system-modes.md`](11-system-modes.md) | Retired decision record | Mixed current and proposed mode guidance was removed; the page separates the implemented `ModeManager` boundary from unadopted power and orchestration targets. |
| [`12-multi-pod-orchestration.md`](12-multi-pod-orchestration.md) | Target design, partially implemented | Separates the current two-pod fixed workflow from app-selected master, general drill, six-pod, and synchronized-clock targets. |
| [`13-deterministic-virtual-platform.md`](13-deterministic-virtual-platform.md) | Target design | Defines conditional QEMU adoption, physical/QEMU composition roots, the production radio and peer-contract seams, scheduler/causal observability, deterministic transport, CI, and hardware-calibrated qualification; implementation state remains in `PROGRAM_STATUS.md`. |
| [`trace-overhaul-architecture.md`](trace-overhaul-architecture.md) | Current reference | Describes the separate console/UART topology, trace protobuf and binary event boundary, retained dump snapshot, streaming, and local-only merge behavior. |

## Promotion And Retirement

A proposal is promoted only when implementation, automated checks, and any required hardware
verification are complete. Promotion means updating the as-built architecture and program status;
it does not make every example in the original proposal current.

When a proposal is abandoned or replaced, keep a concise retirement record that links the
replacement; Git history retains the full proposal. Do not maintain a second live copy of protocol
values, pinouts, NVS schemas, commands, or build configuration in this directory.
