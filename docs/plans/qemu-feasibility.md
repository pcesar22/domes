# Prove ESP32-S3 QEMU Feasibility

Status: complete
Current phase: FS-WP-002B closed; retained evidence and status independently accepted
Repository state: implementation commit `b12dbddeac64397b0b15e9970251694f40632cf9` has a retained
100-run acceptance package under `docs/evidence/qemu-feasibility/`
Last updated: 2026-08-04; binary disposition `Viable`

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
- [x] Freeze implementation commit `b12dbdd`, run 100 fixed deterministic repetitions, and measure
  cold/cached build and execution time.
- [x] Publish the fidelity inventory, numeric patch/maintenance budget, and binary `Viable`
  disposition.
- [x] Run repository checks after populating the pinned nanopb submodule and keeping host-tool and
  ESP-IDF Python environments separate.
- [x] Independently review retained evidence/status; all findings resolved and the package accepted.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Probe build | Acceptance runner cold and cached builds | passed: 7.096 s cold, 0.560 s cached |
| Target execution | `python3 tools/simulation/qemu_feasibility.py --runs 100 ...` | passed: HMP/GDB and 100/100 processes |
| Determinism | Retained `report.json` and raw logs | passed: one structural and one normalized observation signature |
| Runner unit tests | `python3 -m unittest tools/simulation/test_qemu_feasibility.py -v` | [31 passed](../evidence/qemu-feasibility/runner-unit-tests-b12dbdd.log) |
| Repository software | `scripts/verify.sh --component firmware --component tooling --component docs` | [passed](../evidence/qemu-feasibility/repository-verification-b12dbdd.log): 283 host tests, host tooling/docs, production ESP-IDF build with 27% app-partition headroom |
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
- The clean immutable campaign reported `invocation_status=SUCCEEDED` and
  `acceptance.status=PASS`. Its SHA-256 manifest passed all 112 listed artifacts; the manifest is the
  113th package file and necessarily does not list itself.
- The technical disposition is `Viable`. It makes D eligible but does not select D or establish any
  production-firmware, scheduler-trace, radio, cycle-accuracy, CI, or predictive claim.
- The first scoped repository-verification attempt exposed an uninitialized nanopb submodule and a
  shell-contaminated Python path. After populating the recorded submodule commit and running host
  tooling outside the ESP-IDF environment, the same selected verification passed completely.

## Resume checkpoint

FS-WP-002B is closed. FS-WP-002D is the next eligible simulation package, but it must be separately
selected; do not expand B into D.
