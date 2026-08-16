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

Inspect repository authorities and GitHub read-only. Make no repository or tracker mutation.
Return only the schema-conforming selection; do not include a transcript.
