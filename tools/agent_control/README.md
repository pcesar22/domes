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
agent:verification
agent:human-review
agent:blocked
agent:done
priority:p0
priority:p1
priority:p2
priority:p3
```

The issue form starts in `agent:needs-specification`. Requirements stewardship and plan acceptance
remain deliberate human/steward actions; an empty queue never authorizes invented work.

## Safety and authority

- `queue` is read-only; `run --execute` is mutating.
- The process uses the existing `gh` and `codex` authentication.
- Planner and judge runs use a read-only Codex sandbox. Worker and verification runs use a
  workspace-write sandbox in the issue worktree.
- The tool never merges, releases, adds `hw-test`, or deletes a worktree.
- JSONL logs are retained for operator diagnosis but excluded from cross-role prompts.
