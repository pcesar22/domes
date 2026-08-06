# Deliver Scheduler And Causality Trace Evidence

Status: implementation complete; review and required exact-checkout CI pending
Current phase: publish review-ready delivery and qualify its final commit
Repository state: `codex/feat/scheduler-trace-observability` based on `d4251d5`
Last updated: 2026-08-05; local software, 100-process QEMU, and bounded NFF runtime evidence passed

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
- [x] Run 100-process QEMU identity/overhead acceptance and bounded registered-NFF runtime capture.
- [ ] **Current:** Self-review, commit, push, open the linked review-ready PR, and monitor required
  exact-checkout CI.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | 67 focused Python tests; 289 host firmware tests; 91 Rust unit and 10 integration tests; generated protocols; fresh ESP-IDF v5.4.4 physical/QEMU builds | passed; `scripts/verify.sh` code gates passed, while host tooling could not run because `pre-commit` is absent and Flutter 3.38.9 is older than required 3.44.8 |
| Target execution | `tools/simulation/qemu_runtime.py` fixed 100-run trace campaign | passed 100/100 with one ready signature `dc56c865...51b3`, one 62-event trace signature `4bb920d7...e8d`, zero drops/discontinuities, and 59/130 us disabled/enabled 32-record measurements; ignored report `/tmp/tmp.8Ru93ygX7k/evidence/runtime-report.json` |
| Accepted command | serial raw trace dump and versioned normalization from registered NFF pod 2 | passed: 74 events, SHA-256 `5a37fe73...0505`, zero drops/discontinuities, complete causal chain, and 92/177 us disabled/enabled 32-record measurements; target-runtime/framed-command evidence only |
| Physical/runtime boundary | independent read-only evidence review plus post-capture pod query | accepted with qualification: pod identity is corroborated rather than bound into the artifact; default-image restoration is corroborated, not proven by a retained flash hash; no actuation, RF, hardware-equivalence, or predictive claim |

## Decisions, discoveries, and deviations

- FS-WP-002C was selected because FS-WP-002D is merged and green, no eligible package is open, and
  the program ledger names C as the next autonomous execution delivery.
- Issue 101 is the only execution issue authorized by this continuation cycle.
- ESP-IDF v5.4.4 compiles the forced hook header through the SMP FreeRTOS kernel and links the
  QEMU profile. Portable callbacks resolve the current task through the public task API because
  ESP-IDF keeps the per-core current-TCB array private.
- The existing 350 ms readiness drill is too broad for bounded scheduler capture. A short GPTimer
  interrupt-to-production-main-task probe will run after readiness, retain raw events, and measure
  tracing separately without changing readiness behavior.
- The retained raw artifact is written and hashed before semantic interpretation. The normalizer is
  versioned, retains every event field, rejects incomplete mappings/lifecycles/causal chains and any
  drop or discontinuity, and produces separate replay and cross-target semantic projections.
- Physical capture metadata identifies pod 2 but does not embed the CP2102N serial or firmware hash.
  The registered identity and restored idle/trace-disabled state were independently corroborated;
  neither is promoted to cryptographically bound artifact evidence.

## Resume checkpoint

Commit and publish the review-ready FS-WP-002C delivery, then require Software CI to rebuild and run
the exact PR head before changing the package from `Active` / `Amber` to `Complete` / `Green`.
