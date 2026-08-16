# Close the DOMES autonomous delivery loop

Status: active
Current phase: publication and bootstrap
Repository state: `codex/feat/agent-autopilot` at `db83783`; this plan is the first intentional
change
Last updated: 2026-08-15; live control plane and program queue reconciled

## Objective and observable outcome

When no implementation role is active, the controller selects one bounded, already-authorized
execution delivery from the repository program ledger and live GitHub state, materializes an
accepted task DAG, dispatches isolated planner/worker/judge/verification contexts, and continues
until each eligible software-only pull request is merged or a genuine external boundary is
recorded. Routine planning, review, CI repair, and slot refill require no conversational manager.

## Authorities and contracts

- Authority: `PROGRAM_STATUS.md` - current milestone priority and authorization boundaries.
- Authority: `WORKFLOW.md` - deterministic lifecycle, concurrency, retry, and merge policy.
- Authority: `docs/agent-system/README.md` - role separation and context-hygiene rules.
- Authority: root and nested `AGENTS.md` plus `docs/TESTING.md` - implementation and proof rules.
- Preserve: GitHub issues and commits remain authoritative; raw role transcripts never become
  cross-role context.
- Preserve: no automatic purchase, vendor/fabrication commitment, release, destructive hardware
  action, `hw-test` activation, requirements rewrite, or unsupported physical claim.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Workflow policy | `WORKFLOW.md` | Declare selection, task materialization, and scoped merge rules |
| Role contracts | `.codex/orchestration/` | Add fresh selector/steward verdicts and schemas |
| Controller | `tools/agent_control/control.py` | Implement closed-loop selection, DAG creation, CI/PR gating, merge, and refill |
| Tests | `tools/agent_control/test_control.py` | Cover idempotency, safety gates, and lifecycle transitions |
| Operator guide | `tools/agent_control/README.md` | Document unattended mode and exact hard boundaries |

## Stages and dependencies

- [x] Reconcile live `origin/main`, PRs, issues, CI, and the current milestone pointer.
- [x] Implement fail-closed selector, plan materialization, verification, and merge policy.
- [x] Add focused unit and command-level tests; depends on the controller contract.
- [x] Run repository validation and strongest feasible software checks.
- [ ] **Current:** Commit, publish one review-ready PR, monitor required CI, and repair ordinary
  failures.
- [ ] Launch the reviewed controller on `ministrom`; depends on integration into `main`.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 -m unittest discover -s tools/agent_control -p 'test_*.py' -v` | passed, 49 tests |
| Contract | `python3 tools/agent_control/control.py validate` | passed |
| Live read-only | `python3 tools/agent_control/control.py queue --live` | passed; no managed eligible or blocked issues before bootstrap |
| Repository | `scripts/verify.sh --changed origin/main` plus focused tooling rerun | protocol, host firmware, CLI, host tooling, and ESP-IDF v5.4.4 builds passed; local Flutter remained unavailable at the required 3.44.8 pin and is delegated to required CI |
| Physical confirmation | None | not applicable; controller work makes no physical claim |

## Decisions, discoveries, and deviations

- Extend the current Python controller instead of porting to the experimental Elixir reference;
  this reaches unattended DOMES delivery without discarding the repository-specific judge and
  evidence contracts.
- The first selected live package remains issue `#101` / PR `#105`; it is existing active work and
  outranks opening a competing package.
- Automatic merging must be mechanically limited to an explicit ticket authorization and safe
  software-only surfaces with independent approval and current required CI.
- Independent prepublication review found and drove repairs for handoff binding, planner-restart
  journaling, refreshed-PR race checks, and complete paginated changed-path validation; regression
  tests now cover those paths.

## Resume checkpoint

Implementation and local verification are complete on `codex/feat/agent-autopilot`. Publish and
land the bootstrap PR, reconcile managed labels, then launch
`control.py run --execute --watch --autopilot` from the reviewed checkout. The first live selector
must resume issue `#101` / PR `#105`; issue `#109` remains the completed zero-change pilot.
