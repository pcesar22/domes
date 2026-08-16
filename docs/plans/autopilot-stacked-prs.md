# Allow one-deep autonomous stacked pull requests

Status: active
Current phase: publication and CI
Parent delivery: PR #118 was human-merged as `a1547b09bed63baf7a3de6f957fce81a4a49e8ae`
Last updated: 2026-08-16

## Objective and observable outcome

A software child with exactly one nonterminal dependency may execute while that dependency waits in
`agent:human-review`. The child is based on the parent pull request's exact independently judged
head and opens its own pull request against the parent branch. Parent review therefore does not
idle dependent delivery, while every merge remains human-only.

Success is observable when a child worker is dispatched from an exact healthy parent head, its
artifact and PR base are mechanically verified, and any parent head/state/merge change invalidates
the child and forces a fresh worker, judge, and CI cycle. Main-based work remains unchanged.

## Authorities and safety contract

- Preserve `WORKFLOW.md`, the ticket DAG, exact artifact handoffs, and independent judge acceptance.
- Permit exactly one stack parent and one stack level; reject multi-parent fan-in and stacked parents.
- Permit only automated software tickets with no hardware operations.
- Require the parent ticket to be open in `agent:human-review`, with one open, non-draft, clean PR
  against `main`, an exact reviewed artifact head, and no requested changes.
- Never carry child approval across parent head movement, conflict, closure, rework, or merge.
- Never approve or merge any pull request.

## Affected components

| Component | Required change |
| --- | --- |
| Ticket dependency eligibility | Recognize one narrowly validated human-review stack parent while retaining terminal dependency defaults |
| Worker workspace and prompt | Branch from the exact fetched parent head and create the child PR against the parent branch |
| Artifact verification | Bind base branch, base OID, ancestry, parent issue, parent PR, and parent head |
| CI and human-review reconciliation | Recheck the live parent and rework the child whenever its stack binding changes |
| Tests and documentation | Cover allowed stack flow, rejected fan-in/depth/hardware cases, and invalidation cascades |

## Stages and dependencies

- [x] Audit every main-only and terminal-dependency assumption.
- [x] Implement the exact one-parent stack context and dispatch contract.
- [x] Bind workspace preparation, worker prompt, artifact validation, CI, and review reconciliation.
- [x] Add parent-change/merge invalidation and focused regressions.
- [x] Run scoped verification and hostile review.
- [ ] **Current:** Publish one human-review PR against `main` and monitor CI without merging it.

## Verification

| Evidence | Status |
| --- | --- |
| Main-based controller regressions | passed; focused suite contains 125 tests |
| Exact parent branch/head workspace and artifact tests | passed, including live #106 -> #101 / PR #105 resolution before its later rework |
| Parent movement/conflict/merge invalidation tests | passed, including merged-child main-ancestry and parent-terminal races |
| Comment-handoff trust boundary | passed; only the pinned `tracker_actor` can supply durable controller handoffs |
| `python3 tools/agent_control/control.py validate` | passed |
| `pre-commit run --all-files` | passed with repository-local `TMPDIR` |
| `scripts/verify.sh --component tooling --component docs` | passed; retained under `/home/pncosta/.cache/domes-stacked-final.fk3hWb/` |
| GitHub CI for stacked PR | pending |

## Decisions and deviations

- PR #118 was human-merged while this delivery was in progress. This branch was fast-forwarded to
  its merge commit and will therefore publish against `main`, not against the now-integrated branch.
- A review-ready child that a human merges into its exact parent remains nonterminal until its
  integration commit is proven on `main`. If the parent drops that commit, the child alone blocks;
  the controller does not rewrite the human-authoritative ticket body and continues other work.
- Durable comment handoffs accept only GitHub comments authored by the version-pinned
  `tracker_actor`; other role-marker lookalikes are ignored.

## Resume checkpoint

Implementation and hostile review are complete on current `origin/main`. Publish the scoped branch,
monitor required CI, then activate the reviewed-CI controller without approving or merging its PR.
