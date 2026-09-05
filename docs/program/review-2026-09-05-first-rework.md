# First independent review and bounded rework

Reviewed 2026-09-05 01:22 UTC. Management baseline cacc1ba5329b30e7b478efd62cb3e4e9dc1869e1;
software contracts still pin main 3b62a6c82160d0271b276bb42894e3d3bb69761e. The unmerged
runtime candidates are not imported into this management checkout or accepted as product behavior.

## Direct evidence and disposition

- [App judge](https://github.com/pcesar22/domes/issues/197#issuecomment-5548323912) rejected
  PR #201 at cffa30e3fccbfe5558ee4b4e7c3ab862351f3c46: disposal can race an in-flight connection,
  lose ownership of its supplied transport and leave resources alive. Required rework is complete
  teardown plus running/in-flight disposal regression tests. The judge accepted the injectable
  clock/seed criterion; the earlier exploratory clock concern is not an established defect.
- [Firmware judge](https://github.com/pcesar22/domes/issues/198#issuecomment-5548328290) rejected
  PR #200 at 876fdd8b40f22175341671bff1d303d956376ebf solely for missing numerical build-size
  evidence in the retained handoff/repository artifact. The public PR body contains size numbers,
  but the judge requires a durable record tied to source, image/ELF/config hashes and isolated
  ESP-IDF v5.4.4 provenance. No implementation or physical failure is inferred from that omission.
- Both original heads passed all eight software CI checks. That does not override either rejection;
  every changed repair head needs fresh review and exact-head CI. PR identity, base and descriptions
  were independently read from GitHub; the labels alone are not this evidence.

At approximately 01:18 UTC a third finite controller cycle started. Both issue-197 and issue-198
workspace-write repair processes were directly observed at 01:19 UTC. Clean ministrom, current
contracts, authentication, valid controller, no duplicate process and full queue order were checked
first. Each ticket explicitly binds its existing PR; six open PRs leave no new-PR capacity.
No watch/autopilot, device opt-in, host-service change or approval/merge occurred.

## Hardware desk progress and unchanged gaps

The setup inventory now contains a supported-profile acceptance checklist and separates retained
NFFs, four radio-only additions, four full-interaction additions and product-risk coupons. The
independent source review used current profile/defaults/partition and setup authorities, not vendor
or ownership assumptions. No supplier, price, possession, calibration or electrical limit is claimed.
Qualified ownership, instruments, phone/Mac access, product envelope and NFF5 clarification remain open.

## Validation and publication boundary

Main account allowance was 5% consumed / 95% remaining; no reset was used. The local no-model
containment test remains failed: the standard wrapper cannot find Node; selecting the already
installed native executable instead fails to locate codex-linux-sandbox. Neither alternative
relaxes containment, and no software fix or host modification was made. Activation PR #202 stays
draft; its own CI is separate from this local failure and was still pending at inspection.

FS-WP-004A, NFF-MEM-001 and HW-WP-002 remain Active, not accepted. All product/physical milestones,
P1/Pre-EVT/Amber and G1 Hold are unchanged. The two boards' retained readiness is still 0/2.
This source update awaits the sole scheduled Site publisher's source audit, matching receipt,
deterministic refresh, validation and private deployment. Keep live version 6 intact until then;
do not update its receipt merely to suppress expected source drift.
