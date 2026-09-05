# DOMES coordinator checkpoint

Updated: 2026-09-05 01:56 UTC. This is resumable operational state, not a milestone acceptance record.

- Authority: docs/agent-system/OPERATING_MODEL.md; activation approved by Paulo in this task.
- Coordinator heartbeat: coordinate-domes-delivery, ACTIVE, every 30 minutes, current task.
- Sole scheduled Site publisher: refresh-domes-product-status, existing two-hour cadence,
  owner-only; preserve its no-device/no-ministrom/no-GitHub-write boundary.
- Runtime host: ministrom; clean main 3b62a6c82160d0271b276bb42894e3d3bb69761e. Use normal
  zsh login environment for Codex/Flutter. No service changes or installations performed.
- The first finite controller cycle at 00:44:46 UTC completed both workers and ended normally.
  The second finite cycle's independent judges rejected both candidates at 01:12/01:13 UTC.
  A third finite run --execute --limit 2 started around 01:18 UTC in domes-coordinated;
  both existing-PR repair workers were observed at 01:19 UTC; that cycle finished by the 01:47 check.
  A fourth finite cycle began around 01:49 UTC after clean host/authentication/controller, no-live-
  process and full queue/cap checks. Both read-only issue-197/198 judges were observed at 01:50 UTC.
  Both judges returned approve at 01:52 UTC and that cycle exited. A fifth finite cycle of two
  verification workers started around 01:56 UTC after renewed exact-head/capacity/queue/host checks.
  Both verification-result-schema worker processes were directly observed at 01:56 UTC.
  Do not launch another controller while that cycle still owns work.
  No watch, autopilot or hardware opt-in. Inspect live state before any new cycle.
- App issue #197: virtual lab; NFF issue #198: software memory repair; #199: hardware desk
  definition. Contract specification is current main; management/Site sources remain on
  codex/feat/development-dashboard, not merged. Do not assume those branches are equivalent.
- Six open PRs now fill the cap: #190/#191/#194, worker PRs #200/#201 and activation draft #202.
  Do not start another new-PR package. Existing-PR repair/review may continue after live checks.
  Initial issue contracts now explicitly bind #197 to PR #201 and #198 to PR #200; no acceptance
  criteria, specification revisions or physical boundaries changed.
- Repaired app PR #201 head b698a3f9c5bd1fc870bb04052eaf0c1626a08882 and NFF PR #200 head
  876fdd8b40f22175341671bff1d303d956376ebf both target main at the pinned specification.
  All eight checks, including CI Gate, pass on each exact head as independently queried from
  GitHub's commit check-runs endpoint at 01:48 UTC. The app's On-Device Tests was skipped, not a
  pass. Both new independent verdicts approve the exact repaired artifacts, seven criteria each,
  no required rework. The app verdict excludes physical/parity evidence; the NFF verdict excludes
  physical readiness. Final verification and human approval/merge remain. Rejections of the first
  artifacts remain historical; CI cannot by itself establish that their remedies are acceptable.
