# Establish a repository-native agent control plane

Status: complete
Current phase: implementation complete
Repository state: `codex/feat/agent-control-plane` in `.worktrees/agent-control-plane`; control-plane
setup committed with a clean worktree
Last updated: 2026-08-15; implemented and verified the GitHub-backed control plane, including live
repository labels; no agent task was dispatched because the managed queue is empty

## Objective and observable outcome

DOMES can deterministically select eligible GitHub issues, launch disposable role-specific Codex
runs in isolated worktrees, reject blocked or malformed tickets before dispatch, and accept work
only through an independent judge result. Product intent and acceptance contracts remain in tracked
repository and issue state; raw agent transcripts are never an input to planners or judges.

## Authorities and contracts

- Authority: `WORKFLOW.md` - orchestration states, dispatch policy, and role handoffs.
- Authority: `docs/agent-system/README.md` - project-brain map and role boundaries.
- Authority: `PROGRAM_STATUS.md` - program priority and current delivery state.
- Preserve: `AGENTS.md` authorization boundaries and component-specific verification contracts.
- Preserve: GitHub issues as the task DAG; runtime sessions and worktrees are disposable.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Repository guidance | `AGENTS.md`, `.codex/README.md`, `docs/README.md` | Route agents to the control-plane contract |
| Workflow policy | `WORKFLOW.md`, `.github/ISSUE_TEMPLATE/agent-task.yml` | Define states and complete ticket inputs |
| Agent roles | `.codex/orchestration/prompts/` | Separate steward, planner, worker, judge, and verification duties |
| Handoffs | `.codex/orchestration/schemas/` | Admit only concise machine-readable results |
| Control plane | `tools/agent_control/` | Validate tickets/DAGs and explicitly dispatch eligible work |

## Stages and dependencies

- [x] Reconciled existing project authorities and selected a no-service GitHub adapter.
- [x] Added workflow, role, schema, and ticket contracts.
- [x] Implemented deterministic validation, eligibility, isolation, dispatch, retry, and acceptance.
- [x] Integrated repository checks and ran focused plus host-tooling verification.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 -m unittest discover -s tools/agent_control -p 'test_*.py' -v` | passed, 25 tests |
| Automated | `python3 tools/agent_control/control.py validate` | passed |
| Automated | pinned pre-commit 4.6.1 hooks for all changed files | passed after the contention fix |
| Automated | `scripts/verify.sh --quick --component tooling` | Python, docs, formatting, and repository suites passed; aggregate stopped only because host `shellcheck` is absent |
| GitHub dry run | `python3 tools/agent_control/control.py queue --live` | passed; zero managed eligible or blocked issues |
| GitHub setup | `python3 tools/agent_control/control.py labels --apply` plus live label readback | passed; 15 labels present |
| Live agent cycle | `python3 tools/agent_control/control.py run --execute` | not run: no issue is in a dispatchable managed state |

## Decisions, discoveries, and deviations

- OpenAI's reference implementation currently targets Linear and is explicitly experimental.
  DOMES keeps GitHub issues and implements the language-agnostic Symphony scheduling contract.
- Runtime hosting is excluded. The control plane is an explicit CLI and makes no resident-process
  assumption.
- Existing repository authorities form the project brain. New documents index them and do not copy
  product or architecture facts.

## Resume checkpoint

The repository setup is complete in `codex/feat/agent-control-plane`. GitHub labels are installed,
the live queue is empty, and no agent task has been dispatched. The next operational action is to
merge or otherwise install this branch, create a ticket with the checked-in issue form, accept its
specification and plan states, inspect `queue --live`, and then explicitly run one bounded cycle.
The host still needs the repository-pinned `shellcheck` dependency before the aggregate tooling
wrapper can reach its final actionlint step.
