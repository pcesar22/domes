# Prove ESP32-S3 QEMU Feasibility

Status: active
Current phase: bounded target probe
Repository state: implementation candidate prepared on `codex/feat/qemu-feasibility`; the retained
100-run campaign must bind to the next immutable implementation commit
Last updated: 2026-08-04; ESP-IDF v5.4.4 and its pinned QEMU 9.2.2 package were discovered locally

## Objective and observable outcome

Return a binary `Viable` or `Not viable` result for FS-WP-002B. A passing result boots a standalone
ESP32-S3 image under the ESP-IDF-pinned QEMU, proves pinned task progress on both target CPUs, a
cross-core block/wakeup, tick progress, and a controlled GPTimer interrupt, then reproduces one
canonical result signature 100 times. This package does not implement the DOMES QEMU board profile,
virtual radio, production composition root, scheduler trace, or predictive model.

## Authorities and contracts

- Authority: `research/architecture/13-deterministic-virtual-platform.md` - package acceptance,
  claim boundary, and stop condition.
- Authority: `PROGRAM_STATUS.md` - selected package, state, and final disposition.
- Preserve: `firmware/domes` production build, startup order, task topology, protocols, and physical
  behavior remain untouched.
- Preserve: ESP-IDF v5.4.4 and the QEMU revision selected by its checked-in `tools.json` are immutable
  inputs to the result.
- Stop before a QEMU fork, custom DOMES device, production transport refactor, required CI lane, or
  broad peripheral model.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Feasibility firmware | `firmware/qemu_probe/` | Standalone dual-core, block/wake, tick, and interrupt probe |
| Linux runner | `tools/simulation/qemu_feasibility.py` | Build, inspect, repeat, compare, and retain evidence |
| Runner tests | `tools/simulation/test_qemu_feasibility.py` | Validate parsing, command construction, and mismatch handling |
| Evidence | `docs/evidence/qemu-feasibility/` | Versioned inventory, measurements, signatures, and disposition |
| Program status | `PROGRAM_STATUS.md` | Mark B active, then record only evidence-supported exit state |

## Stages and dependencies

- [x] Architecture and package contract accepted locally in commit `852dffb`.
- [x] ESP-IDF v5.4.4 found; pinned QEMU package is
  `esp_develop_9.2.2_20250817`.
- [x] Build and run the isolated probe once; retain target, monitor, and GDB evidence.
- [ ] **Current:** Freeze the implementation commit, then run 100 fixed deterministic repetitions
  and measure cold/cached build and execution time.
- [ ] Publish the fidelity inventory, numeric patch/maintenance budget, and binary disposition.
- [ ] Run repository checks and update the resume checkpoint.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Probe build | `python3 tools/simulation/qemu_feasibility.py --build-only` | pending |
| Target execution | `python3 tools/simulation/qemu_feasibility.py --runs 1 --allow-dirty` | passed during development; immutable rerun pending |
| Determinism | `python3 tools/simulation/qemu_feasibility.py --runs 100` | pending |
| Runner unit tests | `python3 -m unittest tools/simulation/test_qemu_feasibility.py -v` | 31 passed |
| Repository software | `scripts/verify.sh --component firmware` plus the package-specific runner | pending |
| Physical hardware | Not required | B makes no physical-device or peripheral claim |

## Decisions, discoveries, and deviations

- The Arch host lacks a system-installed `libslirp.so.0`; the downloaded ESP-IDF QEMU binary runs
  with the unmodified Arch `libslirp` package extracted into a temporary library path. A system
  package install remains the reproducible operator setup. This is an environment prerequisite, not
  a QEMU source patch.
- GPTimer is the controlled interrupt because Espressif's ESP32-S3 QEMU feature matrix declares
  Timer Groups supported and the IDF callback contract executes in ISR context.
- The 100-run signature contains stable structural outcomes. Raw boot ticks and timer counts remain
  retained observations but cannot be silently discarded from the evidence package.
- Independent implementation review required distinct invocation/acceptance status, end-of-campaign
  identity revalidation, signal-safe cleanup, retryable debug endpoints, disjoint output paths, and
  an enforced compiler-package pin. All six items are implemented; a hardened development smoke run
  passed with `acceptance.status=NOT_ELIGIBLE` as intended.

## Resume checkpoint

Commit the probe, runner, tests, and operator documentation. Execute the clean commit for 100 fresh
QEMU processes, retain both signature classes plus monitor/GDB and media-integrity evidence, then
publish the binary disposition and status update. Do not expand into FS-WP-002D from this package.
