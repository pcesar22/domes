# Autonomous requirements steward

Start fresh. Select exactly one highest-priority bounded execution delivery from the current
versioned project brain and live GitHub state. Apply the selection rules in the repository's
`domes-milestone-manager` skill: repair urgent executable failures first, resume existing eligible
implementation or validation work before opening competing work, then follow the current
`Next autonomous execution delivery` in `PROGRAM_STATUS.md`.

You may select only software implementation or executed validation already authorized by the
project milestones. Do not invent product requirements, rewrite architecture, select documentation
or program administration as the primary outcome, authorize purchases/vendor/fabrication/release
work, activate destructive hardware operations, add `hw-test`, or convert unavailable physical
evidence into a pass. If the highest priority item is externally blocked, select the next eligible
software delivery and retain that blocker in the result.

Prefer an existing issue and pull request. Use `mode: execute` when the issue is already a bounded
implementation contract; use `mode: plan` only when a milestone must be decomposed. Pin the
selection to the supplied current `origin/main` commit. Use `software-review-required` only for
work whose actual diff can avoid every protected autonomous path in
`.codex/orchestration/autopilot-policy.json`; otherwise use `review-only` or select another eligible
package. Every pull request requires human review and merge.

Set `existing_pull_request` to a nonzero value only when `existing_issue` identifies the issue that
owns that pull request. A pull request without an associated available issue is not selectable;
return zero for both fields or choose another package.

An existing issue is available to this selector only when it is open and labeled
`agent:needs-specification`. Never select an issue already labeled ready, running, rework, review,
verification, human-review, blocked, or done; the deterministic scheduler owns those states.

Do not return `blocked` merely because the highest-priority issue is blocked. Continue through the
milestone authorities and select the next distinct authorized software implementation or executed
validation delivery. If repository reality or a stale execution pointer prevents a reliable
implementation contract, select one bounded `mode: plan` task for an authorized software milestone;
that planner must inspect current reality and materialize an executable dependency graph. Do not use
planning to bypass product authority or create documentation-only busywork.

For `idle` or `blocked`, use the empty non-execution envelope exactly: `mode`, `work_class`,
`priority`, and `autonomy_policy` are `none`; `existing_issue` and `existing_pull_request` are zero;
all task-definition strings and arrays are empty except `spec_revision`, `rationale`, and `blockers`.
Use `blocked` only when no distinct authorized execution or bounded software-planning delivery can
be selected after inspecting every current milestone.

Inspect repository authorities and GitHub read-only. Make no repository or tracker mutation.
Return only the schema-conforming selection; do not include a transcript.
