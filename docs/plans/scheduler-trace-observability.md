# Deliver Scheduler And Causality Trace Evidence

Status: implementation review complete; final registered-NFF evidence and merge pending
Current phase: corrected implementation and exact-checkout software CI passed; physical rerun open
Repository state: reviewed implementation `5ea561c` on `codex/feat/scheduler-trace-observability`
Last updated: 2026-08-05; current software/QEMU passed, earlier NFF artifact is pre-fix

## Objective and observable outcome

Deliver FS-WP-002C so the physical and QEMU firmware profiles emit the same bounded trace ABI for
task, ISR, synchronization, timeout, callback, and causal behavior. A retained raw trace and a
versioned normalizer must prove complete deterministic identity across 100 fixed QEMU runs, reject
invalid evidence, and report enabled/disabled overhead without creating a hardware-equivalence or
predictive claim.

## Authorities and contracts

- Authority: `research/architecture/13-deterministic-virtual-platform.md` - FS-WP-002C behavior,
  evidence, and stop condition.
- Authority: `firmware/common/proto/trace.proto` - event names, values, and generated consumers.
- Authority: `firmware/domes/main/trace/` - the 16-byte event ABI and device trace capture.
- Authority: `firmware/domes/profiles/runtime_profiles.json` and
  `tools/simulation/qemu_runtime.py` - QEMU profile and accepted run artifacts.
- Preserve: the 16-byte `TraceEvent` ABI, physical/QEMU composition separation, target FreeRTOS and
  target time, bounded allocation-free hook/ISR recording, and current protocol envelopes.
- Stop before: replacing FreeRTOS scheduling, unbounded kernel/ISR work, reserved-value reuse, an
  unmeasured ABI migration, FS-WP-002E radio-seam work, or hardware-equivalence/predictive claims.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Trace schema/ABI | `firmware/common/proto/trace.proto`, generated nanopb/prost/Dart if needed, `firmware/domes/main/trace/` | Define non-reserved event semantics and bounded metadata without changing the event size |
| Runtime integration | FreeRTOS config/hooks, task/runtime composition, QEMU adapters | Stable IDs and balanced task/ISR/synchronization/callback events in both profiles |
| Evidence tooling | `tools/simulation/qemu_runtime.py` and focused trace normalizer/tests | Raw hashing, versioned normalization, causal validation, and fail-closed acceptance |
| Verification | host tests, physical/QEMU builds, QEMU campaign, NFF campaign | Determinism, invalid-evidence rejection, overhead, and disabled-trace preservation |
| Program authority | `PROGRAM_STATUS.md` | Selection, evidence, state, risks, and next execution pointer |

## Stages and dependencies

- [x] Reconcile `main`, merged deliveries, open issues/PRs, and select FS-WP-002C.
- [x] Create issue 101 and isolated branch/worktree.
- [x] Map existing trace ABI, task creation paths, supported ESP-IDF/FreeRTOS hooks,
  synchronization objects, synthetic interrupt path, and generated consumers.
- [x] Implement stable manifest IDs and bounded target hook capture.
- [x] Implement raw artifact hashing, normalization, causal validation, and rejection tests.
- [x] Run focused host tests and isolated ESP-IDF v5.4.4 physical/QEMU builds.
- [x] Run the original 100-process QEMU identity/overhead acceptance and bounded registered-NFF
  runtime capture; retain the NFF result as pre-fix history after review changed the implementation.
- [x] Complete corrective review, push implementation `5ea561c`, and pass exact-checkout Software CI
  run 31067343275, including the accepted 100-process QEMU job and aggregate `CI Gate`.
- [ ] Capture and normalize a fresh registered-NFF trace from implementation `5ea561c` (or a
  docs-only descendant), then restore and verify the default image without making physical-output
  claims.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | Full pre-commit suite; 53 focused Python tests; 294 host firmware tests; 98 Rust unit and 10 integration tests plus clippy; generated protocols; trace generator; fresh ESP-IDF v5.4.4 physical build | passed on reviewed implementation `5ea561c` |
| Target execution | `tools/simulation/qemu_runtime.py` current fixed 100-run trace campaign | passed 100/100 in Software CI with one ready signature `45b54e96...0964` and one trace signature `e8a211a3...8fced`; separate final local development run passed with 68 events, zero drops/discontinuities, and 101/160 us disabled/enabled measurements |
| Historical accepted command | pre-fix serial raw trace dump and normalization from registered NFF pod 2 | retained: 74 events, SHA-256 `5a37fe73...0505`, zero drops/discontinuities, complete causal chain, and 92/177 us measurements; this predates `5ea561c` and does not verify the corrected implementation |
| Physical/runtime boundary | final registered-NFF capture and default-image restoration on the corrected implementation | not run; required before technical exit, with no actuation, RF, hardware-equivalence, or predictive claim |
| Exact checkout | [Software CI run 31067343275](https://github.com/pcesar22/domes/actions/runs/31067343275) on PR 102 head `5ea561c` (merge ref `f5c2d2b`) | passed: eight software checks including fresh physical/QEMU builds, 100-process accepted QEMU execution, and aggregate `CI Gate`; hardware CI intentionally skipped |

## Decisions, discoveries, and deviations

- FS-WP-002C was selected because FS-WP-002D is merged and green, no eligible package is open, and
  the program ledger names C as the next autonomous execution delivery.
- Issue 101 is the only execution issue authorized by this continuation cycle.
- ESP-IDF v5.4.4 compiles the forced hook header through the SMP FreeRTOS kernel and links the
  QEMU profile. Hooks use passed TCB pointers plus bounded DRAM registries protected by
  cache-disabled-safe critical sections; they do not call flash-resident FreeRTOS task APIs.
- The existing 350 ms readiness drill is too broad for bounded scheduler capture. A short GPTimer
  interrupt-to-production-main-task probe will run after readiness, retain raw events, and measure
  tracing separately without changing readiness behavior.
- The retained raw artifact is written and hashed before semantic interpretation. The normalizer is
  versioned, retains every event field, rejects incomplete mappings/lifecycles/causal chains and any
  drop or discontinuity, and produces separate replay and cross-target semantic projections.
- Corrective review made task/object identity immutable, bounded host allocation and chunk
  retention, bound session catalogs/timestamps to raw events, and required strictly positive
  enabled-over-disabled overhead. These behavioral changes invalidate the earlier NFF artifact as
  final implementation evidence even though it remains useful pre-fix history.
- Physical capture metadata identifies pod 2 but does not embed the CP2102N serial or firmware hash.
  The registered identity and restored idle/trace-disabled state were independently corroborated;
  neither is promoted to cryptographically bound artifact evidence.

## Resume checkpoint

With explicit device authorization, capture and normalize the registered-NFF trace for reviewed
implementation `5ea561c`, restore and query the default image, then update evidence and complete PR
102 review. Do not merge or select FS-WP-002E until that acceptance gap is closed.
