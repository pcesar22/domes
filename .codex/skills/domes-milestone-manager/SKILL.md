---
name: domes-milestone-manager
description: Create, sequence, audit, accept, and update evidence-based DOMES delivery milestones. Use when defining milestone outcomes and acceptance gates, reporting project status or next steps, checking whether a milestone is ready or complete, reconciling MILESTONES.md with code, CI, documentation, architecture, CLI, firmware, or hardware evidence, or deciding a milestone status transition.
---

# DOMES Milestone Manager

Treat a milestone as an outcome acceptance contract, not a task bucket. Keep
[`firmware/MILESTONES.md`](../../../firmware/MILESTONES.md) authoritative for delivery status and use
[`docs/MILESTONE_TEMPLATE.md`](../../../docs/MILESTONE_TEMPLATE.md) as guidance. Judge substance and
intent; do not require exact headings, tables, ordering, or phrasing.

For product-realization work, read
[`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](../../../docs/PRODUCT_REALIZATION_FRAMEWORK.md) first. It
defines the single active phase, entry and exit semantics, and required status report.

## Select The Operation

- **Create or reorganize:** inventory current evidence, propose sequencing, and draft milestones.
- **Check status:** compare every claimed state with current evidence and report delivered, now, next,
  blockers, decisions, and the acceptance decision.
- **Update status:** change the ledger only after verification supports the transition.
- **Audit quality:** reason through the milestone, challenge ambiguity, trace claims to evidence, and
  identify unsupported status or completion claims.

## Gather Evidence

1. Read root and applicable nested `AGENTS.md` files.
2. Read `firmware/MILESTONES.md`, `docs/TESTING.md`, `docs/README.md`, and the architecture documents
   relevant to the milestone.
3. Read `research/PRODUCT_DEFINITION.md` when product scope, customer value, requirements, launch,
   form factor, compliance, economics, or open-source readiness affects the decision.
4. Inspect the implementation, tests, CI workflows, open PR state, and retained hardware evidence.
5. Record the exact source revision and evidence date when behavior may have changed.
6. Treat missing, stale, indirect, or contradictory evidence as `Unverified`; never infer `Pass`.

Use the verification ladder in root `AGENTS.md`. Command acceptance is not physical confirmation,
host tests are not firmware integration, and a successful build is not hardware verification.

## Create A Milestone

1. State one externally meaningful outcome in one sentence.
2. Name one accountable owner.
3. Bound included and excluded behavior explicitly.
4. List hard constraints and dependencies before deliverables.
5. Keep independently inspectable deliverables and acceptance gates few enough for one coherent
   evidence review.
6. Write each gate as a binary required result plus a reproducible verification method.
7. Identify the evidence artifact each gate must produce.
8. Define invalidation and reopening conditions.
9. Define a binary entry gate, then sequence it after every dependency needed to execute its exit
   gates.
10. Leave it `Proposed` until the milestone manager's semantic audit returns `Meets intent`.

For lifecycle phases, permit only one `In progress` or `Acceptance pending` phase. Later-phase work
may retire a named risk, but cannot be reported as phase execution before entry passes.

Split a milestone when it contains unrelated outcomes, independently reviewable capabilities, an unbounded
research question, or gates that cannot be evaluated in the same validation campaign.

## Check Or Update Status

Keep lifecycle and delivery confidence separate:

- `Status`: `Proposed`, `Ready`, `In progress`, `Acceptance pending`, `Complete`, or `Superseded`.
- `Health`: `On track`, `At risk`, or `Blocked`.

Apply transitions conservatively:

| Transition | Required evidence |
| --- | --- |
| `Proposed` to `Ready` | Semantic audit returns `Meets intent`; outcome, boundaries, dependencies, entry, exit gates, and owner are clear; every entry condition has current `Pass` evidence |
| `Ready` to `In progress` | Owner, reviewed revision, start date, and first exit-gate action are recorded |
| `In progress` to `Acceptance pending` | Deliverables present and applicable gates appear satisfied; final evidence audit remains |
| `Acceptance pending` to `Complete` | Milestone manager audits direct current evidence and finds no failed, unverified, contradictory, or stale gate |
| `Complete` to `In progress` | A listed invalidation condition occurs; record why the milestone reopened |

Do not use percentages. Report progress as accepted gates over total gates and name the next unmet
gate. The milestone manager owns acceptance and status transitions. Humans refine the contract and
may provide measurements or physical observations; those inputs are evidence, not approval. Record
the reasoning and evidence for every transition, and refuse a transition when the evidence is weak.

For a status request, return this order:

1. Active phase, lifecycle status, health, actual start, forecast exit, reviewed revision, and
   evidence date.
2. Entry result and phase start state.
3. Exit gates passed over total, including failed or unverified gates.
4. Delivered and accepted results.
5. One current gate and owner, then the next concrete action.
6. Risks and decisions with owner and consequence.
7. Following phase and the exact unmet entry condition.
8. Any mismatch between the ledger and observed evidence.

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
8. **Ownership:** Is one person or role accountable for resolving ambiguity and producing evidence?
9. **Truthfulness:** Are targets separated from observations, and are limitations visible rather than
   softened by wording?
10. **Closure:** Does `Complete` mean the outcome is accepted, not merely that code exists or a command
    ran?
11. **Durability:** Are the conditions that make old evidence stale or reopen the milestone understood?
12. **Supervision:** Can a reader immediately identify delivered results, current work, next action,
    blockers, decisions, and acceptance state?

Return one of `Meets intent`, `Needs revision`, or `Not verifiable` for each milestone, followed by
specific evidence and the smallest changes needed. Explicitly state uncertainty and conflicting
evidence. Do not reward template compliance, count headings, or rewrite historical evidence merely to
make the document look consistent.

## Preserve The Ledger

- Keep current status in `firmware/MILESTONES.md`; link to evidence instead of duplicating it.
- Keep lifecycle rules in `docs/PRODUCT_REALIZATION_FRAMEWORK.md` and product hypotheses in
  `research/PRODUCT_DEFINITION.md`; do not turn the ledger into a second copy.
- Keep its delivery dashboard synchronized with the detailed milestone contracts.
- Keep detailed test procedures in `docs/TESTING.md`.
- Keep product targets in `research/SYSTEM_ARCHITECTURE.md`.
- Keep as-built boundaries in `research/SOFTWARE_ARCHITECTURE.md` and `research/architecture/`.
- Put implementation tasks in issues or PRs; milestones contain outcomes, gates, and status only.
- Update `Last reviewed` whenever status, health, evidence, blockers, or next action changes.
