# DOMES Agent Control Plane

This standard-library Python tool implements the repository-facing portion of OpenAI Symphony's
[language-agnostic contract](https://github.com/openai/symphony/blob/main/SPEC.md) for DOMES. It
reads GitHub issues, validates the task DAG, creates isolated worktrees, invokes fresh Codex roles
with JSON-schema outputs, and performs explicit state transitions. It does not require or install a
service manager.

Validate the checked-in contracts:

```bash
python3 tools/agent_control/control.py validate
```

Inspect live GitHub eligibility without changing anything:

```bash
python3 tools/agent_control/control.py queue --live
```

Run one bounded scheduling cycle only after reviewing the queue:

```bash
python3 tools/agent_control/control.py run --execute
```

Keep the explicit process polling and refilling available slots until interrupted:

```bash
python3 tools/agent_control/control.py run --execute --watch
```

Run the closed-loop milestone selector, accepted-plan materializer, CI reconciler, human-review
handoff, and continuous slot refill:

```bash
python3 tools/agent_control/control.py run --execute --watch --autopilot
```

To let accepted tickets request registered-NFF evidence, opt in when starting the controller:

```bash
python3 tools/agent_control/control.py run --execute --watch --autopilot --allow-registered-hardware
```

This is not a general hardware permission. A ticket must also contain an explicit, finite
`Hardware operations` section and explicit `Hardware boards` aliases. Before dispatch, the controller verifies the two registered NFF
CP2102N identities, takes an exclusive hardware lease, and starts a host-side broker. Codex remains
in the workspace-write sandbox with no direct `/dev` access; it receives only a temporary
ticket/specification-bound queue capability and only the ticketed board aliases. The broker revalidates the
private udev mapping immediately before each request and executes fixed argv for only the ticketed
operation. It requires a committed tracked-clean worktree and binds each manifest event to that
commit. Flash and OTA images are built by the host broker from a private clean clone with ESP-IDF
v5.4.4; worker-supplied build artifacts are rejected. It retains device evidence under the
ticket's controller state.

A failed preflight blocks only that ticket. It is automatically requeued only when a later
preflight succeeds and the saved typed blocker still matches the same issue, specification, and PR
head. Old comments and prose are not recovery authority.

For a live tmux/operator view instead of machine-readable JSON snapshots:

```bash
python3 tools/agent_control/control.py run --execute --watch --autopilot --dashboard
```

The dashboard refreshes every poll and shows active roles, CI state, review-ready PRs, blockers, and
the selector's latest action. It never displays raw worker transcripts; detailed JSONL remains only
in the runtime state directory.

`run` is deliberately explicit because it creates worktrees, launches mutation-capable workers,
and changes GitHub issue labels/comments. Mutation-capable runs are pinned to the reviewed
`scheduler_host` in `WORKFLOW.md`, and a non-blocking advisory file lock prevents two scheduler
processes there from dispatching concurrently. Multi-host scheduling is intentionally unsupported.
Each launched Codex process is held behind a startup gate until its process-group lease is durable;
a restarted scheduler terminates any matching orphan before dispatch. Runtime JSONL, leases, and
final result files live under
`${XDG_STATE_HOME:-~/.local/state}/domes-agent-control/`; they are diagnostic state and are never
fed to another role.

Each failed or timed-out role is restarted up to two times with bounded exponential backoff.
`--watch` is ordinary foreground process behavior; hosting it later is an operational choice and is
not encoded into this repository.

## GitHub labels

Create or reconcile the managed labels before the first live cycle:

```bash
python3 tools/agent_control/control.py labels --apply
```

The command manages these labels:

```text
agent:needs-specification
agent:plan
agent:plan-review
agent:ready
agent:running
agent:rework
agent:agent-review
agent:ci-pending
agent:verification
agent:human-review
agent:blocked
agent:done
priority:p0
priority:p1
priority:p2
priority:p3
```

The issue form starts in `agent:needs-specification`. Outside explicit `--autopilot`, requirements
stewardship and plan acceptance remain deliberate human/steward actions. Autopilot uses one fresh
read-only selector to translate existing milestone authority into a bounded contract; the selector
cannot edit the project brain or implement work.

## Safety and authority

- `queue` is read-only; `run --execute` is mutating.
- The process uses the existing `gh` and `codex` authentication.
- Planner and judge runs use a read-only Codex sandbox. Worker and verification runs always use a
  workspace-write sandbox in the issue worktree. Hardware workers reach registered boards only
  through the host broker described above.
- Controller-marked `software-review-required` tickets are implemented, published, independently
  judged, and repaired through exact-head CI. They then stop at `agent:human-review`; the controller
  never submits a GitHub approval or merge. It continues separate unblocked work while review waits.
- It never releases, adds `hw-test`, performs destructive device actions, or deletes a worktree.
- `--allow-registered-hardware` does not authorize `hw-test`, erase, NVS/factory reset, eFuse,
  secure boot, encryption, key, release, or arbitrary host commands.
- JSONL logs are retained for operator diagnosis but excluded from cross-role prompts.

Autopilot deliberately fails closed on its own policy and implementation, GitHub workflow files,
hardware, requirements/architecture authorities, dependency manifests, release paths, and security
policy. Those changes stop at human review even when every software check passes.
