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

### Planner

Reads the pinned project brain and repository, then produces a bounded dependency DAG. It may
propose tasks but cannot make them dispatchable or modify governing specifications. Planner
contexts are disposable; recursive planning means a planner may propose narrower planning tickets,
not spawn an untracked conversational hierarchy.

### Worker

Owns one accepted ticket and isolated worktree. It implements only allowed surfaces, runs required
checks, publishes the scoped change when authorized by the ticket, and returns structured evidence.
It cannot change the governing specification, approve its own work, or activate follow-up tasks.

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

Workers may add proposed follow-ups to their result. Those remain inert until a planner or
requirements steward accepts and creates or transitions a ticket.

## Context hygiene

The durable state is limited to tracked authorities, issue state, commits, PRs, concise structured
results, and retained verification artifacts. Session transcripts, idle events, repeated status
updates, and partial reasoning are diagnostic data and never project-brain input.