- NFF initially entered rework directly from its worker because of two pending evidence records:
  then-pending CI (now passed) and physical revalidation (explicitly outside this software ticket).
  No failed records, blockers or judge verdict caused that transition. The coordinator preserved
  the handoff and documented routing it to fresh independent review, without acceptance:
  [Routing evidence](https://github.com/pcesar22/domes/issues/198#issuecomment-5548306287).
  Do not replay it as a worker missing a judge handoff or demand prohibited physical checks.
- Most older tickets have unmet dependencies; #166/#193 were the other eligible rework packages.
  Never start a cycle without reviewing the complete live candidate set; there is no ID filter.
- Last account check: main allowance 6% consumed / 94% remaining; spare 20% reserve applies.
  Per-package tokens are not yet available. Do not redeem either available reset credit.
- NFF outcome: both programmed and reachable; readiness 0/2 due Heap, unchanged thresholds.
  No physical operations are authorized in initial worker contracts.
- Hardware first desk record: hardware/SETUP_INVENTORY_2026-09-05.md; instruments, owner,
  safe-bench details, phone access, quotes and NFF5 naming conflict remain open.
  Added-node profile acceptance fields and radio-only/full-interaction/coupon boundaries were
  refined this heartbeat and recorded on #199; keep it open for unresolved setup-definition work.
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
- A no-model local rerun selected the already-installed native Codex executable, avoiding the
  Node wrapper, but still failed: bwrap could not find codex-linux-sandbox. No containment setting
  was relaxed and no host change was made. Both failures remain disclosed; no further retry loop.
- Activation head cacc1ba5329b30e7b478efd62cb3e4e9dc1869e1 passed all eight software CI checks
  at 01:24 UTC; On-Device Tests was skipped, not a pass. Later source commits require their own CI.
- New program review: docs/program/review-2026-09-05-first-rework.md, graph/ledger synchronized
  with unchanged milestone states and G1 Hold. Structural validation passed; 17/18 status tests
  pass, with the actual-receipt test correctly rejecting the now-changed sources. status:check
  also refuses the stale receipt. Four changed docs / 20 links and applicable policy hooks pass.
  Generated Site outputs, receipt and deployment are deliberately untouched. The sole scheduled
  publisher must audit these sources, record the matching receipt, refresh, fully validate and
  publish privately. Version 6 remains live in the meantime; this is pending publication, not a
  passed production build. No substantive historical evidence was made fresh merely by this review.
- Publisher handoff at 01:36 UTC supersedes that earlier source-drift result: its memory records
  successful substantive review, refresh/check, all 18 tests, lint, typecheck and production build.
  Its private-source push/deployment was denied by its execution environment, so live version 6
  remains intact. Exact rejection details were not independently retrieved; do not invent a
  permission diagnosis or retry through another channel. Obtain permission review for the sole
  publisher's private push/deploy retry; no audience expansion. Preserve its four unstaged files:
  sites/product-status-dashboard/{public/evidence.html,public/status.json,status/program-status.json,
  status/reviewed-sources.json}. Do not stage, overwrite or regenerate another owner's outputs.
- Activation head bb7421cd5f638fe10a429ada8f695f9e1f650916 has eight successful software checks;
  On-Device Tests skipped. The unrelated local containment failure remains unresolved and is not
  overwritten by remote CI. No broad local retest or host-service changes were attempted this wakeup.
- Slack: activation plus first steering/progress digest sent to Paulo U0BS70FLB3K / self-DM
  D0BRYQMBP5H, message 1788569982.547489. Inbound handling remains unverified; steering stays here.
  [Sent message](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788569982547489).
- Notification keys sent: activation-2026-09-05-v6; decisions-owner-inventory-envelope-initial;
  daily-progress-2026-09-04-Pacific. Do not repeat without a meaningful change or explicit reminder.
- Follow-up sent at 01:12 UTC: both first PRs, exact-head CI passing and read-only judges started;
  message 1788570726.295089, key first-prs-200-201-ci-pass-judges-started.
  [Review-cycle update](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788570726295089).
- Notification key first-judge-rejections-197-cffa-198-876f-repairs-started sent at 01:24 UTC,
  message 1788571469.637529. No duplicate daily digest; prior hardware steering remains open.
  [Rework update](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788571469637529).
- Notification keys second-judge-pass-197-b698-198-876f and publisher-private-write-denied-bb7421c
  sent at 01:56 UTC, message 1788573367.571789. Requested direction in this Codex task for the
  same owner-only publisher retry. Do not repeat the permission request or daily digest on an
  unchanged wakeup; no retry or alternate publishing route without resolved permission.
  [Review and publisher update](https://self-trk7705.slack.com/archives/D0BRYQMBP5H/p1788573367571789).
- App next: final verification on judged head b698a3f9c5bd1fc870bb04052eaf0c1626a08882. The repair handoff retains
  223 passing app tests, including running-dispose and gated connection races, fatal analysis,
  formatting and pinned locked restore. Remote head/base, all 13 allowed paths and CI were checked.
  The formal judge accepted injectable deterministic clock/seed; the explorer withdrew its earlier
  stronger clock allegation. Actual required repairs are direct-running and in-flight disposal
  ownership cleanup plus race tests. Do not resurrect the withdrawn concern as an accepted defect.
- NFF next: worker returned at 01:22 with the same source head, fresh numerical size evidence and
  339/339 tests. Fresh build bin SHA256 8837169017c7a9b032c8e3bdb6fb130ee26116ddae4749db3edeae28811d3e24,
  ELF 8d217dbed8f1993e78d7787b9c9078775306c489d2f46e7592a7f55d5f47a0fd, config unchanged.
  DIRAM 205747/341760 B; IRAM 16383/16384 B; image 1486004 B before padding / 1486128 B binary;
  smallest app partition 1966080 B / 480976 B free. These are worker-retained software results,
  not physical heap measurements. The existing software checks still pass on the unchanged head.
  Pending CI/physical records again mechanically returned it to rework; coordinator documented
  reconciliation and routed it to a fresh judge. Do not rerun another implementation worker for
  those excluded physical checks, erase evidence, or substitute new build hashes in old campaigns.
- Hardware desk audit: the retained size report's 16383/16384 B pure-IRAM row describes the
  0x40374000-0x40378000 subwindow. Its one spare byte is alignment between vectors and text, not
  reusable overall firmware margin. IRAM text extends into dual-mapped DIRAM, competing with data
  and runtime internal heap. DIRAM's 136013 B remainder is static linker accounting, not a measured
  readiness result. The specialist checked the exact v0.1.0-268-g876fdd8 map, ESP-IDF v5.4.4 linker/
  heap source and ESP32-S3 size memory-map definition. The temporary build disappeared after the
  decisive evidence was read; additional configuration cross-checks were unavailable. No build,
  device test, component choice or new hardware acceptance occurred. Retain this boundary in #199.
- NFF fresh judge approved retained source/size evidence at 01:52 UTC. The next step is final
  verification on the same source head; physical checks remain separately unauthorized. Judge
  approval here is an implementation verdict, not a GitHub approval, human merge or product exit.
- After the active verification cycle exits, inspect the complete live queue and PR count before the next
  bounded cycle. Continue existing-PR repair/review only at the six-PR cap. Activation #202 remains
  draft due local verification and pending Site refresh. No approval, merge or product acceptance.
