---
schema_version: 1
tracker_kind: github
repository: pcesar22/domes
tracker_actor: pcesar22
state_prefix: agent:
scheduler_host: ministrom
max_concurrent_workers: 4
max_open_pull_requests: 6
workspace_root: .worktrees/agents
base_branch: main
poll_interval_seconds: 30
stall_timeout_seconds: 1800
max_retry_backoff_seconds: 600
---

# DOMES Agent Workflow

This file is the versioned policy consumed by the DOMES Symphony-compatible control plane. The
control plane is a deterministic CLI; it is not a product manager, implementer, reviewer, service,
or source of product truth.

## Governing context

Before acting, every role reads the ticket's pinned specification revision and the authority map in
[`docs/agent-system/README.md`](docs/agent-system/README.md). A ticket without a resolvable full Git
commit in `Specification revision` is not dispatchable. Later changes to `main` do not silently
change that ticket's acceptance contract.

## Tracker state machine

Every managed issue has exactly one state label:

```text
agent:needs-specification
  -> agent:plan
  -> agent:plan-review
  -> agent:ready
  -> agent:running
  -> agent:agent-review
  -> agent:rework -> agent:agent-review
  -> agent:ci-pending
  -> agent:verification -> agent:agent-review
  -> agent:human-review or agent:done
```

`agent:blocked` may be entered from any active state. Only a requirements steward or human may move
work out of `agent:needs-specification`. Only an accepted plan may enter `agent:ready`. Only an
independent judge verdict may move implementation from `agent:agent-review` to
`agent:ci-pending`. CI failure may enter `agent:verification`; every repair returns through a fresh
judge. Versioned `software-review-required` policy authorizes autonomous implementation and CI
repair only. Every pull request stops at `agent:human-review`; only a human may approve or merge it.
After observing a human merge, the controller performs issue bookkeeping and unlocks ordinary
dependencies. A narrowly bounded software child may start earlier only when its accepted contract
explicitly requires code from one unmerged direct dependency whose exact head is independently
judged, CI-passing, and in `agent:human-review`. Independent work starts from `main`. Release
remains outside this workflow.

## Dispatch policy

The scheduler performs only these actions:

1. Read active issues and their state labels.
2. Validate the ticket contract and pinned specification revision.
3. Reject implementation issues with unresolved dependencies or overlapping active workspaces. A
   planner may run ahead of its nonterminal declared dependencies because it is read-only; every
   materialized child inherits those external prerequisites. The only implementation exception is
   an automated software child whose contract explicitly requires an unmerged direct dependency,
   has no hardware operations, and targets one stable `agent:human-review` parent. Independent
   work and dependency joins that can wait for merged inputs target `main`.
4. Repair failed CI first, then sort remaining eligible issues by numeric priority and issue
   number.
5. Reserve up to three slots and create one controller-owned standalone Git workspace per issue.
6. Move a newly accepted implementation task to `agent:running` and launch a fresh role-specific
   run with the matching output schema; rejected work retains `agent:rework` so its judge handoff is
   never lost across a restart.
7. Validate the final structured result and translate it into an explicit tracker transition.
8. Retry transient process failures with bounded exponential backoff.
9. Admit at most six open pull requests repository-wide. Existing pull requests may continue
   through repair, review, CI, and human review while the cap is full, but no worker that would
   create another pull request and no selector may start until capacity exists. Active new-PR
   workers reserve capacity before launch so concurrent workers cannot overshoot the cap.
10. In explicit autopilot mode, reserve any otherwise-free slot for one disposable requirements
   steward even while other roles, CI, or human review are active. Repeated selections fill free
   capacity with milestone-authorized contracts, and accepted planner DAGs materialize
   idempotently. Only one selector may run at once.
11. Poll required CI without spending an agent slot, repair failures through a fresh worker and
    judge cycle, then place the exact passing head in `agent:human-review` without approving or
    merging it.

For the dependency-only stack exception, the controller—not a worker narrative—validates every ancestor's
issue, pull request, branch, exact reviewed head, independent approval, and required CI before it
binds a child to the immediate live parent. It creates the child branch from that head and requires
the child pull request to target the parent branch. Fan-in, cycles, unstable or changed ancestors,
requested changes, conflicts, and hardware-executing children fail closed. When a parent merges
into its own parent, an open child is rebuilt and retargeted to the next live ancestor; no earlier
child approval survives that base transition. If a human explicitly merges a review-ready child
into its exact parent branch first, the child remains nonterminal while the controller follows the
validated ancestor chain and proves that the integration commit ultimately reached `main`. If any
ancestor drops or fails to land that commit, the merged artifact is blocked without rewriting its
acceptance contract; a fresh steward-approved delivery is required while the selector continues
other work.

