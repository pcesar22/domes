# Final review of two bounded software exits

Reviewed 2026-09-05 02:30 UTC. Management baseline
2f73d135a1cd486736dbec6bdc8cca5f6eb0d3f8; both software contracts pin main
3b62a6c82160d0271b276bb42894e3d3bb69761e. Candidate acceptance below is confined to the named
immutable software artifacts. Neither is merged, imported into this management checkout or installed
on either NFF. Source identity and required results, not GitHub labels, establish this disposition.

## App virtual-pod lab: FS-WP-004A Complete

[PR #201](https://github.com/pcesar22/domes/pull/201), head
b698a3f9c5bd1fc870bb04052eaf0c1626a08882, passes the bounded app-virtual-model exit:

- Two/six stable virtual identities use the real app repository/providers and generated-message path.
- Seed/time controls reproduce modeled results; this is not production-runtime or RF prediction.
- Direct running disposal and disposal during a gated in-flight connection retain ownership and
  clean up the supplied transport; the formerly rejected lifecycle race has regression coverage.
- The retained pinned Flutter 3.44.8 checks pass: 223 tests, fatal analysis, formatting and locked restore.
- The [fresh judge](https://github.com/pcesar22/domes/issues/197#issuecomment-5548550518)
  approves seven criteria. [Final verification](https://github.com/pcesar22/domes/issues/197#issuecomment-5548574905)
  at 01:56:56 UTC reports no repairs or blockers and binds the same head.
- All eight required software checks pass in
  [exact-head CI](https://github.com/pcesar22/domes/actions/runs/33936222944), independently queried
  from commit check runs. On-device tests were skipped, not passed.

No software criterion remains open for A. Human review/merge remains a separate commitment.
Full offline/fault journeys belong to B; production-simulator semantic parity belongs to C;
real phones, radio timing and six physical nodes retain their separate acceptance boundaries.

## NFF memory repair candidate: NFF-MEM-001 Complete

[PR #200](https://github.com/pcesar22/domes/pull/200), head
876fdd8b40f22175341671bff1d303d956376ebf, passes the software-candidate exit:

- The allocation rationale replaces a 32,000-byte persistent internal tone buffer with a bounded
  512-byte static buffer, a 31,488-byte static allocation reduction, not measured runtime recovery.
- Six focused regressions and all 339 host tests pass, including long-tone chunking and error/partial
  writes. Readiness thresholds and allocation-failure handling are not weakened.
- A fresh isolated ESP-IDF v5.4.4 NFF build retains numerical size and exact configuration/artifact
  identity. The first judge's missing-size-provenance rejection is closed by retained evidence,
  not by changing the already-reviewed source.
- The [fresh judge](https://github.com/pcesar22/domes/issues/198#issuecomment-5548549998)
  approves seven criteria. [Final verification](https://github.com/pcesar22/domes/issues/198#issuecomment-5548572289)
  at 01:56:26 UTC reports no repairs or blockers and binds the same head.
- All eight required software checks pass in
  [exact-head CI](https://github.com/pcesar22/domes/actions/runs/33934639219), independently queried.

Retained build identities:

| Artifact | SHA-256 |
| --- | --- |
| Binary | 8837169017c7a9b032c8e3bdb6fb130ee26116ddae4749db3edeae28811d3e24 |
| ELF | 8d217dbed8f1993e78d7787b9c9078775306c489d2f46e7592a7f55d5f47a0fd |
| SDKCONFIG | 5b26571b1387b69ab0610462b58e928883c8c21ba5998a541db4e2bec2a9bf17 |

DIRAM: 205,747 / 341,760 bytes. Pure IRAM subwindow: 16,383 / 16,384 bytes. The one-byte alignment
hole is not overall firmware headroom; IRAM extends into dual-mapped DIRAM and competes with
internal data/heap. Static DIRAM remainder is not a physical free-heap measurement. Binary size is
1,486,128 bytes; the smallest 1,966,080-byte app partition has 480,976 bytes free.

The repair is uninstalled. The retained physical campaign remains 9/10 self-tests and **0/2 ready**;
LAB0 and the separately authorized NFF1 campaign must establish actual readiness/recovery. No
peripheral, measured-margin, OTA rollback, HR0 or product-gate claim follows from this software exit.

## Dispatch and remaining gaps

The 02:16 UTC operational check found the fifth finite controller cycle ended and both issues in
human review. Clean main, exact PR heads/base and current checks were inspected; neither PR was
approved or merged by the coordinator. Six open PRs (#190/#191/#194/#200/#201/#202) exhaust capacity.
FS-WP-004B changes from Not due to Blocked: A's technical evidence now passes, but no new-PR slot
exists and its accepted artifact is unmerged. An eligible reviewed baseline and bounded contract
are required before dispatch. No unreviewed dependency-stack exception is introduced.

Read-only review also rejected dispatch of legacy reworks:

- #166 / PR #190: latest structured worker result names 4c5288f7, while the PR is 66e9a801.
  Its explicit prerequisite blocker requires accepted #164/#171 execution evidence/attestation.
  There is no current structured judge handoff.
- #193 / PR #194: handoff and head agree at 274077e4, but required #154/#155/#174–#176 outputs
  are not integrated on one exact commit and four physical alpha nodes are unavailable. Its base
  remains 0f1659c6, behind main. There is no current structured judge handoff.
- Both reserve tools/simulation; they cannot execute together. Passing CI and mechanical rework
  labels do not repair these input, identity or resource gaps. Neither was dispatched.

The hardware desk record and supported-profile alternatives remain delivered, not full setup
acceptance. Qualified ownership, instruments/calibration, phone/Mac access, product envelope,
quotes and NFF5 clarification remain open. P1/Pre-EVT/Amber, G1 Hold, HR releases and physical
milestones are unchanged. Product-definition evidence remains dated August 3.

## Validation, usage and publication boundary

Account allowance at the coordination check was 6% consumed / 94% remaining, shared across the
account; exact package tokens are unavailable. No reset, device operation or service change occurred.
Two validator tests now explicitly construct an unmet prerequisite instead of assuming A will
remain unfinished forever. Their fail-closed assertions and production validation rules are unchanged.

This update passes graph/source structural validation, 17 status tests, lint, typecheck and the
changed-document link check. The eighteenth test and production build stop on the intentionally
stale publisher receipt: `Milestone model changed without a matching evidence review`. An initial
sandbox Git-inventory subprocess denial was resolved by a permitted read-only validation rerun;
it did not authorize a private upload. No successful current-source production build is claimed.

The sole publisher's 01:36 UTC record reports its previous source review, refresh/check, 18 tests,
lint, typecheck and build passed, but the private upload/deployment was denied. Live version 6 stays
intact. Permission for that same-owner-only retry is still unresolved; no retry, audience change or
alternate route was attempted. This newer review intentionally invalidates the previous receipt;
the publisher must audit it and fully validate again before any upload. Its four generated/receipt
files are preserved, not overwritten or staged by the coordinator.

The earlier local containment diagnostic remains failed (Node unavailable to the wrapper, then
codex-linux-sandbox unavailable to the native alternative); no guard was relaxed. Activation head
2f73d13 passed eight remote software checks, with on-device tests skipped, but that does not erase
the local failure or validate a later head. Activation PR #202 remains draft.
