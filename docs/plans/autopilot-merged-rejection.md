# Merged-Before-Judge Recovery

## Objective

Prevent a human merge that completes before independent judgment from making rejected work appear
terminal or unlocking dependent tasks.

## Evidence

PR #144 was human-merged while its judge was running. GitHub auto-closed issue #140, then the
judge rejected the exact merged head and transitioned the issue to `agent:rework`. The scheduler's
former `closed OR agent:done` terminal rule nevertheless made #141 eligible.

## Design

- Only controller-owned `agent:done` is terminal for dependency evaluation.
- A merged PR attached to an inactive `agent:rework` ticket is recovery state, not completion.
- Recovery reopens a closed issue, clears only its execution-time PR binding, refreshes the signed
  contract digest, and leaves the pinned specification and acceptance contract unchanged.
- Active workers are never mutated. Recovery waits until the owning attempt exits.
- Corrective work creates a new PR and returns through worker, judge, CI, and human review.

## Verification

- Closed `agent:rework` dependencies remain nonterminal.
- Merged rework issues reopen with a valid contract and an empty PR slot.
- Active merged-rework tickets are not mutated.
- Controller unit tests, repository contract validation, and scoped tooling/docs verification pass.

## Status

Implemented and verified. Live issue #140 was reconciled manually before #141 dispatched; this
change makes that recovery deterministic.
