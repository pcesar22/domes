# DOMES Agent Control Plane

This standard-library Python tool implements the repository-facing portion of OpenAI Symphony's
[language-agnostic contract](https://github.com/openai/symphony/blob/main/SPEC.md) for DOMES. It
reads GitHub issues, validates the task DAG, creates isolated controller-owned Git workspaces,
invokes fresh Codex roles with JSON-schema outputs, and performs explicit state transitions. It does
not require or install a service manager.

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
ticket/specification-bound queue capability and only the ticketed board aliases. The implementation
worker never receives that capability. It first pushes a PR head; a fresh read-only judge must
approve that immutable head for finite hardware verification. The broker then revalidates the
private udev mapping immediately before each request and permits only the judged remote head and
ticketed operation. Flash and OTA images are built by the host broker from a fresh GitHub clone in a
networkless `bwrap` sandbox with an explicit ESP-IDF v5.4.4 Python/compiler/ULP/ROM toolchain and
registry-hash-verified locked components. Worker-supplied builds, host Git metadata, and broad
`export.sh` activation are rejected. Device evidence remains in controller-private ticket state.

`flash-trace-acceptance` is distinct from ordinary `flash` and must appear explicitly in the
ticket's finite `Hardware operations` list. The broker generates its private Kconfig defaults from
the checked-in physical defaults plus only `CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE=y`, verifies the
resulting profile, and hashes the defaults and final SDKCONFIG into provenance. Ordinary `flash`
always rebuilds the trace-disabled default profile for restoration. Failed attempts remain in the
hash-chained audit manifest and are disclosed to the independent judge; they are not silently
dropped or converted into passing evidence.

For `trace-dump`, the broker derives the board's active image from that manifest and rejects any
rebuild whose application hash differs. It builds the exact PR-head CLI inside a metadata-free,
network-isolated `bwrap` sandbox with a cleared environment, then runs it in a second sandbox that
can see only a broker-owned PTY, the matching application image, the trace-name map, and a dedicated
output directory. The PTY relay forwards exactly one CRC-valid empty trace-dump frame and records
the device responses; the candidate never receives the physical serial path. The broker independently
binds the firmware image, framed session identity, complete raw event stream, and candidate CLI. It
then runs the judged trace normalizer in another networkless sandbox and returns only artifact IDs,
hashes, and a privacy-safe semantic summary. A fresh final judge receives a controller attestation
created by rehashing the private artifacts; raw evidence, device paths, factory IDs, and worker
transcripts never enter judge or tracker context.

A failed preflight blocks only that ticket. It is automatically requeued only when a later
preflight succeeds and the saved typed blocker still matches the same issue, specification, and PR
head. Old comments and prose are not recovery authority.

For a live tmux/operator view instead of machine-readable JSON snapshots:

```bash
python3 tools/agent_control/control.py run --execute --watch --autopilot --dashboard --allow-registered-hardware
```

The dashboard refreshes every poll and shows workers, planners, an active milestone selector, CI
state, review-ready PRs, blockers, and the selector's latest action. A selector occupies one of the
configured agent slots and may run alongside unrelated workers whenever capacity remains. It never
displays raw worker transcripts; detailed JSONL remains only in the runtime state directory.

`run` is deliberately explicit because it creates worktrees, launches mutation-capable workers,
and changes GitHub issue labels/comments. Mutation-capable runs are pinned to the reviewed
`scheduler_host` in `WORKFLOW.md`, and a non-blocking advisory file lock prevents two scheduler
processes there from dispatching concurrently. Multi-host scheduling is intentionally unsupported.
Each launched Codex process is held behind a startup gate until its process-group lease is durable;
a restarted scheduler terminates any matching orphan before dispatch. Runtime JSONL, leases, and
final result files live under
`${XDG_STATE_HOME:-~/.local/state}/domes-agent-control/`; they are diagnostic state and are never
fed to another role.

Agent workspaces are disposable standalone clones below the configured workspace root, with private
`.git` metadata inside the sandbox. Every role starts from a new controller-created clone; the host
never invokes Git against metadata left by a previous worker. Durable work must be pushed to the
ticket branch/PR. This prevents worker configuration, hooks, filters, refs, or object state from
becoming control-plane authority. Codex receives an explicit writable-directory grant for only the
current clone's private `.git`.

Each failed or timed-out role is restarted up to two times within a run with bounded exponential
backoff. A planner process or response-contract failure remains `agent:plan` with a persisted,
bounded retry delay so the selector can fill other capacity; a valid planner-reported project
blocker still enters `agent:blocked`.
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
stewardship and plan acceptance remain deliberate human/steward actions. Autopilot uses at most one
fresh read-only selector at a time to translate existing milestone authority into a bounded
contract. It runs whenever a role slot would otherwise be unused, revalidates its decision against
fresh tracker state, and cannot edit the project brain or implement work. A planner task explicitly
marks each child `execute` or `plan`; recursive planning therefore creates another tracked,
bounded `agent:plan` issue instead of an untracked conversation.

## Safety and authority

- `queue` is read-only; `run --execute` is mutating.
- The process uses the existing `gh` and `codex` authentication.
- Planner and judge runs use a read-only Codex sandbox. Worker and verification runs always use a
  workspace-write sandbox in the issue worktree. Hardware workers reach registered boards only
  through the host broker described above.
- Controller-marked `software-review-required` tickets are implemented, published, independently
  judged, and repaired through exact-head CI. They then stop at `agent:human-review`; the controller
  never submits a GitHub approval or merge. It continues separate unblocked work while review waits.
- It never releases, adds `hw-test`, performs destructive device actions, or deletes an operator
  worktree. Disposable controller-owned agent clones are replaced between roles.
- `--allow-registered-hardware` does not authorize `hw-test`, erase, NVS/factory reset, eFuse,
  secure boot, encryption, key, release, or arbitrary host commands.
- JSONL logs are retained for operator diagnosis but excluded from cross-role prompts.

Autopilot deliberately fails closed on its own policy and implementation, GitHub workflow files,
hardware, requirements/architecture authorities, dependency manifests, release paths, and security
policy. Those changes stop at human review even when every software check passes.
