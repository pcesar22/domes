# DOMES Agent System

This index is the repository-native project brain for disposable agent sessions. It points to the
owning source for each kind of truth instead of copying facts into a giant instruction file.

## Authority map

| Question | Read this authority |
| --- | --- |
| What product are we building? | [`research/PRODUCT_DEFINITION.md`](../../research/PRODUCT_DEFINITION.md) |
| What is built now? | Current code, generated artifacts, and tests; then [`research/SOFTWARE_ARCHITECTURE.md`](../../research/SOFTWARE_ARCHITECTURE.md) |
| What is the target system? | [`research/SYSTEM_ARCHITECTURE.md`](../../research/SYSTEM_ARCHITECTURE.md) |
| What is the current program state? | [`PROGRAM_STATUS.md`](../../PROGRAM_STATUS.md) |
| What decisions and proposals exist? | [`research/architecture/README.md`](../../research/architecture/README.md) |
| What work is active and resumable? | [`docs/plans/`](../plans/) and the governing GitHub issue |
| What proof is required? | [`docs/TESTING.md`](../TESTING.md) and nearest component `AGENTS.md` |
| How is agent work dispatched and accepted? | [`WORKFLOW.md`](../../WORKFLOW.md) |

The pinned Git commit named by a ticket's `Specification revision` selects the governing version of
these documents. Implementation evidence is evaluated against that revision even when a newer
revision exists.

## Separation of duties

### Requirements steward

Owns discussion with the user, product intent, accepted ambiguity resolutions, architectural
decisions, and authorization of planning tickets. It may edit governing documents. It does not
implement tickets, supervise sessions, read raw worker transcripts, resolve routine conflicts, or
rewrite requirements to fit an implementation.

In explicit autopilot mode, a fresh read-only selector performs the narrower steward function of
choosing one already-authorized software or executed-validation milestone. Its structured contract
is disposable, mechanically validated, and cannot modify the project brain.

### Planner

Reads the pinned project brain and repository, then produces a bounded dependency DAG. It may
propose tasks but cannot make them dispatchable or modify governing specifications. Every proposed
task declares a mode: `execute` creates a worker-ready child, while `plan` creates a narrower
planning child. Planner contexts are disposable; recursive planning is therefore an explicit,
tracker-backed sequence of bounded planning tickets, not an untracked conversational hierarchy.

### Worker

Owns one accepted ticket and isolated controller-owned Git workspace. It implements only allowed
surfaces, runs required checks, publishes the scoped change when authorized by the ticket, and
returns structured evidence. Its private Git metadata is inside its sandbox; it cannot write another
workspace's Git state. It cannot change the governing specification, approve its own work, or
activate follow-up tasks.

### Judge

Starts with fresh context. It receives the pinned specification, ticket contract, actual diff, and
verification artifacts—not the worker transcript. It returns `approve`, `reject`, or `blocked` with
criterion-level evidence. Approval cannot waive an acceptance check or turn software evidence into
a physical claim.

### Verification worker

Watches required CI and performs bounded repairs against judge-approved intent. A repair that
materially changes behavior returns the ticket to independent agent review. It cannot merge,
release, or add `hw-test` without the authorization already required by `AGENTS.md`.

## Ticket contract

Every dispatchable ticket contains these headings with non-empty values:

- `Specification revision` — a full 40-character commit SHA reachable in this repository.
- `Parent objective`
- `Goal`
- `Non-goals`
- `Required behavior`
- `Acceptance checks`
- `Allowed architectural surfaces` — one repository-relative path or glob per line; worker diffs
  outside these surfaces fail closed before agent review.
- `Dependencies` — `None` or issue references such as `#123`.
- `Required proof`
- `Hardware operations` — `None` or the finite broker operation names required by this task. This
  field is bound into the controller contract digest; narrative references to hardware grant no
  device access.
- `Hardware boards` — `None` without hardware, otherwise the exact registered broker aliases
  authorized for this task. This field is also contract-digest-bound.

Workers may add proposed follow-ups to their result. Those remain inert until a planner or
requirements steward accepts and creates or transitions a ticket.

Autonomous tickets also contain `Autonomy policy`, `Work package`, and controller markers bound to
the pinned specification. `software-review-required` permits autonomous implementation, PR
publication, independent agent judgment, and CI repair. It never permits GitHub approval or merge;
those remain human actions. Planner children inherit that policy and remain blocked on their parent
until the complete DAG has been materialized. Planner execution itself may run ahead of nonterminal
dependencies because it is read-only; the controller copies the planning ticket's external
dependencies onto every materialized child so implementation remains gated. A planner child
explicitly marked `plan` receives a fresh planner context; it may only decompose its inherited
bounded objective and cannot expand the parent's accepted surfaces, hardware operations, or
autonomy policy.

An implementation dependency does not globally idle delivery while a human reviews its parent.
The controller may execute one automated software child with no hardware operations on the exact
reviewed head of its sole nonterminal `agent:human-review` parent. The child PR targets the parent
branch, forming one explicit stack level. The controller rejects fan-in, nested stacks, unstable or
changed parent artifacts, and any stacked child that needs hardware. A parent change or merge
invalidates the child's prior worker, judge, and CI evidence; the child must be regenerated against
the new exact base before returning to human review. A human may instead merge an already
review-ready child into the exact parent branch. That child remains nonterminal until the
controller proves its integration commit reached `main`; a dropped or abandoned integration is
blocked without rewriting the governing contract and requires a fresh steward-approved delivery.

Even with a non-empty hardware operation list, Codex remains workspace-write with no direct device
access. Dispatch also requires explicit ticketed board aliases, the controller's
`--allow-registered-hardware` opt-in, and an exact
registered-board preflight. A host broker maps opaque board aliases to private device paths,
revalidates identity for every operation, binds its evidence to the committed worktree HEAD, and
accepts no worker-provided command line or firmware build. Flash and OTA use a private clean clone
and a controller-pinned ESP-IDF v5.4.4 build.

## Context hygiene

The durable state is limited to tracked authorities, issue state, commits, PRs, concise structured
results, and retained verification artifacts. Session transcripts, idle events, repeated status
updates, and partial reasoning are diagnostic data and never project-brain input.
