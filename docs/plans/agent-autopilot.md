# Close the DOMES autonomous delivery loop

Status: active
Current phase: mandatory human-review correction and operator dashboard
Repository state: `codex/fix/autopilot-human-review` from `origin/main` at `224c229`
Last updated: 2026-08-15; merge-capable controller paused before correction

## Objective and observable outcome

When no implementation role is active, the controller selects one bounded, already-authorized
execution delivery from the repository program ledger and live GitHub state, materializes an
accepted task DAG, dispatches isolated planner/worker/judge/verification contexts, and continues
until each eligible software-only pull request is ready for human review or a genuine external
boundary is recorded. Routine planning, agent judgment, CI repair, and slot refill require no
conversational manager, and waiting human reviews do not occupy execution slots.

## Authorities and contracts

- Authority: `PROGRAM_STATUS.md` - current milestone priority and authorization boundaries.
- Authority: `WORKFLOW.md` - deterministic lifecycle, concurrency, retry, and review policy.
- Authority: `docs/agent-system/README.md` - role separation and context-hygiene rules.
- Authority: root and nested `AGENTS.md` plus `docs/TESTING.md` - implementation and proof rules.
- Preserve: GitHub issues and commits remain authoritative; raw role transcripts never become
  cross-role context.
- Preserve: no automatic purchase, vendor/fabrication commitment, release, destructive hardware
  action, `hw-test` activation, requirements rewrite, or unsupported physical claim.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Workflow policy | `WORKFLOW.md` | Declare selection, task materialization, and human-review rules |
| Role contracts | `.codex/orchestration/` | Add fresh selector/steward verdicts and schemas |
| Controller | `tools/agent_control/control.py` | Implement closed-loop selection, DAG creation, CI/PR gating, review handoff, and refill |
| Tests | `tools/agent_control/test_control.py` | Cover idempotency, safety gates, and lifecycle transitions |
| Operator guide | `tools/agent_control/README.md` | Document unattended mode and exact hard boundaries |

## Stages and dependencies

- [x] Reconcile live `origin/main`, PRs, issues, CI, and the current milestone pointer.
- [x] Implement fail-closed selector, plan materialization, verification, and review policy.
- [x] Add focused unit and command-level tests; depends on the controller contract.
- [x] Run repository validation and strongest feasible software checks.
- [x] Publish and land bootstrap PR 111, then exercise the first live selection.
- [x] Pause the controller and remove automatic PR approval/merge authority.
- [x] Add a live operator dashboard that excludes raw worker transcripts.
- [ ] **Current:** Verify, publish the mandatory-human-review correction, and monitor required CI.
- [ ] Restart the non-merging controller on `ministrom` with `--dashboard`.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 -m unittest discover -s tools/agent_control -p 'test_*.py' -v` | passed, 54 tests |
| Contract | `python3 tools/agent_control/control.py validate` | passed |
| Live read-only | `python3 tools/agent_control/control.py queue --live` | passed; no managed eligible or blocked issues before bootstrap |
| Repository | `scripts/verify.sh --changed origin/main` plus focused tooling rerun | protocol, 294 host-firmware tests, 108 CLI tests, pre-commit, and focused controller tests passed; local `shellcheck` is unavailable, Flutter is 3.38.9 instead of 3.44.8, and the ESP-IDF v5.4.4 build exhausted `/tmp` at final link, so required CI remains authoritative |
| Physical confirmation | None | not applicable; controller work makes no physical claim |

## Decisions, discoveries, and deviations

- Extend the current Python controller instead of porting to the experimental Elixir reference;
  this reaches unattended DOMES delivery without discarding the repository-specific judge and
  evidence contracts.
- The first selected live package remains issue `#101` / PR `#105`; it is existing active work and
  outranks opening a competing package.
- Human review and merge are mandatory. The controller may prepare and repair review-ready PRs but
  cannot approve or merge them; it continues separate unblocked work while reviews wait.
- Independent prepublication review found and drove repairs for handoff binding, planner-restart
  journaling, refreshed-PR race checks, and complete paginated changed-path validation; regression
  tests now cover those paths.

## Resume checkpoint

The merge-capable session was stopped before this correction. Finish verification on
`codex/fix/autopilot-human-review`, publish the review-ready correction PR, then launch
`control.py run --execute --watch --autopilot --dashboard`. Resume issue `#101` / PR `#105` through
worker and judge validation, but stop the PR at `agent:human-review`; issue `#109` remains the
completed zero-change pilot.
