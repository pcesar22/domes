# Deliver Scheduler And Causality Trace Evidence

Historical source-bound scheduler-trace evidence. Implementation is merged through
PR105, but distinct final-source physical acceptance remains open in
[issue101](https://github.com/pcesar22/domes/issues/101). Candidate results below do
not substitute for that final acceptance or current NFF readiness.

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

- [x] Map existing trace ABI, task creation paths, supported ESP-IDF/FreeRTOS hooks,
  synchronization objects, synthetic interrupt path, and generated consumers.
- [x] Implement stable manifest IDs and bounded target hook capture.
- [x] Implement raw artifact hashing, normalization, causal validation, and rejection tests.
- [x] Run focused host tests and isolated ESP-IDF v5.4.4 physical/QEMU builds.
- [x] Run the original 100-process QEMU identity/overhead acceptance and bounded registered-NFF
  runtime capture; retain the NFF result as pre-fix history after review changed the implementation.
- [x] Complete corrective review, push implementation `5ea561c`, and pass exact-checkout Software CI
  run 31067343275, including the accepted 100-process QEMU job and aggregate `CI Gate`.
- [x] Capture and normalize a fresh registered-NFF trace from final implementation `b3cb19c`, then
  restore the default image without making physical-output claims.
- [x] Add device-originated firmware/image/hardware identity, candidate-image verification, and
  transport identity to the retained trace session; pass focused host and paired-profile builds.
- [x] Bind the retained physical session to the exact flashed-image hash and stable hardware
  identity instead of relying on operator-correlated commands.
- [x] Retain the post-restoration command evidence and pass the repository-required `system health`
  and complete `system self-test` checks before technical exit.
- [x] Reclaim the redundant 16 KiB trace merge buffer without changing event ordering, then measure
  the default image again instead of weakening either heap threshold.
- [x] Bind reviewed PR 105 candidate `2772f633` to retained device/image identity, causal trace,
  two-boot default restoration, and all eight required software checks.
- [ ] Complete the distinct final-source physical trace and default-restoration acceptance in issue101.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | Full pre-commit suite; 53 focused Python tests; 294 host firmware tests; 98 Rust unit and 10 integration tests plus clippy; generated protocols; trace generator; fresh ESP-IDF v5.4.4 physical build | passed on final implementation head `b3cb19c`; the final head's documentation-only delta was also covered by exact-checkout CI |
| Superseded closure implementation | Generated nanopb/Dart drift checks; 296 host-firmware tests; 100 Rust unit and 10 integration tests plus clippy; 22 trace-tool tests; pre-commit hooks; fresh ESP-IDF v5.4.4 physical and QEMU builds; paired-profile validator | Historical commit `ce159ba0c10610648caa0b7a158abc9dc6ff5d95` passed these checks, but it is not an ancestor of reviewed PR 105 candidate `2772f633` and is not evidence for that candidate. Its artifact hashes are intentionally omitted here to prevent them from being mistaken for the reviewed closure. |
| Target execution | `tools/simulation/qemu_runtime.py` final fixed 100-run trace campaign | passed 100/100 in Software CI run 31068033646 with ready signature `505b529d...de1be` and trace signature `a9800774...85d4`; separate target run passed with 68 events, zero drops/discontinuities, and 101/160 us disabled/enabled measurements |
| Historical accepted command | pre-fix serial raw trace dump and normalization from an NFF test device | retained: 74 events, SHA-256 `5a37fe73...0505`, zero drops/discontinuities, complete causal chain, and 92/177 us measurements; this predates `5ea561c` and does not verify the corrected implementation |
| Registered-NFF target execution | fresh isolated ESP-IDF v5.4.4 probe build and serial capture, operator-correlated to the NFF test device and final PR head `b3cb19c` | trace semantics passed: 75 events, zero drops/discontinuities, one complete causal chain at raw positions 49/58/59/62/64/65/66/69/71/72, 12 task mappings, 6 object mappings, and 154/267 us disabled/enabled overhead for 32 records; raw SHA-256 `b3232cf4eb39ddaa69168b7503c16395bdb9032f0c0e44bcb64c7039b92d98ec`, normalizer-declared content SHA-256 `78d2694b97aec7a22a15576b2f3b0d8c4fde1be6baf55c823b1c234577177225`. The session itself does not bind the image hash or CP2102N identity, so it is not final physical-differential evidence |
| Default-image restoration | fresh isolated default build from `b3cb19c`, `domes.bin` SHA-256 `7fe4124c1a6e89b3adb1beb4182115acd46f8957cba11a57ae3f083d171937df`, flashed back to the operator-correlated NFF test device | observed firmware `v0.1.0-27-gb3cb19c`, idle mode, disabled/empty trace, and restored default feature mask, but the command outputs were not retained. Required verification did not pass: `system health` missed its 16 KiB current/minimum-free-heap thresholds and `system self-test` passed 9/10, missing its separate 30 KiB heap threshold. The old-main pre-capture health value was also below 16 KiB, but that does not prove non-regression or satisfy restoration acceptance |
| Device-bound registered-NFF closure | exact reviewed PR 105 candidate `2772f633b57663f731bbf30856802ac51862ae42`, the NFF test device, and its stable CP2102N endpoint | passed: 74 events, zero drops/discontinuities, complete causal chain at raw positions 50/59/60/63/65/66/67/69/71/72, 12 task mappings, 6 object mappings, and 153/239 us disabled/enabled overhead for 32 records. The session verifies device identity, version `v0.1.0-31-g2772f63`, ELF SHA-256 `ab499b0de770add4eb8e6989d7783238c9ed7d2b3d038f41a9d341b9d00c1b5b`, running-image SHA-256 `8d549e706cf19d92a7f455da1a72c73a7c3c2c141e48fa82d106ac1267a69116`, and candidate-file SHA-256 `a089546cd290ebed99fbeddd2cb0951f0cca7e16d6d4f0dbcd22b0cc4f82d084`. Raw SHA-256 is `df0334afe9ba8d5c697f45b302e75493047b20bc7acb12f1652c6a74a34cfc2f`; replay and semantic projections are `8e47b196edafe6dc797706ec68353523d421d383a714bf2e503c8c6b16dcd2ac` and `0695b88015de7cf328adfb39ffe76ee72bc2d20279edfe37318b1a78a8ed400d` |
| Passing default-image restoration | exact reviewed candidate `2772f633b57663f731bbf30856802ac51862ae42` default app restored to the NFF test device, followed by boots 39 and 40 | passed twice: tracing disabled, zero events/drops/discontinuities, `system health` pass, and `system self-test` 10/10. This is command-visible device evidence, not physical-actuation evidence |
| Reviewed-candidate required CI | [Software CI run 31918252989](https://github.com/pcesar22/domes/actions/runs/31918252989) on exact PR 105 head `2772f633b57663f731bbf30856802ac51862ae42` | passed all eight required jobs, including the 100-process QEMU runtime campaign and aggregate `CI Gate`; hardware CI intentionally skipped |
| PR merge-ref checkout | [Software CI run 31068033646](https://github.com/pcesar22/domes/actions/runs/31068033646) on merge ref `b451b118` formed from final PR 102 head `b3cb19c` and base `d4251d5` | passed: all eight software checks, 100 identical accepted QEMU processes with trace signature `a98007744af8b34395141cd9c3303bee95d5e983f2531b5cd0a1073c1ada85d4`, and aggregate `CI Gate`; hardware CI intentionally skipped |

## Decisions, discoveries, and deviations

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
- Physical capture metadata identifies the NFF test device but does not embed the CP2102N serial or firmware hash.
  The registered identity and restored idle/trace-disabled state were independently corroborated;
  neither is promoted to cryptographically bound artifact evidence.
- The final registered-NFF artifact was captured after PR 102 reached its accepted head. Its raw,
  session, and normalized files bind trace content, but not the flashed-image hash or stable CP2102N
  identity required by the physical-differential contract. Operator correlation is insufficient for
  technical exit.
- Default-image verification has two distinct heap thresholds: `system health` requires 16 KiB for
  current and historical minimum free heap, while the on-device self-test requires 30 KiB current
  free heap. Both required checks failed after restoration. The prior main image also missed the
  health threshold, but no retained pre/post record proves non-regression and a prior failure is not
  a passing restoration result.
- The physical evidence record will use device-originated base-MAC, firmware-version, ELF-hash, and
  running-app-image-hash fields. When given the candidate `domes.bin`, the CLI will verify its
  embedded application descriptor and appended image hash against those device fields, then retain
  the artifact's full-file SHA-256 and the selected transport endpoint in the same session record.
- The two per-core capture buffers are required for bounded SMP hook recording, but the additional
  16 KiB global merge buffer is not. Stable in-place sorting per core followed by a stable two-way
  merge preserves the existing timestamp and equal-timestamp ordering contract while returning that
  internal DRAM to the default image.
- Two guarded serial OTA attempts of the exact probe and default candidates completed transfer and
  rebooted, but the bootloader rolled back before framed commands became available. Directly writing
  the same verified default app to the already-active `ota_0` slot then passed health and the 30 KiB
  self-test twice. This does not invalidate the trace/default-restoration exit evidence, but it is a
  separate OTA verification-timing or heap-margin defect; no successful-OTA claim is made here.

## Remaining acceptance

Candidate `2772f633` has retained device-bound trace evidence, two passing restored
boots and passing required software CI. The later source reconciliation changed
firmware identity; affected trace and restoration checks remain unaccepted for that
final source. Issue101 retains the required repeat and independent acceptance.
Guarded-update behavior remains a separate issue; this trace record makes no
successful-update claim.
