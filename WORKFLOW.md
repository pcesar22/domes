---
schema_version: 1
tracker_kind: github
repository: pcesar22/domes
state_prefix: agent:
scheduler_host: ministrom
max_concurrent_workers: 3
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
After observing a human merge, the controller performs issue bookkeeping and unlocks dependencies.
Release remains outside this workflow.

## Dispatch policy

The scheduler performs only these actions:

1. Read active issues and their state labels.
2. Validate the ticket contract and pinned specification revision.
3. Reject issues with unresolved dependencies or overlapping active workspaces.
4. Sort eligible issues by numeric priority and then issue number.
5. Reserve up to three slots and create one isolated worktree per issue.
6. Move a newly accepted implementation task to `agent:running` and launch a fresh role-specific
   run with the matching output schema; rejected work retains `agent:rework` so its judge handoff is
   never lost across a restart.
7. Validate the final structured result and translate it into an explicit tracker transition.
8. Retry transient process failures with bounded exponential backoff.
9. In explicit autopilot mode, invoke one disposable requirements steward only when no role is
   runnable, adopt one milestone-authorized execution contract, and materialize accepted planner
   DAGs idempotently.
10. Poll required CI without spending an agent slot, repair failures through a fresh worker and
    judge cycle, then place the exact passing head in `agent:human-review` without approving or
    merging it.

It never changes product intent, weakens acceptance checks, reads raw transcripts into another
role's prompt, or treats a successful process exit as acceptance. The disposable selector may only
translate existing milestones and live execution state into one bounded software or executed-
validation contract; its output is schema- and policy-validated before any tracker mutation.

## Role routing

| Ticket state | Role | Successful handoff |
| --- | --- | --- |
| `agent:plan` | planner | materialized accepted DAG or blocked parent |
| `agent:ready` | worker | commit, PR, evidence, and proposed follow-ups for agent review |
| `agent:agent-review` | judge | approve to CI or reject against the original contract |
| `agent:ci-pending` | controller | exact-head required-check reconciliation |
| `agent:verification` | verification worker | bounded repair of failed CI |
| `agent:human-review` | human | review and merge, while the controller continues separate work |

Interactive requirements stewardship remains the only authority for changing product intent. In
explicit autopilot mode a fresh selector may choose from already-authorized milestones when the
execution queue is empty; it cannot edit governing documents or perform implementation.

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
narrative. Runtime logs exist only for operator diagnosis.

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
`hw-test`. These surfaces fail closed under `.codex/orchestration/autopilot-policy.json`. A blocked
package does not prevent the selector from choosing a separate eligible milestone delivery.
