# Restore a usable DOMES delivery system

Status: complete (dashboard delivered; unrelated host-tooling check unavailable)
Current phase: privately published and recurring refresh restored
Repository state: main 0f1659c; existing unrelated edits preserved
Last updated: 2026-09-04

## Objective

Replace the existing private dashboard at its existing URL with three concurrent delivery
tracks, a navigable dependency graph, explicit evidence gaps, and a short human decision queue.
Restore the refresh source to the scheduled task's actual checkout. Define the next development
setup and the ordered dual-NFF campaign without accessing devices or changing host services.

## Authorities and contracts

- PROGRAM_STATUS.md owns executive state; docs/program/milestones.json owns its detailed task graph.
- docs/PRODUCT_REALIZATION_FRAMEWORK.md retains all phase/gate and spend boundaries.
- hardware/NEXT_ITERATION_REQUEST.md owns HR0-HR2; the new setup package is an input to it.
- The old dashboard source exists only in worktree 23b3. Reuse its Site ID and pinned toolchain.
- Preserve the old live version until the replacement validates; saved Sites versions retain history.
- Never advance a gate from GitHub activity, historical candidate evidence, or a successful build.
- No firmware, app, protocol, device, host-service, purchasing, or GitHub publication changes here.

## Stages

- [x] Locate old dashboard and private audience; audit current app and hardware evidence in parallel.
- [x] Reconcile status; specify three milestone sequences, dependencies, setup needs and human steer.
- [x] Replace Site interface; add deterministic refresh, evidence drift checks and negative tests.
- [x] Run lint, type checks, build and affected repository verification; limitation recorded below.
- [x] Publish privately, repair existing refresh automation and record handoff.

## Findings

- Scheduled checkout omitted dashboard files; prior worktree retained them untracked.
- Ledger dated Aug 15 predates Aug 22 local HEAD and Aug 29 remote main.
- PR 105 is merged, but its reviewed candidate and final head differ; reconcile evidence before credit.
- App simulation uses direct touches, not a production-protocol device adapter.
- Qualification entry report rejects predictive acceptance: terminal G/H evidence is absent.

## Verification

Passed: 14 meaningful validator tests, status refresh/check, no-op byte/mtime preservation, ESLint,
TypeScript, production Worker build, Worker fetch export and markdownlint on all affected documents.
No browser QA or new app/firmware/physical campaign was claimed or run.

Repository-wide docs component selects host-tooling. Precommit passed outside the sandbox, then
unrelated agent-eval checks failed: host Python 3.10 lacks tomllib; bundled Python 3.12 advances past
that but the installed Codex containment runtime lacks a usable Node path/codex-linux-sandbox.
Independent test triage confirmed this is an environment prerequisite; no services or runtime install
were changed. It is not a passing full repository gate. Summary: /tmp/domes-dashboard-docs-check.json.

Private publication succeeded 2026-09-04T23:35:27Z at
<https://domes-product-status.pcesar22.chatgpt.site>. Site version 5, source commit
689e6bdf50b21e8d1a4880729d97a2cfbca5076e, deployment
appgdep_6a9b55a96e3081919fa43e8a56791aa2. The existing Site is verified owner-only, no groups/external
visitors. Previous versions and the original worktree remain recoverable. No duplicate Site created.

Existing automation refresh-domes-product-status was updated in place to every two hours; its model,
notification preference, project and local execution were preserved. Future runs use this README,
review receipt and exact sources, fail closed on conflicts, and leave unchanged publication untouched.

## Handoff

Next programming package is FS-WP-004A, the app virtual pod lab; no app implementation was included
in this management reset. Next program package is HW-WP-002 setup definition. LAB0 readiness precedes
current physical NFF tests. G1 remains Hold. P1/Pre-EVT and unverified physical/predictive limits remain.
The primary wrote all changes; read-only app/hardware reviewers caught and resolved ID collisions,
premature acceptance, over-serialized dependencies and missing G2 cross-functional criteria.
