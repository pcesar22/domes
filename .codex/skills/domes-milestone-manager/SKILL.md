---
name: domes-milestone-manager
description: Manage the integrated DOMES product program through evidence-based phases, cross-functional gates, parallel Product/System, Firmware/Software, Hardware/NPI, and Verification/Compliance workstreams, hardware releases, and CEO status. Use when the user says "Continue DOMES", asks the AI to select or execute the next priority, defines or audits program status, sequences NFF learning into hardware requests, decides what hardware work is authorized, checks gate readiness, or reconciles product, architecture, firmware, CLI/app, simulation, CI, hardware, manufacturing, compliance, and test evidence.
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

## Select Autonomous Work

When the user issues the autonomous-continuation directive in root `AGENTS.md`, first reconcile
`PROGRAM_STATUS.md` with the worktree, open execution plans, GitHub issues and pull requests, current
CI, and retained evidence. A more specific current user request always wins. Then apply this order:

1. Repair an urgent integrity failure: a safety or security defect, failing active PR or `main` CI,
   contradiction in an authority, invalidated evidence, or accepted evidence expiring before use.
2. Resume an existing unblocked execution plan or pull request before opening competing work.
3. Use the control panel's `Next AI-owned action` when it remains current, authorized, and unblocked.
4. Otherwise rank eligible work as: active next-gate critical path, ready next-gate critical path,
   other active, then other ready. Exclude `Not due`, prohibited, externally owned, and blocked work.
5. Break ties by earliest decision or forecast date, most named next-gate dependencies unlocked,
   greatest material risk retired, then lowest stable work-package ID.
6. Select exactly one smallest coherent evidence-producing deliverable with observable acceptance,
   an owner, dependencies, invalidation rule, and stop condition. Do not select generic cleanup while
   gate-critical work is eligible.

If the highest program dependency needs a CEO, supplier, lab operator, purchase, or physical action,
keep that decision visible and evaluate the next AI-owned candidate instead of stalling or treating
the dependency as authorized. Record the selected package, rationale, execution issue, gate or risk
unlocked, and next reserved decision in `PROGRAM_STATUS.md`.

## Execute Selected Work

1. Reuse an exact open execution issue or create one bounded issue with objective, acceptance
   evidence, dependencies, authorization, and stop condition. The exact continuation directive is
   authorization for this one issue and its one pull request.
2. When resuming, reuse the issue, branch/worktree, plan, and PR already linked to the package.
   Otherwise create an isolated branch/worktree and add a plan under `docs/plans/` when `PLANS.md`
   requires one.
3. Route read-only specialists using `.codex/README.md`; the primary agent is the only writer and
   retains the gate, scope, and acceptance judgment.
4. Implement the package, update affected authorities, and run the strongest feasible automated and
   physical verification required by root and nested instructions. Never convert an unavailable
   capability into a pass.
5. Self-review, commit, and push. Open one review-ready PR linked to the issue only when one does not
   already exist, then monitor every required check. Diagnose and repair ordinary failures until CI
   passes or an external blocker is proven.
6. Before completion, update the issue and `PROGRAM_STATUS.md` with evidence, changed forecasts or
   risks, current execution state, and the next AI-owned action selected by the same rules.
7. End the cycle when this package reaches its PR, CI, and status boundary. Record the next candidate
   but do not begin it without another directive.

For registered NFF development boards, the directive permits repository-standard application flash
or OTA, temporary runtime configuration, reboot, and observation when the selected package requires
them. Resolve stable identity, capture pre-state, use the checked-in runbook, and restore temporary
state. It does not permit whole-flash or NVS erase, factory reset, eFuse, secure-boot,
flash-encryption or key provisioning, destructive electrical/battery testing, or host configuration.

The checked-in hardware CI is a narrow exception to the destructive-operation prohibition. Apply
`hw-test` only when the selected package requires it, the runner is online and idle, and exactly two
attached NFF CP2102N identities match the current registered board IDs in `PROGRAM_STATUS.md`. The
directive then authorizes that workflow's documented erase, factory-programming, OTA/recovery, and
forced-rollback sequence on those two boards. Do not reproduce those destructive steps manually or
apply the label when inventory cannot be proven.

The directive does not authorize merge, release, other labels, purchases, vendor or fabrication
commitments, other destructive device operations, work beyond the selected package, work barred by
the current gate, or unsupported product, safety, compliance, certification, or market claims. Stop
at that boundary and name the exact decision or capability required.

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
3. Current execution package, next AI-owned action, and its acceptance/stop boundary.
4. Immediate hardware authorization: definition, schematic, PCB layout, EVT, DVT, PVT or release.
5. `PS`, `FS`, `HW`, `VC`: delivered, now, next, owner, health and forecast.
6. Hardware release-ladder position and next evidence release.
7. Critical path and top risks with owner, mitigation and decision-by date.
8. CEO commitment decisions, team recommendation, alternatives and delay consequence.
9. Changes since the last review and any mismatch between ledger and observed evidence.

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
