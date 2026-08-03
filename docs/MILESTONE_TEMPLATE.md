# Milestone Contract Template

Use this template for every delivery milestone in
[`firmware/MILESTONES.md`](../firmware/MILESTONES.md). A milestone is a zero-duration decision point:
the required outcome has either been accepted or it has not. Work items belong in issues and pull
requests; this contract records what must be delivered and how acceptance is proven.

This is a thinking and review aid, not a formatting specification. Adapt the headings, prose, or
tables when that improves understanding. Preserve the underlying outcome, boundaries, evidence,
acceptance, ownership, current state, and next action.

## Status Model

Track lifecycle separately from health.

| Field | Allowed values | Meaning |
| --- | --- | --- |
| Status | `Proposed`, `Ready`, `In progress`, `Acceptance pending`, `Complete`, `Superseded` | Position in the acceptance lifecycle |
| Health | `On track`, `At risk`, `Blocked` | Confidence that the next gate can be reached as planned |
| Deliverable state | `Not started`, `In progress`, `Delivered`, `Accepted` | State of an inspectable output |
| Gate state | `Not run`, `Pass`, `Fail`, `Unverified`, `N/A` | State of acceptance evidence |

`Complete` requires all applicable gates to pass and a semantic audit of direct evidence from the
reviewed revision. The AI milestone manager owns acceptance and status transitions. Humans refine
the contract and may provide measurements or physical observations; those inputs are evidence, not
approval. Do not report percentage complete. Report accepted gates, remaining gates, and the next
unmet gate.

## Ready Checklist

A milestone may move from `Proposed` to `Ready` only when:

- the outcome is singular, valuable, and understandable without reading its task list;
- scope and exclusions prevent reasonable misinterpretation;
- dependencies, constraints, and owner are named;
- every deliverable is inspectable;
- every acceptance gate has a binary result, method, and required evidence;
- hardware, security, migration, compatibility, and human-observation requirements are explicit;
- evidence invalidation conditions are understood; and
- the milestone manager's semantic audit returns `Meets intent`.

## Recommended Delivery Dashboard

A compact derived dashboard above the milestone contracts is useful when it improves supervision.
Update its row in the same change as the owning milestone; it is an index, not a second source of
truth.

```markdown
| ID | Outcome | Status | Health | Accepted gates | Next gate | Last reviewed |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | <Short outcome> | `In progress` | `On track` | 3/7 | <Next unmet gate> | YYYY-MM-DD |
```

Prefer gate counts over estimated percentage complete. A stale review date, mismatch with the
detailed contract, or missing next gate is a status-quality defect regardless of presentation.

## Scope Heuristics

- Keep the number of deliverables and gates small enough for one coherent evidence review.
- Target one coherent validation campaign and one accountable outcome.
- Split unrelated outcomes or independently reviewable capabilities.
- Do not hide research uncertainty inside an implementation milestone. Time-box discovery first or
  define a decision deliverable with explicit evidence.
- Prefer objective thresholds over adjectives such as fast, robust, complete, seamless, or ready.

## Copyable Template

```markdown
### M<sequence>: <Outcome-oriented title>

**Outcome:** <One sentence describing the accepted user or system result.>

**Status:** `Proposed`
**Health:** `On track`
**Owner:** <One directly accountable individual or role>
**Last reviewed:** YYYY-MM-DD
**Depends on:** <Milestone IDs or None>
**Blocks:** <Milestone IDs or None>

#### Scope

**Included:**

- <Behavior, platform, environment, or operating envelope included>

**Excluded:**

- <Adjacent behavior that is explicitly not promised>

#### Constraints

1. <Architecture, compatibility, safety, resource, or process constraint>

#### Deliverables

| Deliverable | Required artifact or result | Evidence | State |
| --- | --- | --- | --- |
| <Inspectably named output> | <What must exist or work> | <PR, path, report, or pending> | `Not started` |

#### Acceptance Gates

| Gate | Required result | Verification method | Evidence | State |
| --- | --- | --- | --- | --- |
| <Binary gate name> | <Measurable threshold or exact behavior> | <Reproducible command or human procedure> | <Artifact, URL, revision, or pending> | `Not run` |

#### Current State

**Delivered:** <Accepted results, or None>

**Now:** <The one current focus, or Not started>

**Next:** <The next unmet gate and concrete action>

**Blockers:** <Blocking condition, owner, and resolution needed, or None>

**Decisions needed:** <Decision, decision owner, and needed-by point, or None>

#### Invalidation And Reopening

- Reopen when <source, hardware, dependency, requirement, or evidence change invalidates acceptance>.
```

## Status Review Output

Every review should answer, in order:

1. Which milestone is current, and what are its status and health?
2. What has been delivered and accepted, with evidence?
3. What is being done now?
4. What is the next unmet acceptance gate?
5. What is blocked or needs an owner decision?
6. Which milestone follows, and which dependency enables it?
7. Is any evidence stale or inconsistent with the implementation?

Consistency can improve scanning, but content takes priority over aesthetics. An AI audit must reason
about the evidence and intent even when a milestone uses different formatting.
