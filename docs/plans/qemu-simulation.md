# Deliver Deterministic ESP32-S3 QEMU Execution

**Status:** active; implementation complete, required QEMU CI execution pending

**Delivery:** [PR 100](https://github.com/pcesar22/domes/pull/100)

**Packages:** `FS-WP-002B` feasibility and `FS-WP-002D` production composition

## Objective

Establish that the pinned Espressif ESP32-S3 QEMU can execute deterministic target FreeRTOS
behavior, then boot the supported DOMES production runtime through a compile-time QEMU profile
without weakening or contaminating the physical NFF firmware path.

## Delivered Scope

- A standalone target probe for CPU0/CPU1 tasks, cross-core synchronization, target ticks, and a
  controlled GPTimer interrupt.
- A strict Linux runner that pins tool identities, starts fresh snapshot-backed QEMU processes,
  inspects target state, compares signatures, and fails on reset, panic, timeout, drift, or mutated
  media.
- Compile-time-exclusive physical and QEMU composition roots around shared production runtime
  assembly.
- Injected platform identity and random-source interfaces with physical and deterministic QEMU
  implementations.
- Explicit fidelity and task catalogs with generated, hashed build manifests.
- QEMU adapters for bounded LED, touch, IMU, haptic, and audio behavior; unsupported radio,
  transport, OTA, and vendor paths remain disabled.
- Build validators for profile consistency, source and linker closure, direct objects, archive
  identity, disabled symbols, and root separation.
- Host tests and CI coverage for both firmware profiles, required 100-process QEMU execution,
  release packaging, and repository gates.
- Concise architecture, testing, and program-status updates.

## Acceptance

| Gate | Exit |
| --- | --- |
| Feasibility | 100/100 fresh target processes; one structural and normalized signature; both CPUs, synchronization, target time, GPTimer ISR, HMP, and GDB pass |
| Runtime | 100/100 fresh target processes; 9/9 task entries; deterministic readiness scenario; zero panic/reset/drop/failure; complete fidelity and linked closure |
| Physical regression | Exact firmware tree passes two-board serial/BLE diagnostics, self-test, ESP-NOW, trace, and cleanup checks |
| Software | Host firmware, CLI, simulation tooling, Flutter, physical/QEMU ESP-IDF builds, 100-process exact-checkout QEMU runtime execution, release packaging, and CI gate pass |
| Reviewability | One PR against `main`; generated logs and binaries excluded; manual campaign outcomes recorded in the PR and automated results retained by CI |

## Generated Output Policy

The repository contains source, tests, reproducible commands, CI definitions, architecture, and
current program status. It does not contain generated run directories, raw logs, build trees, ELFs,
flash images, SDKCONFIG dumps, campaign reports, or trace captures.

- Local generated output goes under ignored `.artifacts/` or `/tmp`.
- CI results are retained in GitHub Actions logs; generated diagnostics are uploaded on failure.
- The PR records manual campaign outcomes, CI retains automated build and test results, and
  `PROGRAM_STATUS.md` records only current state and the bounded claim.

## Boundaries

- The physical profile remains the default and preserves physical startup order and vendor inputs.
- Runtime profile selection is compile-time only.
- Shared services do not contain scattered QEMU conditionals.
- Target FreeRTOS and target time remain authoritative; host wall time is not substituted.
- QEMU does not initialize unsupported production radio, BLE, WiFi, USB, or OTA stacks.
- No scheduler-coverage, RF fidelity, peripheral-actuation, hardware-equivalence, or predictive
  claim is created.

## Next Package

`FS-WP-002C`, scheduler/ISR/synchronization causality and trace normalization, becomes eligible when
the required QEMU execution check and aggregate `CI Gate` pass. It has not been selected or started;
it requires a separate execution cycle and must preserve the claim boundaries above.