Every worker and verification-worker Codex process remains workspace-write and has no direct device
access. Hardware execution requires all four of: an explicit finite `Hardware operations` enum and
explicit `Hardware boards` aliases in the accepted ticket, the operator's
`--allow-registered-hardware` startup opt-in, and a passing host preflight for the exact registered
NFF CP2102N identities. The controller then holds one exclusive device lease and starts a host-side
broker. The worker receives only a ticket-bound queue capability;
the broker maps board aliases to private device paths, revalidates the udev identity immediately
before every operation, requires a committed tracked-clean worktree, executes fixed allowlisted
commands, and retains a hash-chained, commit-bound manifest with the ticket checkpoint. Flash is
compiled by the broker from a private clean clone with pinned ESP-IDF v5.4.4, restricted to the
standard DOMES application layout, and cannot write NVS, PHY, or OTA-data partitions. OTA likewise
uses the broker-built application image. It cannot consume a worker build directory or execute
worker-supplied argv.

Each agent workspace is a standalone clone with private Git metadata inside the workspace-write
sandbox. The scheduler does not reuse operator worktrees and never grants workers write access to
the source repository's shared `.git` directory. The worker command explicitly grants write access
only to its clone-local `.git`, which Codex otherwise protects even below a writable root.

If preflight is unavailable, only that hardware ticket enters `agent:blocked`. Automatic recovery
requires a new successful preflight and a typed blocker bound to the same issue, specification, and
PR head; prose or an old worker summary can never trigger requeue.

It never changes product intent, weakens acceptance checks, reads raw transcripts into another
role's prompt, or treats a successful process exit as acceptance. The disposable selector may only
translate existing milestones and live execution state into one bounded software or executed-
validation contract; its output is revalidated against fresh main, issue, and pull-request state
before any tracker mutation.

Every autonomous pull request must use a plain-language title and a fully populated CEO-first
description. The standalone word `gate` is prohibited in both because it hides the actual
prerequisite or decision. The controller rejects incomplete summaries, unexplained internal work
package codes in titles, template placeholders, and descriptions that omit the outcome, why it
matters, approval boundary, next action, or verification boundary.

The executive summary uses four short labeled bullets: `Problem`, `Change`, `Result`, and
`User impact`. Technical terminology belongs in the collapsed appendix. Autonomous pull requests
may not commit raw logs, per-run result streams, traces, binary captures, or repeated generated
campaign output. The controller limits tracked evidence to 12 files and 1,000 changed lines, and
limits the complete pull request to 120 changed files. Raw evidence remains in ignored workspace
storage or controller-private state. The complete pull request is also limited to 5,000 changed
lines. A pull request may retain one small aggregate report when its acceptance contract requires a
durable repository result.

## Role routing

| Ticket state | Role | Successful handoff |
| --- | --- | --- |
| `agent:plan` | planner | materialized execute/recursive-plan DAG or explicit project blocker |
| `agent:ready` | worker | commit, PR, evidence, and proposed follow-ups for agent review |
| `agent:agent-review` | judge | approve to CI or reject against the original contract |
| `agent:ci-pending` | controller | exact-head required-check reconciliation |
| `agent:verification` | verification worker | bounded repair of failed CI |
| `agent:human-review` | human | review and merge, while the controller continues separate work |

Interactive requirements stewardship remains the only authority for changing product intent. In
explicit autopilot mode a fresh selector continuously fills otherwise-free capacity from
already-authorized milestones; it cannot edit governing documents or perform implementation.
Planner process/schema failures remain retryable controller failures with bounded backoff rather
than being mislabeled as external project blockers.

Exactly one scheduler host is supported. `scheduler_host` pins mutation-capable runs to the reviewed
machine, and the local advisory lock permits only one scheduler process there. Changing hosts is a
versioned workflow-policy change, not an automatic failover. Stalled child runs are restarted by
the owning scheduler. A persistent process-group lease and pre-execution gate prevent an orphaned
Codex process from overlapping a replacement after scheduler restart. This version does not provide
multi-host high availability.

## Information boundary

Planners and judges receive the governing revision, ticket contract, repository state, diff, and
schema-validated evidence appropriate to their role. They do not receive worker JSONL,
chain-of-thought, idle notifications, acknowledgements, or an implementer's persuasive completion
narrative. Runtime logs exist only for operator diagnosis. Durable handoffs recovered from GitHub
comments are accepted only when GitHub attributes the comment to the version-pinned
`tracker_actor`; lookalike role markers from any other commenter are inert.

## Completion

An agent's final message has no lifecycle authority. Review readiness requires a valid structured
result, independent agent judgment, the corresponding tracker transition, and required exact-head
CI. Every automated ticket stops at `agent:human-review`; only an observed human merge moves it to
`agent:done`. The independent judge never submits a GitHub approval. Physical-device claims still
require the evidence defined by `docs/TESTING.md`.

## Autonomous authority boundary

`--autopilot` records the user's standing authorization for routine software delivery. It does not
authorize product-requirement or architecture changes, policy/self-modification, dependency or
security-policy changes, releases, purchases, vendors, fabrication, destructive device actions, or
`hw-test`. Registered hardware requires the separate controller-startup
`--allow-registered-hardware` opt-in and an explicit ticket operation list; it never authorizes
`hw-test`, erase/NVS/factory reset, eFuse, secure-boot, encryption, key, release, or arbitrary host
commands. These surfaces fail closed under
`.codex/orchestration/autopilot-policy.json`. A blocked package does not prevent the selector from
choosing a separate eligible milestone delivery.
