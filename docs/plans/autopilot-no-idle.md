# Keep autonomous delivery capacity productive

Status: completed
Current phase: merged and active
Repository state: PR #118 was human-merged as `a1547b09bed63baf7a3de6f957fce81a4a49e8ae`
Last updated: 2026-08-16; live diagnosis confirmed empty slots do not trigger selection and planner issues fail before execution

## Objective and observable outcome

The watched autopilot continuously fills available agent capacity with eligible implementation,
verification, selection, or planning work. Human-review and externally blocked tickets remain visible
without preventing independent milestone selection. Planner output is accepted by Codex structured
outputs and can recursively materialize narrower tracked planning tickets as well as worker tickets.

Success is observable in focused tests that hold one worker active while a selector fills another
slot, materialize both `plan` and `execute` children, reject unsafe recursive expansion, and keep a
planner infrastructure failure retryable instead of misclassifying it as an external project
blocker.

## Authorities and contracts

- Authority: `WORKFLOW.md` and `docs/agent-system/README.md` - lifecycle and role boundaries.
- Authority: `tools/agent_control/control.py` - deterministic dispatch, reconciliation, and tracker transitions.
- Authority: `.codex/orchestration/schemas/*.json` and prompts - disposable role input/output contracts.
- Preserve: human review and merge authority; no agent approval or merge.
- Preserve: one scheduler, bounded concurrency, non-overlapping write surfaces, dependency checks, and hardware broker exclusivity.
- Preserve: selectors and planners remain fresh contexts and never consume worker transcripts.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Planner contract | planner schema and prompt | Accept supported structured output and distinguish recursive plan children from executable children |
| Controller | `tools/agent_control/control.py` | Fill spare slots, reconcile selector results against fresh state, materialize recursive planning, and retry controller failures safely |
| Tests | `tools/agent_control/test_control.py` | Reproduce the idle-slot, planner-schema, recursive-materialization, and failure-classification regressions |
| Workflow docs | `WORKFLOW.md`, agent-system/operator README | Describe capacity-filling selection and retry behavior accurately |

## Stages and dependencies

- [x] Reproduced the live idle state and planner API rejection.
- [x] Created an isolated branch/worktree from current `origin/main`.
- [x] Repair the planner contract and scheduler state machine.
- [x] Add focused regression coverage and update workflow documentation.
- [x] Run focused tests, control-plane validation, scoped repository verification, and diff review.
- [x] Publish one human-review PR and monitor required CI.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 -m unittest discover -s tools/agent_control -p 'test_control.py'` | passed; 110 tests |
| Automated | `python3 tools/agent_control/control.py validate` | passed |
| Automated | `scripts/verify.sh --component tooling --component docs` | passed; retained JSON summary and logs under `/home/pncosta/.cache/domes-autopilot-verify.v00cl1/` |
| Runtime dry check | controlled mocked watch-loop regression with active worker plus selector | passed in focused suite |
| Production controller | merged change running in `domes-autopilot` | passed; foreground tmux loop, no service manager, no automated PR approval or merge |

## Decisions, discoveries, and deviations

- A selector is productive agent work and consumes one concurrency slot; only one selector may run at a time.
- Selection must use fresh GitHub and `origin/main` state immediately before validation and mutation, not only the snapshot captured when the model started.
- Planner process or schema failures are controller failures, not external project blockers; they remain retryable with bounded backoff.
- Recursive planning is tracker-backed: `mode: plan` creates another `agent:plan` issue, while `mode: execute` creates `agent:ready` work.
- Existing milestone issues are materialized through one atomic PATCH, and new selector issues are serialized and reconciled by exact contract marker.
- A broader local workflow run was attempted. Host firmware passed, but unrelated checks were incomplete because `/tmp` reached its user quota, Flutter 3.38.9 did not match the pinned 3.44.8, and the globally activated Dart protobuf plugin was incompatible. Clean pinned GitHub CI remains the full-gate authority.

## Resume checkpoint

PR #118 was human-merged after full GitHub CI passed. Its controller is active in the
`domes-autopilot` tmux session and has already refilled a free slot with milestone selection and
reworked PR #105 after `main` advanced. No automated approval or merge authority was added.
