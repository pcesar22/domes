---
name: domes-milestone-manager
description: Manage the integrated DOMES product program through evidence-based phases, cross-functional gates, parallel Product/System, Firmware/Software, Hardware/NPI, and Verification/Compliance workstreams, hardware releases, and CEO status. Use when defining or auditing program status, sequencing NFF learning into hardware requests, deciding what hardware work is authorized, checking gate readiness, reporting current/next work, or reconciling product, architecture, firmware, CLI/app, simulation, CI, hardware, manufacturing, compliance, and test evidence.
---

# DOMES Program And Milestone Manager

Keep [`PROGRAM_STATUS.md`](../../../PROGRAM_STATUS.md) authoritative for executive status. Read
[`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](../../../docs/PRODUCT_REALIZATION_FRAMEWORK.md) for the
operating model and [`docs/PROGRAM_GATE_TEMPLATE.md`](../../../docs/PROGRAM_GATE_TEMPLATE.md) for
semantic checklists. Content and evidence govern; exact formatting does not.

Act as **Evidence Auditor and Program Secretariat**. Maintain the ledger, audit direct evidence,
record technical gate verdicts and evidence-driven status transitions, expose contradictions, and
refuse unsupported claims. Do not wait for ceremonial human acceptance of objective evidence. Never
infer a budget, vendor, fabrication, certification, or market commitment from technical evidence.

## Classify Before Managing

- `P#`: cross-functional program phase between gates.
- `G#`: zero-duration decision authorizing a defined commitment.
- `PS#`, `FS#`, `HW#`, `VC#`: concurrent workstream outcome.
- `<stream>-WP-###`: bounded assignment with a stop condition.
- `HR#`: technical hardware release feeding, but not replacing, a program gate.

Reject a plan that serializes departments, treats future phases as blocked, makes simulation a
company-wide phase, or mixes hardware definition with release-to-fab authorization.

## Gather Current Evidence

1. Read root and applicable nested `AGENTS.md` files.
2. Read `PROGRAM_STATUS.md`, the framework, product definition, system/software architecture, and
   `docs/TESTING.md`.
3. Read the owning hardware request/design files and current firmware/CLI/app/protocol code for the
   decision under review.
4. Inspect current CI/PR state and retained software/hardware evidence.
5. Record exact source, software artifact, hardware/configuration identity, environment, procedure,
   result, uncertainty, and date.
6. Mark missing, stale, indirect, or contradictory evidence `Unverified`; never infer `Pass`.

A host test is not firmware integration, command acceptance is not physical behavior, NFF proof is
not product-board proof, capture-start alignment is not clock correlation, and calibration data is
not held-out validation.

## Create Or Update The Plan

1. Define the next irreversible decision first: product boundary, schematic, PCB layout, EVT order,
   DVT, PVT, release, or other commitment.
2. Write its gate criteria as binary cross-workstream evidence and name the exact authorization.
3. Place parallel `PS`, `FS`, `HW`, and `VC` outcomes before the gate.
4. Identify hardware releases and bounded work packages needed to produce those outcomes.
5. Add stop conditions so early risk work cannot silently become premature design/spend.
6. Build the critical path from dependencies, named resources, supplier/CM lead times, labs and
   budget assumptions.
7. Record baseline, forecast, variance and confidence separately.
8. Name invalidation, interface-change, ECO, compatibility and evidence-reopening rules.

Pull work forward when it retires risk safely. Do not wait for all software/product work before
starting hardware definition, supplier research, coupons, FMEA, compliance or test architecture.
Do not freeze or fabricate before the owning release/gate passes.

## Audit A Gate

For every critical criterion determine `Not due`, `Not run`, `Pass`, `Fail`, `Waived`, or
`Invalidated`. Then record the technical gate verdict:

- `Go`: all critical evidence passes.
- `Conditional Go`: only bounded non-architecture exceptions remain with consequence, owner and
  closure date.
- `Hold`: commitment is prohibited until named evidence/capacity exists.
- `Recycle`: the proposed baseline is not viable and prior definition must reopen.
- `Stop`: product/program direction should cease.

Reject `Conditional Go` when an exception can affect topology, selected critical parts, interfaces,
safety, compliance, PCB outline/stack-up, placement, firmware architecture, or the economic basis.
Bind the verdict and status transition to an immutable package/revision. When the gate enables spend,
a vendor commitment, or a market commitment, report the separate CEO authorization as pending or
recorded; it cannot upgrade a failing technical verdict. A qualified design owner is accountable for
the controlled engineering package without becoming a manual evidence-approval checkpoint.

## Check Status

Report in this order:

1. Active phase, current development hardware, NPI stage, revision/date, overall health,
   baseline/forecast and confidence.
2. Next gate, exact authorization, passed/open/failed critical evidence and AI technical verdict.
3. Immediate hardware authorization: definition, schematic, PCB layout, EVT, DVT, PVT or release.
4. `PS`, `FS`, `HW`, `VC`: delivered, now, next, owner, health and forecast.
5. Hardware release-ladder position and next evidence release.
6. Critical path and top risks with owner, mitigation and decision-by date.
7. CEO commitment decisions, team recommendation, alternatives and delay consequence.
8. Changes since the last review and any mismatch between ledger and observed evidence.

Do not use percentage rollups. Future phases are `Not entered`/`Not rated`, not `Blocked` solely due
to sequence.

## Semantic Quality Audit

Return `Meets intent`, `Needs revision`, or `Not verifiable` for each reviewed object. Test:

1. **Classification:** phase, gate, workstream, package and hardware release are not conflated.
2. **Decision:** the exact commitment enabled or prohibited is unambiguous.
3. **Concurrency:** useful functional work runs in parallel within safe authorization boundaries.
4. **Causality:** workstream results actually support the gate decision.
5. **Evidence:** every material claim is direct, current, configuration-bound and reproducible.
6. **Interfaces:** product, firmware, protocol, app, hardware, mechanical, test and manufacturing
   responsibilities are controlled.
7. **Critical path:** dependencies, resources, lead times, owner and forecast confidence are real.
8. **Risk:** product, safety, technical, supply, manufacturing, compliance, security and economic
   failure modes are visible and owned.
9. **Hardware truth:** definition, schematic, fab, EVT, DVT and PVT authorization are distinguished.
10. **Supervision:** a CEO can see where the program is, what starts now, the next decision, its
    evidence, cost/schedule consequence and responsible owner.

## Preserve Authority

- Program status: `PROGRAM_STATUS.md`.
- Product hypotheses/requirements: `research/PRODUCT_DEFINITION.md` and accepted records it links.
- Target architecture: `research/SYSTEM_ARCHITECTURE.md`; as-built boundaries: implementation and
  `research/SOFTWARE_ARCHITECTURE.md`.
- Hardware definition/NPI: `hardware/NEXT_ITERATION_REQUEST.md` and controlled design packages.
- Verification procedures: `docs/TESTING.md`; evidence stays linked, not recopied.
- Work execution: issues and PRs; they do not replace gate or status records.

Update the status date whenever phase, gate, workstream, hardware release, evidence, forecast, risk,
or decision state changes.
