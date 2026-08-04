# Product Brief And Canonical Six-Pod Workflow

Status: active
Current phase: product baseline synthesis
Repository state: `codex/docs/ps-wp-001-product-brief` in `.worktrees/ps-wp-001`; based on
`origin/main` at `0378730`; no intentional dirty files before this plan
Last updated: 2026-08-04; authoring and available local verification complete, publication/CI next

## Objective and observable outcome

Produce one evidence-labeled product brief and canonical six-pod workflow that gives PS1, FS3, HW1,
and VC1 a common requirements-entry boundary. Success is observable when the brief names the buyer,
user, job, kit, environment, economic guardrails, unresolved discovery, every workflow stage from
setup through charging/storage, and the policy for partial failure and recovery without presenting
target behavior as implemented or verified.

## Authorities and contracts

- Authority: `research/PRODUCT_DEFINITION.md` - product hypotheses, decisions, workflow, and
  requirements-entry boundary.
- Authority: `research/SOFTWARE_ARCHITECTURE.md` and implementation - current as-built software.
- Authority: `research/SYSTEM_ARCHITECTURE.md` and `research/ID_REQUIREMENTS.md` - target product and
  industrial-design inputs, not current proof.
- Authority: `PROGRAM_STATUS.md` - execution, evidence, risk, forecast, and next-action status.
- Preserve: no runtime, protocol, architecture, part, spend, market, compliance, or physical-product
  claim is authorized by this documentation package.
- Preserve: evidence, hypothesis, decision, target, and current gap remain distinguishable.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Product authority | `research/PRODUCT_DEFINITION.md` | Establish the brief, workflow, recovery policy, gaps, and requirement seeds |
| As-built map | `research/SOFTWARE_ARCHITECTURE.md`, `ios/domes_app/README.md` | Clarify current app workflow limits where existing summaries can overstate readiness |
| Navigation | `research/README.md` | Point readers to the expanded authority and evidence boundary |
| Program control | `PROGRAM_STATUS.md` | Record PS-WP-001 state/evidence and the next AI-owned package |
| Execution record | Issue 91 and the eventual pull request | Link commands, results, uncertainty, and stop boundary |

No generated consumer changes.

## Stages and dependencies

- [x] Reconciled live PR, issue, plan, CI, program, product, architecture, app, and hardware-request
  truth; no higher-priority integrity repair or resumable package exists.
- [x] Created issue 91 and isolated branch/worktree from current `origin/main`.
- [x] Completed bounded read-only repository exploration and identified current/target conflicts.
- [x] Wrote the product brief, canonical workflow, explicit gaps, and working program-state update.
- [x] Closed semantic-review findings and reran documentation/repository verification.
- [ ] **Current:** Self-review, commit, push, open one PR, monitor required CI, and update
  issue/status evidence.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `python3 tools/docs/check_markdown_links.py` | passed: 86 files, 419 relative links after final finding fixes |
| Automated | `scripts/verify.sh --component docs` | all available checks passed; wrapper stopped only because host lacks `shellcheck`; CI required |
| Automated | strongest applicable pre-commit and instruction/evaluator tests | pre-commit passed; 51 agent/instruction/plan tests and 40 CI contract tests passed |
| Accepted command | Not applicable; no transport or device command changes | not required |
| Physical confirmation | Not applicable; no current hardware behavior is claimed | not required |

## Decisions, discoveries, and deviations

- The product brief remains in `research/PRODUCT_DEFINITION.md`; creating a second product authority
  would add ambiguity.
- The current app has an internal multi-pod provider, but no production UI path populates it. Its
  displayed reaction, sequence, and speed types also share the same random-target execution path.
- App scoring uses phone wall time even though touch notifications contain a pod-local timestamp;
  neither value is correlated across pod clocks.
- The product workflow will define operator-visible partial-failure behavior without selecting the
  future BLE/ESP-NOW authority topology owned by FS3.
- Numeric commercial targets remain dated hypotheses. They do not replace customer, cost, warranty,
  support, or purchase evidence.

## Resume checkpoint

Issue 91 and the isolated worktree exist. The brief, workflow, as-built corrections, and working
status update are written. The charging-topology, status-timing, and economic-traceability findings
are closed; the independent rereview found no remaining issue. Available local checks pass except
that the host has no `shellcheck`; required CI supplies it. Self-review the diff, commit/push, open the
one linked PR, and monitor every required check. Do not modify firmware, protocol, CLI, or app
behavior. Main CI for merge commit `0378730` passed including its aggregate gate.
