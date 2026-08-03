---
name: domes-milestone-manager
description: Create, sequence, audit, and update evidence-based DOMES delivery milestones. Use when defining milestone outcomes and acceptance gates, reporting project status or next steps, checking whether a milestone is ready or complete, reconciling MILESTONES.md with code, CI, documentation, architecture, CLI, firmware, or hardware evidence, or preparing a milestone for human sign-off.
---

# DOMES Milestone Manager

Treat a milestone as an outcome acceptance contract, not a task bucket. Keep
[`firmware/MILESTONES.md`](../../../firmware/MILESTONES.md) authoritative for delivery status and use
[`docs/MILESTONE_TEMPLATE.md`](../../../docs/MILESTONE_TEMPLATE.md) as guidance. Judge substance and
intent; do not require exact headings, tables, ordering, or phrasing.

## Select The Operation

- **Create or reorganize:** inventory current evidence, propose sequencing, and draft milestones.
- **Check status:** compare every claimed state with current evidence and report delivered, now, next,
  blockers, decisions, and sign-off needs.
- **Update status:** change the ledger only after verification supports the transition.
- **Audit quality:** reason through the milestone, challenge ambiguity, trace claims to evidence, and
  identify unsupported status or completion claims.

## Gather Evidence

1. Read root and applicable nested `AGENTS.md` files.
2. Read `firmware/MILESTONES.md`, `docs/TESTING.md`, `docs/README.md`, and the architecture documents
   relevant to the milestone.
3. Inspect the implementation, tests, CI workflows, open PR state, and retained hardware evidence.
4. Record the exact source revision and evidence date when behavior may have changed.
5. Treat missing, stale, indirect, or contradictory evidence as `Unverified`; never infer `Pass`.

Use the verification ladder in root `AGENTS.md`. Command acceptance is not physical confirmation,
host tests are not firmware integration, and a successful build is not hardware verification.

## Create A Milestone

1. State one externally meaningful outcome in one sentence.
2. Name one accountable owner and one human approver.
3. Bound included and excluded behavior explicitly.
4. List hard constraints and dependencies before deliverables.
5. Keep independently inspectable deliverables and acceptance gates few enough that a human can
   understand the outcome and sign-off decision in one review.
6. Write each gate as a binary required result plus a reproducible verification method.
7. Identify the evidence artifact each gate must produce.
8. Define invalidation and reopening conditions.
9. Sequence it after every dependency needed to execute its acceptance gates.
10. Leave it `Proposed` until a human approves scope and acceptance criteria.

Split a milestone when it contains unrelated outcomes, multiple independent sign-offs, an unbounded
research question, or gates that cannot be evaluated in the same validation campaign.

## Check Or Update Status

Keep lifecycle and delivery confidence separate:

- `Status`: `Proposed`, `Ready`, `In progress`, `Acceptance pending`, `Complete`, or `Superseded`.
- `Health`: `On track`, `At risk`, or `Blocked`.

Apply transitions conservatively:

| Transition | Required evidence |
| --- | --- |
| `Proposed` to `Ready` | Outcome, scope, constraints, dependencies, gates, owner, and approver accepted |
| `Ready` to `In progress` | Dependency gates satisfied and implementation work started |
| `In progress` to `Acceptance pending` | Deliverables present and every non-human gate passes on the reviewed revision |
| `Acceptance pending` to `Complete` | Required human sign-off recorded; no failed, unverified, or stale gate remains |
| `Complete` to `In progress` | A listed invalidation condition occurs; record why the milestone reopened |

Do not use percentages. Report progress as accepted gates over total gates and name the next unmet
gate. An AI may recommend `Acceptance pending`; it must not fabricate or grant human approval.

For a status request, return this order:

1. Current milestone, lifecycle status, health, and evidence freshness.
2. Delivered and accepted results.
3. Current work and the next unmet gate.
4. Blockers and decisions needing an owner.
5. Next milestone and why it follows.
6. Any mismatch between the ledger and observed evidence.

## Audit Quality

Perform a semantic review. Formatting differences are irrelevant when the content is clear; a
well-formatted milestone still fails when its meaning or evidence is weak.

For each milestone, answer:

1. **Outcome:** Is there one valuable end state, or merely a list of activity?
2. **Boundary:** Can a reasonable reader tell what is and is not promised?
3. **Causality:** Do the deliverables actually produce the stated outcome?
4. **Acceptance:** Would two independent reviewers reach the same pass/fail decision?
5. **Evidence:** Does each material claim have direct, current evidence for the reviewed revision and
   required environment?
6. **Coverage:** Are software, firmware, protocol, CLI, CI, documentation, and hardware implications
   included where relevant?
7. **Sequence:** Are prerequisites complete or explicitly blocking the milestone, and does the next
   milestone logically follow?
8. **Ownership:** Is one person or role accountable for resolving ambiguity and obtaining sign-off?
9. **Truthfulness:** Are targets separated from observations, and are limitations visible rather than
   softened by wording?
10. **Closure:** Does `Complete` mean the outcome is accepted, not merely that code exists or a command
    ran?
11. **Durability:** Are the conditions that make old evidence stale or reopen the milestone understood?
12. **Supervision:** Can a reader immediately identify delivered results, current work, next action,
    blockers, decisions, and required sign-off?

Return one of `Meets intent`, `Needs revision`, or `Not verifiable` for each milestone, followed by
specific evidence and the smallest changes needed. Explicitly state uncertainty and conflicting
evidence. Do not reward template compliance, count headings, or rewrite historical evidence merely to
make the document look consistent.

## Preserve The Ledger

- Keep current status in `firmware/MILESTONES.md`; link to evidence instead of duplicating it.
- Keep its delivery dashboard synchronized with the detailed milestone contracts.
- Keep detailed test procedures in `docs/TESTING.md`.
- Keep product targets in `research/SYSTEM_ARCHITECTURE.md`.
- Keep as-built boundaries in `research/SOFTWARE_ARCHITECTURE.md` and `research/architecture/`.
- Put implementation tasks in issues or PRs; milestones contain outcomes, gates, and status only.
- Update `Last reviewed` whenever status, health, evidence, blockers, or next action changes.
