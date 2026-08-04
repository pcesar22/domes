# Program Phase, Gate, And Work-Package Template

Use this as a semantic checklist with
[`PRODUCT_REALIZATION_FRAMEWORK.md`](PRODUCT_REALIZATION_FRAMEWORK.md). Adapt the presentation when
clarity improves; preserve the underlying decision, boundaries, evidence, ownership, timing, and
invalidation rules.

## Choose The Correct Object

| If the subject is | Use | Do not call it |
| --- | --- | --- |
| Cross-functional work between two decisions | Program phase `P#` | Firmware or hardware milestone |
| A decision authorizing definition, schematic, build, spend, freeze, or release | Decision gate `G#` | Multi-month phase |
| Discipline delivery | Workstream outcome `PS#`, `FS#`, `HW#`, `VC#` | Company phase |
| A bounded assignment | Work package `<stream>-WP-###` | Gate |
| A technical hardware design release | Hardware release `HR#` | Product authorization |

Future phase status is `Not entered` and health is `Not rated`. Do not report normal dependency
sequencing as `Blocked`.

## Program Phase Contract

```markdown
### P#: <Cross-functional phase name>

**Outcome:** <System/product result at the exit gate>
**Status:** `Not entered` | `Ready` | `Active` | `Gate review` | `Closed` | `Superseded`
**Health:** `Green` | `Amber` | `Red` | `Not rated`
**Entry gate:** G#
**Exit gate:** G#
**Baseline:** YYYY-MM-DD to YYYY-MM-DD
**Forecast:** YYYY-MM-DD to YYYY-MM-DD
**Confidence:** High | Medium | Low with reason

| Workstream | Required phase outcome | Owner | Evidence | Status / health |
| --- | --- | --- | --- | --- |
| PS | <Product/System outcome> | <Role> | <Direct result> | <State> |
| FS | <Firmware/Software/Simulation outcome> | <Role> | <Direct result> | <State> |
| HW | <Hardware/NPI outcome> | <Role> | <Direct result> | <State> |
| VC | <Verification/Compliance outcome> | <Role> | <Direct result> | <State> |

**Critical path:** <Dated dependency chain controlling exit>
**Resources/lead times:** <People, supplier, lab, CM, budget assumptions>
**Top risks:** <Owner, consequence, mitigation, decision-by date>
**Invalidation:** <Change that reopens phase evidence>
```

## Decision Gate Record

```markdown
### G#: <Decision name>

**Decision enabled:** <Exact commitment; be specific about definition, schematic, layout, EVT, DVT, PVT, or release>
**Engineering owner:** <Named accountable role/person for the controlled package>
**Audited package/revision:** <Immutable identifier>
**Baseline / forecast:** YYYY-MM-DD / YYYY-MM-DD
**Evidence date:** YYYY-MM-DD

| Critical criterion | Required binary result | Verification method | Evidence | State |
| --- | --- | --- | --- | --- |
| <Criterion> | <Threshold/behavior> | Test/Analysis/Inspection/Demonstration | <Artifact> | `Not due`/`Not run`/`Pass`/`Fail`/`Waived`/`Invalidated` |

**AI technical gate verdict:** `Go` | `Conditional Go` | `Hold` | `Recycle` | `Stop`
**Evidence status transition:** <Phase/release/package state recorded by AI>
**Technical authorization:** <What technical work may start and what remains prohibited>
**CEO commitment authorization:** <Budget/vendor/market action, owner and date, or Not required>
**Exceptions:** <Consequence, owner, closure date, or None>
**Invalidation:** <Configuration/evidence change reopening the gate>
```

`Conditional Go` is invalid when an exception can change topology, selected critical parts,
interfaces, safety, compliance route, PCB outline/stack-up, placement, firmware architecture, or the
economic basis of the commitment.

## Work-Package Contract

```markdown
### <Stream>-WP-###: <Bounded assignment>

**Objective:** <One inspectable result>
**Owner:** <Role/person>
**State / health:** <Work-package state> / <Health>
**Requested start / finish:** YYYY-MM-DD / YYYY-MM-DD
**Inputs:** <Required baselines/evidence>
**Dependencies/blockers:** <Predecessors, external actions, or None>
**Gate/risk unlocked:** <Named gate input, downstream package, or risk retired>
**Execution authority:** AI-owned | CEO/external-owned | Mixed with explicit boundary
**Authorization:** <Permitted work>
**Stop condition:** <Work or spend explicitly prohibited>

| Deliverable | Acceptance result | Evidence | State |
| --- | --- | --- | --- |
| <Output> | <Binary result> | <Artifact/procedure> | <State> |

**Now:** <One current result>
**Next:** <Concrete evidence-changing action>
**Selected next / rationale:** <Yes/No and semantic priority reason>
**Risks/decisions:** <Owner and due date>
**Invalidation:** <Change reopening acceptance>
```

## CEO Status Output

Every review reports:

1. active program phase, NPI stage, evidence revision/date, health, baseline/forecast and confidence;
2. next gate, exact authorization, critical evidence, AI technical verdict and any separate CEO
   commitment decision;
3. current execution package, separate next program action and autonomous execution delivery,
   acceptance boundary, blocker and linked issue;
4. current hardware authorization level and next hardware release: definition, schematic, layout,
   EVT, DVT, PVT, or release;
5. each workstream's delivered, now, next, owner, health and forecast;
6. critical path, resources/lead times and top risks;
7. CEO decisions with recommendation, alternatives, needed-by date and consequence of delay; and
8. scope, schedule, cost, requirement, configuration, evidence and risk changes since last review.

Do not report percentage complete. Do not mix phase status, gate disposition, work-package status,
health, and hardware release state.
