# DOMES Development Dashboard

Private Site: <https://domes-product-status.pcesar22.chatgpt.site>

The founder-facing delivery map has three parallel tracks: phone app/simulation, dual-NFF
validation, and next setup/hardware. A selected node shows its prerequisites, acceptance evidence,
owner, gap, invalidation rule and downstream work. Shared inputs, simulator qualification, human
steer, evidence gaps and the complete P0–P7 horizon remain visible without percentage rollups.

## Authority and states

PROGRAM_STATUS.md is executive authority. docs/program/milestones.json is its detailed reviewed
graph. The framework controls program gates and HR releases. Dashboard-only NFF labels are bounded
campaign steps, not new company phases. Existing HW0–HW6 outcome IDs are not redefined.

- Complete: recorded accepted scope; the card explicitly retains historical/configuration limits.
- Ready: entry dependencies are accepted and work can start; not an assertion that execution began.
- Active: execution is actually underway.
- Acceptance pending: entry dependencies passed; a candidate awaits evidence audit.
- Not due: a predecessor or program interval has not passed. Prototype artifacts may exist separately.
- Blocked: an explicit resource or decision prevents the next result.

Gates never advance from PRs, merges or green CI. Physical, software, simulation and business
authorization remain different evidence. The Site audience stays owner-only.

## Deterministic refresh

From this directory, with the checked-in npm lockfile and Node 24:

```sh
npm ci --no-audit --no-fund
npm run status:refresh
npm run status:check
npm test
npm run lint
npm run typecheck
npm run build
```

Set DOMES_REPO_ROOT to an absolute DOMES checkout when running a copied Site publication directory.
The default is the checkout containing sites/product-status-dashboard.

Refresh first validates all required repository files, executive/graph agreement, source paths,
statuses, prerequisites, graph cycles, physical inventory and gate boundaries. It then compares
source and model hashes to status/reviewed-sources.json. Missing evidence or drift exits nonzero
before any generated file is written. Build checks generated files without modifying them.
Relevant app/firmware/protocol/simulation/hardware/plan scopes are inventoried as well, so new,
removed or changed implementation files beyond the cited sources also require review.

Generated outputs are status/program-status.json, public/status.json and public/evidence.html.
The last includes escaped source snapshots, so new local documents have reviewable evidence even
before a DOMES GitHub commit exists. Do not expose it outside the existing owner-only audience.
Freshness is computed in the browser; timestamps do not change just because a refresh ran.
An unchanged refresh preserves output bytes and modification times and requires no redeployment.

## Reviewing real changes

If hashes drift, read the changed authorities, implementation, tests and immutable retained evidence.
Update PROGRAM_STATUS.md and docs/program/milestones.json together only where direct evidence
supports the change. Retain uncertainty explicitly; missing or conflicting evidence cannot pass.
Update or add a substantive review note under docs/program/, set the model's review/reviewedAt,
and keep the executive date aligned. Never bump the date merely to hide stale evidence.

After that audit, explicitly record its receipt:

```sh
npm run status:review -- docs/program/review-2026-09-04.md
```

Use the actual new review note path for later reviews. A changed source set requires a changed
substantive note; recording a receipt is never a substitute for reviewing evidence. The command is
not part of refresh or build. Run the full deterministic validation sequence afterward.

The validator catches structural/hash conflicts. Semantic review remains the AI evidence auditor's
responsibility; a hash alone does not certify a claim. Thresholds/requirements, calibration and
independent held-out data stay in their owning authorities.

## Continuous operation

Existing Codex automation refresh-domes-product-status runs every two hours on this checkout while
its host is available. It checks the actual source receipt, reviews changes when supported, and
publishes only a changed, validated owner-only snapshot. A failed review or build leaves the last
successful publication intact and reports the precise gap. No device, ministrom or host-service
access belongs in this automation. It does not merge, release, buy hardware or manufacture gate state.

The open page checks its published snapshot every minute. A newly published content hash reloads
the page. If offline it retains the loaded review and exposes connection/freshness status. A source
review older than seven days is visibly stale. This is bounded periodic freshness, not a claim of
continuous access to a sleeping host or a live hardware monitor.

## Private publication

Reuse .openai/hosting.json's project_id; never create a replacement Site or change its audience.
After all checks and production build pass, use the Sites hosting workflow. Verify current owner-only
access. Commit/push exact validated Site source to its dedicated source repository, package dist with
the Sites package-site.sh helper, save one version and deploy it privately. Wait for terminal success.

DOMES is a monorepo: keep its Site files tracked as ordinary files. Use a fresh temporary clone of
the dedicated Sites source repository for publishing, copy the exact validated Site tree (including
dotfiles but excluding node_modules, build caches and local credentials), and use DOMES_REPO_ROOT
when checking it. Preserve that repository's Git history; never commit/push unrelated DOMES sources
or turn sites/product-status-dashboard into a nested Git repository/gitlink. Copy the already-built
dist unchanged after the source commit; package only build output, not the source tree.

Prior private versions and the original worktree retain the replaced presentation for recovery.
No duplicate daily automation, external GitHub publisher, device job or public activity API is needed.
