# Prevent autonomous planner review dead ends

Status: active
Current phase: candidate publication and runtime deployment
Last updated: 2026-08-21

## Objective and observable outcome

Every task selected by the autonomous requirements steward must carry the repository's
human-review autopilot policy. A selector cannot create a `review-only` planning issue that finishes
in `agent:plan-review` with no executable role. Planner DAGs selected under standing software
authorization materialize into dependency-gated worker or recursive-planner issues while pull
requests still stop for human review and merge.

## Authorities and boundaries

- `WORKFLOW.md` owns lifecycle and human review/merge authority.
- `.codex/orchestration/autopilot-policy.json` owns protected autonomous paths.
- `.codex/orchestration/prompts/selector.md` owns selector instructions.
- `tools/agent_control/control.py` must mechanically reject unsafe or non-executable selections.
- No product requirement, merge authority, release rule, or hardware authorization changes.

## Stages

- [x] Reproduce the stranded `review-only` planner lifecycle on issue #135.
- [x] Require autonomous selections to use `software-review-required`.
- [x] Add regression coverage and run the tooling/documentation verification gate.
- [ ] Deploy the candidate controller and verify planner DAG materialization remains autonomous.

## Verification

| Evidence | Expected result |
| --- | --- |
| Controller unit tests | 133 passed; review-only selected output rejected |
| Controller contract validation | Passed |
| Scoped repository gate | Tooling and documentation passed in 27 seconds |
| Runtime observation | Current planner/worker work continues; no new plan-review dead end |

## Resume checkpoint

Publish a human-review PR. Deploy only after confirming the live controller has no implementation
worker that would be interrupted, then observe a full selector to planner-materialization cycle.
