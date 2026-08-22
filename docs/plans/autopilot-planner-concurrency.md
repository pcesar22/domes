# Autopilot Planner Concurrency

## Objective

Keep disposable planners active while implementation workers use overlapping repository
surfaces. Read-only planning must not consume a worker path reservation.

## Evidence

Issue #148 remained `agent:plan` with three free controller slots while issue #141 was the
only active worker. Both tickets named overlapping firmware and simulation paths, and the
scheduler applied the same path-conflict rule to every role.

## Change

- Reserve architectural surfaces only for worker and verification-worker roles, which may
  mutate repository artifacts.
- Allow planner and judge roles to inspect surfaces concurrently with active workers.
- Preserve conflict exclusion between all mutating roles.
- Keep an exact, judged, mergeable PR reviewable when an unrelated merge merely makes it
  behind `main`; invalidate only actual conflicts, changed heads, or requested changes.
- Preserve stacked-child validity across that harmless base advance so productive work is
  not discarded.

## Verification

- Controller unit tests cover a planner overlapping a running worker and a planner followed
  by a worker on the same surface.
- Existing worker-overlap tests continue to prove mutation exclusion.
- Review and stack tests distinguish a mergeable `BEHIND` state from a `DIRTY` conflict.
- Contract validation and tooling/documentation verification pass.
- The live controller dispatches queued planner issue #148 while worker #141 remains active.

## Status

Implementation and repository verification complete. Live dispatch verification follows
deployment to the controller worktree.
