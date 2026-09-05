# DOMES coordinator checkpoint

Updated: 2026-09-05 01:12 UTC. This is resumable operational state, not a milestone acceptance record.

- Authority: docs/agent-system/OPERATING_MODEL.md; activation approved by Paulo in this task.
- Coordinator heartbeat: coordinate-domes-delivery, ACTIVE, every 30 minutes, current task.
- Sole scheduled Site publisher: refresh-domes-product-status, existing two-hour cadence,
  owner-only; preserve its no-device/no-ministrom/no-GitHub-write boundary.
- Runtime host: ministrom; clean main 3b62a6c82160d0271b276bb42894e3d3bb69761e. Use normal
  zsh login environment for Codex/Flutter. No service changes or installations performed.
- The first finite controller cycle at 00:44:46 UTC completed both workers and ended normally.
  A second finite run --execute --limit 2 started around 01:10 UTC in domes-coordinated;
  both issue-197 and issue-198 read-only judge processes were directly observed at 01:11 UTC.
  No watch, autopilot or hardware opt-in. Inspect live state before any new cycle.
- App issue #197: virtual lab; NFF issue #198: software memory repair; #199: hardware desk
  definition. Contract specification is current main; management/Site sources remain on
  codex/feat/development-dashboard, not merged. Do not assume those branches are equivalent.
- Six open PRs now fill the cap: #190/#191/#194, worker PRs #200/#201 and activation draft #202.
  Do not start another new-PR package. Existing-PR repair/review may continue after live checks.
  Initial issue contracts now explicitly bind #197 to PR #201 and #198 to PR #200; no acceptance
  criteria, specification revisions or physical boundaries changed.
- App PR #201 head cffa30e3fccbfe5558ee4b4e7c3ab862351f3c46 and NFF PR #200 head
  876fdd8b40f22175341671bff1d303d956376ebf both target main at the pinned specification.
  All eight checks, including CI Gate, pass on each exact head as independently queried from
  GitHub's commit check-runs endpoint. Neither independent judge has returned a verdict yet.
- NFF initially entered rework directly from its worker because of two pending evidence records:
  then-pending CI (now passed) and physical revalidation (explicitly outside this software ticket).
  No failed records, blockers or judge verdict caused that transition. The coordinator preserved
  the handoff and documented routing it to fresh independent review, without acceptance:
  [Routing evidence](https://github.com/pcesar22/domes/issues/198#issuecomment-5548306287).
  Do not replay it as a worker missing a judge handoff or demand prohibited physical checks.
- Most older tickets have unmet dependencies; #166/#193 were the other eligible rework packages.
  Never start a cycle without reviewing the complete live candidate set; there is no ID filter.
- Last account check: main allowance 5% consumed / 95% remaining; spare 20% reserve applies.
  Per-package tokens are not yet available. Do not redeem either available reset credit.
- NFF outcome: both programmed and reachable; readiness 0/2 due Heap, unchanged thresholds.
  No physical operations are authorized in initial worker contracts.
- Hardware first desk record: hardware/SETUP_INVENTORY_2026-09-05.md; instruments, owner,
  safe-bench details, phone access, quotes and NFF5 naming conflict remain open.
  Delivery recorded on #199; keep it open for the unresolved setup-definition work.
- Activation source pushed as bcfe483a9dac18633e2aa4c91d8de87d9e5bf1cb on
  codex/feat/development-dashboard; [activation draft](https://github.com/pcesar22/domes/pull/202).
  Preserve unrelated local edits to .codex/PLATFORM.md and tools/agent_control/test_control.py.
- Site version 6 privately deployed successfully at 00:58:42 UTC, dedicated source
  9c7f0f4c856404c1112d7e59d4d9ab5849b13ed4, deployment appgdep_6a9b692da5108191b8a1638a21247b77.
  Owner-only access reverified; no sharing changes. Build, lint, typecheck, all 18 status tests,
  archive and 463 relative links passed. The receipt also passes after documentation formatting.
- Broader docs-scope repository check remains failed at the existing local containment test:
  tools/agent_eval/test_agent_eval.py::IsolationTest::test_no_model_containment_preflight_denies_host_files,
  because its sanitized environment cannot find node. 51 other tests passed; no threshold was
  waived and no host-service change was made. Summary: /tmp/domes-activation-docs-check.json.
  Markdownlint fixed one pre-existing bare URL in product-control-reset.md; other hooks passed.
- Slack: activation plus first steering/progress digest sent to Paulo U0BS70FLB3K / self-DM
  D0BRYQMBP5H, message 1788569982.547489. Inbound handling remains unverified; steering stays here.
  [Sent message](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788569982547489).
- Notification keys sent: activation-2026-09-05-v6; decisions-owner-inventory-envelope-initial;
  daily-progress-2026-09-04-Pacific. Do not repeat without a meaningful change or explicit reminder.
- Follow-up sent at 01:12 UTC: both first PRs, exact-head CI passing and read-only judges started;
  message 1788570726.295089, key first-prs-200-201-ci-pass-judges-started.
  [Review-cycle update](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788570726295089).
- Next: observe the two active judges and retain their exact-head verdicts. A separate bounded
  app pre-review raised a possible provider-clock composition gap; the formal judge must assess
  the actual contract and diff rather than inherit an unverified verdict. Continue bounded repair
  and verification on the existing PRs only, and keep hardware desk work moving within available
  inputs. The activation PR stays draft while its broader verification failure and CI remain
  unresolved. No automated approval or merge. The Site is the dated activation snapshot, not a
  live reflection of unreviewed PR labels; no product milestone acceptance changed.
