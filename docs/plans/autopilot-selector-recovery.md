# Keep autonomous execution supplied after role and selector failures

Status: completed
Current phase: human review; runtime candidate deployed
Repository state: `codex/fix/autopilot-selector-recovery`; intentional changes are limited to the
agent controller, selector prompt, tests, and this plan
Last updated: 2026-08-21; failure chain reproduced from retained controller evidence

## Objective and observable outcome

Controller-owned process, sandbox, schema, and tracker failures remain retryable and do not become
false external blockers. Invalid selector output retries promptly, wakes on tracker changes, and
cannot repeatedly select scheduler-owned issues. When no implementation contract is reliable, the
selector may create one bounded software planner task so the executable DAG can be replenished.

## Authorities and contracts

- Authority: `WORKFLOW.md` and `tools/agent_control/control.py` - deterministic scheduling and state.
- Authority: `.codex/orchestration/prompts/selector.md` - disposable selector responsibility.
- Preserve: human review and merge authority; physical evidence remains separate and cannot pass
  through software inference.

## Affected components and generated consumers

| Component | Files | Required change |
| --- | --- | --- |
| Scheduler | `tools/agent_control/control.py` | Retry role and selector infrastructure failures without false blockers |
| Selector | `.codex/orchestration/prompts/selector.md` | Produce validator-compatible envelopes and planning fallback |
| Regression suite | `tools/agent_control/test_control.py` | Cover retry state, cooldown, and selector envelope invariants |

## Stages and dependencies

- [x] Reproduce the empty-queue, quota failure, invalid selector, and cooldown chain.
- [x] Implement retry and selector-contract corrections with regression coverage.
- [x] Run controller tests and repository contract validation.
- [x] Deploy the candidate runtime revision without merging and observe executable dispatch.
- [ ] **Human boundary:** Review and merge the published pull request.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 -m unittest discover -s tools/agent_control -p 'test_control.py'` | 132 passed |
| Automated | `python3 tools/agent_control/control.py validate` | passed |
| Scoped gate | `scripts/verify.sh --component tooling --component docs` | passed; host tooling 17 s |
| Runtime | candidate `7bb0887`; issue #132 | controller claimed `agent:running` and launched a workspace-write Codex worker |

## Decisions, discoveries, and deviations

- A role-process failure is controller infrastructure, not proof of an external project blocker.
- Valid task-reported blockers remain `agent:blocked`; only thrown role failures use retry journals.
- Selector errors use a short cooldown and retain the inspected snapshot so live state can wake them.

## Resume checkpoint

The runtime worktree is fast-forwarded to candidate `7bb0887`. Issue #132 was requeued after stale,
unused temporary build artifacts were removed; a live worker now owns it. Publish the repair PR for
human review without merging it. If the worker fails, the retry journal should preserve executable
state and the controller should continue selecting other work.
