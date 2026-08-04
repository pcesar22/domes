# DOMES Product Realization Milestones

This is the delivery-status authority for DOMES. The lifecycle and transition rules are defined in
[`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](../docs/PRODUCT_REALIZATION_FRAMEWORK.md). Product targets
belong in [`research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md), product
hypotheses in [`research/PRODUCT_DEFINITION.md`](../research/PRODUCT_DEFINITION.md), as-built
boundaries in [`research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md), and test
procedures in [`docs/TESTING.md`](../docs/TESTING.md).

The framework also contains the initial dependency-driven waterfall through March 2028. Dates are
forecasts and never override the entry and exit evidence in this ledger.

Use [`docs/MILESTONE_TEMPLATE.md`](../docs/MILESTONE_TEMPLATE.md) and
`$domes-milestone-manager` for every status or transition audit. Evidence and intent govern; layout
does not.

**Status reviewed:** 2026-08-03
**Reviewed revision:** `7be7387` plus this proposed ledger change

## Current Status

| ID | Phase outcome | Status | Health | Entry | Exit gates | Forecast exit | Next gate | Last reviewed |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M0 | Trustworthy software and two-board foundation | `Complete` | `On track` | `Pass` | 5/5 | 2026-08-03 actual | Preserve required CI and hardware evidence | 2026-08-03 |
| M1 | Accepted product definition and complete NFF proof | `In progress` | `At risk` | `Pass` | 1/8 | 2026-09-28 | Run customer discovery and define measurable product requirements | 2026-08-03 |
| M2 | Predictive deterministic Linux system model | `Proposed` | `Blocked` | M1 incomplete | 0/7 | 2026-12-07 | Complete M1 | 2026-08-03 |
| M3 | Representative app-driven six-node system alpha | `Proposed` | `Blocked` | M2 incomplete | 0/7 | 2027-03-01 | Complete M2 and provide four economical radio nodes | 2026-08-03 |
| M4 | Production-intent EVT electrical prototype | `Proposed` | `Blocked` | M3 incomplete | 0/7 | 2027-06-21 | Complete M3 and approve EVT input package | 2026-08-03 |
| M5 | Frozen form-factor product validated at DVT | `Proposed` | `Blocked` | M4 incomplete | 0/7 | 2027-11-08 | Complete EVT and freeze the design | 2026-08-03 |
| M6 | Repeatable pilot manufacturing system at PVT | `Proposed` | `Blocked` | M5 incomplete | 0/6 | 2028-01-31 | Complete DVT and prepare intended production line | 2026-08-03 |
| M7 | Reproducible, compliant, supportable open product release | `Proposed` | `Blocked` | M6 incomplete | 0/7 | 2028-03-27 | Complete PVT and select an immutable candidate | 2026-08-03 |

### Active Phase Report

**Active phase:** M1, Product Definition and NFF Proof; `In progress`, `At risk`; actual start
2026-08-03; forecast exit 2026-09-28. Evidence reviewed 2026-08-03 against the revision above.

**Entry:** `Pass`. M0 is complete, the automated two-board baseline is accepted, both identified NFF
boards are available, and M1 execution began 2026-08-03 with product-definition and physical
qualification work active.

**Exit:** 1/8 gates accepted. Product discovery, requirements, compliance/open-source/economics, and
physical LED, touch, IMU, haptic, and audio evidence remain open.

**Delivered:** Reproducible software CI, automated programming and recovery, UART and BLE control,
serial and BLE OTA, two-way ESP-NOW, diagnostics, trace, self-tests, and a 620-second two-board soak.

**Now:** Validate the customer, problem, competitive wedge, product boundary, and measurable
requirements while completing the NFF audio path and physical peripheral campaign.

**Next:** Produce the customer-evidence and product-requirements baseline, then run the observed
two-board LED, touch, IMU, haptic, and audio checklist.

**Risks and decisions:** The product and willingness-to-pay hypotheses are unvalidated; the launch
market, license, economics, populated haptic part, and audio implementation remain unresolved.

**Following phase:** M2 is `Proposed` and blocked by M1. It becomes `Ready` only after M1 is complete
and the accepted NFF measurements define its validation envelope.

## Evidence Register

### Software And Automated Hardware Baseline

| Evidence | Source | Result | Boundary |
| --- | --- | --- | --- |
| Main software CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30782073994](https://github.com/pcesar22/domes/actions/runs/30782073994) | Passed | Builds, tests, generated artifacts, lint, documentation, Flutter Linux, and ESP-IDF release checks |
| Repository effectiveness acceptance | [PR 85](https://github.com/pcesar22/domes/pull/85), merged 2026-08-03 | Accepted | Instructions, verification orchestration, pinned toolchains, and CI behavior |
| Automated hardware CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30785241480](https://github.com/pcesar22/domes/actions/runs/30785241480) | Passed | Two NFF boards, serial/BLE/ESP-NOW/OTA/diagnostics/trace; no physical observation |
| Hardware runner | `domes-hardware-ministrom`, Linux x64, `domes-hardware` label | Online when reviewed | Availability is checked before dispatch |

The accepted software baseline passed 271 host firmware tests, 100 Rust CLI tests, 161 Flutter
tests, generated-binding checks, a clean ESP-IDF v5.4.4 build, and the aggregate `CI Gate`. Use live
test discovery and CI output rather than copying these counts elsewhere.

### Retained Two-Board Campaign

The 2026-08-02 campaign used two NFF ESP32-S3 N8R8 boards, their CP2102N UART bridges, and an Intel
AX210 BLE adapter. The following artifacts were built from
`99db4b77cc58a6695b86b7122ea5ee77fa9cbecb`:

| Purpose | Embedded version | `domes.bin` SHA-256 | `domes.elf` SHA-256 | Additional identity |
| --- | --- | --- | --- | --- |
| Baseline and merged-factory programming | `v0.0.0-0-g99db4b77cc58` | `9b27881a78d0d800277dc1cf6900b4e96f6e8ac3d221614355281ae43e176122` | `e01eb0fc66fe9efe34fd24eb47967bee5c792fa9c17e0f6059c911ad00fc0831` | Factory SHA-256 `b5a5509bc61fd08118d0a0b1a9e2933e9f65c00077c985c9f7bde10441417d94` |
| Accepted serial/BLE OTA and final runtime | `v0.0.0-1-g99db4b77cc58` | `cafb9c480f04b8d67f599977f84fd437fb2d0d0786c52a50e1205f4dc26f510b` | `e2fe59021b24c09b4cfc53ca961b5f1389762fd1160a55c96670ff86f0fb1ce7` | Final campaign image on both boards |
| Forced failed-self-test rollback | `v0.0.0-2-g99db4b77cc58` | `d065b384e95e06251d48e6e3ee2590af81c6895afd449d39ddac56c78ecc03b8` | `5831c1206a22c4ab9f8264c0c3ac85985e00ea53cadca1a0a42435ad4c03be78` | Rollback-test build only |

| Board | Stable CP2102N identity | Runtime identity during campaign |
| --- | --- | --- |
| Pod 1 | `5edf3f45576def11a245cea7c169b110` | Pod ID 1; WiFi `94:a9:90:0a:eb:c0`; BLE `94:A9:90:0A:EB:C2` |
| Pod 2 | `002a9f8e536def119f38c1a7c169b110` | Pod ID 2; WiFi `94:a9:90:0a:ea:50`; BLE `94:A9:90:0A:EA:52` |

The campaign verified erase and merged-factory programming, UART and BLE diagnostics, feature and
mode control, registry fan-out, serial and BLE OTA plus failure recovery, forced rollback, repeated
two-way ESP-NOW benchmarks, a traced drill, restart-snapshot symbolization, and a 620-second soak. It
did not physically confirm light, touch, motion, vibration, or sound.

Across three fresh ESP-NOW lifecycles per direction, both boards received 300/300 benchmark packets.
Command/acknowledgment round-trip observations were 2.644-18.910 ms from Pod 1 to Pod 2 and
2.688-20.780 ms from Pod 2 to Pod 1, with per-session means of 4.975-5.272 ms and 5.062-5.226 ms
respectively. These are round trips, not one-way latency, and do not prove sub-millisecond behavior.

## Phase Contracts

<!-- markdownlint-disable MD024 -->

### M0: Trustworthy Software And Two-Board Foundation

**Outcome:** Every change can be evaluated through reproducible software CI, and a trusted Linux
runner can program, communicate with, update, recover, and test two identified NFF boards.

**Status:** `Complete`
**Health:** `On track`
**Owner:** Repository, firmware, and CI engineering
**Depends on:** None
**Blocks:** M1

#### Entry Gate

Project source, development hardware, and accountable engineering ownership existed. `Pass`.

#### Exit Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Repository authority | Sources of truth, architecture boundaries, and scoped instructions are consistent | `AGENTS.md`, `docs/README.md`, architecture docs | `Pass` |
| Reproducible software CI | Pinned firmware, CLI, Flutter, protocol, lint, docs, and package checks pass behind one required gate | Run 30782073994 | `Pass` |
| Build and program | Both stable board identities build, flash, boot, self-test, and receive unique pod IDs | Run 30785241480 | `Pass` |
| Transport, update, and recovery | UART, BLE, serial/BLE OTA, interruption recovery, second boot, and forced rollback pass | Run 30785241480 | `Pass` |
| Peer, diagnostics, and closure | Two-way ESP-NOW, health, trace, registry fan-out, and deterministic cleanup pass | Run 30785241480 | `Pass` |

**Invalidation:** Reopen affected gates after material changes to required CI, toolchains, generated
contracts, runner/board identity, firmware, CLI, transport, update, peer, or diagnostic behavior.

### M1: Product Definition And NFF Proof

**Outcome:** An evidence-backed product contract defines what DOMES should become, and both NFF
boards provide a complete, healthy physical reference for the next engineering phase.

**Status:** `In progress`
**Health:** `At risk`
**Owner:** Product and systems engineering
**Depends on:** M0
**Blocks:** M2

#### Entry Gate

M0 is complete; both NFF boards and the automated workflow are available; product-definition and
physical-qualification work started 2026-08-03. `Pass`.

#### Scope

Included: target user and job, competitive wedge, willingness to pay, launch scope, measurable
requirements, verification matrix, launch-market compliance plan, preliminary economics,
open-source plan, and physical LED/touch/IMU/haptic/audio qualification on both NFF boards.

Excluded: predictive simulation acceptance, six-node system claims, custom PCB design, certification,
and form-factor product validation.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Customer and purchase evidence | Target user, job, switching reason, kit, and price are supported by at least 10 relevant observations/interviews and 3 paid or deposit-backed pilot commitments | Research record and semantic audit | `Not run` |
| Product requirements | Stable, measurable requirements and a complete verification matrix cover the accepted launch scope | Inspection and semantic traceability audit | `Not run` |
| Product constraints | Launch market, compliance route, preliminary COGS/margin/support model, licenses, and open-source release boundary are accepted | Decision records and specialist input where required | `Not run` |
| Automated NFF baseline | Both boards flash, self-test, report healthy, and pass transport/update/peer/diagnostic automation | Run 30785241480 | `Pass` |
| LED observation | All 16 LEDs show required colors, patterns, brightness, and off behavior on both boards | Observed bring-up record | `Unverified` |
| Touch and IMU observation | Every pad, orientation change, and physical tap produces the required response without persistent false activation | Observed bring-up record plus logs | `Unverified` |
| Haptic and audio observation | Populated haptic parts match their profiles; required effects, samples, and volume range are physically correct on both boards | Inspection, CLI/app action, observation | `Not run` |
| Phase closure | Complete record is retained; both boards finish healthy; product definition and physical evidence are mutually consistent | Milestone manager audit | `Not run` |

**Current work:** Validate the product hypothesis, define requirements, complete audio sample/volume
control, confirm the populated haptic part, and execute the observed bring-up checklist.

**Invalidation:** Reopen affected gates after changes to the target customer, product boundary,
launch market, requirements, peripherals, pins, board assembly, power, drivers, haptic profile, audio
path, or accepted evidence environment.

### M2: Predictive Deterministic Linux System Model

**Outcome:** A deterministic Linux simulation reuses production logic and predicts bounded two-pod
protocol, state, failure, and timing behavior closely enough to guide hardware decisions.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Firmware architecture and test engineering
**Depends on:** M1
**Blocks:** M3

#### Entry Gate

M1 is complete; accepted requirements and NFF measurements define the model and held-out validation
envelope; the contract audit returns `Meets intent`. Currently blocked by M1.

#### Constraints

Virtual time alone advances the simulation. Production state/protocol logic is shared. Time,
scheduling, randomness, delivery, input, and output use explicit interfaces. Equal-time ordering and
seeds are stable. Calibration and held-out validation datasets are separate. Claims remain bounded
to the measured board, firmware, RF/coexistence, and scenario envelope.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Exact replay | Repeated revision/scenario/model/config/seed runs produce byte-identical normalized traces | Linux CI artifacts | `Not run` |
| Production fidelity | In-scope state and protocol behavior execute shared production logic through virtual dependencies | Source audit and focused tests | `Not run` |
| Functional agreement | Every held-out scenario preserves required invariants in simulation and hardware | Shared scenario comparison | `Not run` |
| Safety agreement | No held-out critical scenario passes simulation while hardware violates a required deadline or outcome | Boundary and fault campaign | `Not run` |
| Central timing accuracy | Hardware and simulation p50/p95 differ by no more than `max(1 ms, 15%)` | Held-out correlated report | `Not run` |
| Tail and delivery accuracy | p99 differs by no more than `max(2 ms, 25%)`; delivery differs by at most one percentage point or accepted intervals overlap | Held-out report | `Not run` |
| Model closure | PR suite passes and a versioned record states inputs, envelope, error, limitations, and invalidation | CI and milestone audit | `Not run` |

**Invalidation:** Reopen after material changes to timing, protocol, scheduling, radio parameters,
boards, model, scenario semantics, validation envelope, or correlation method.

### M3: Representative Six-Node System Alpha

**Outcome:** A representative six-node system executes general app-defined offline drills with
bounded timing, physical interaction, diagnostics, result collection, and predictable recovery.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Product software and distributed-systems engineering
**Depends on:** M2
**Blocks:** M4

#### Entry Gate

M2 is complete; drill, authority, timing, result, mobile/network, and failure requirements are
accepted; six suitable nodes exist. The four added nodes may be economical ESP32 radio reference
nodes rather than additional NFF carriers. Currently blocked by M2 and inventory.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Six-node inventory | Six uniquely identified nodes program, self-test, register, and recover reliably | Hardware campaign | `Not run` |
| General drill | Valid app-defined drills execute and invalid definitions fail consistently | App/firmware end-to-end campaign | `Not run` |
| Control paths | Supported BLE/mobile and credentialed WiFi workflows are capability-correct and recoverable | Device campaigns | `Not run` |
| Timing and coexistence | Accepted start, event, completion, delivery, and coexistence bounds pass with truthful clock correlation | Six-node trace report | `Not run` |
| Failure and diagnostics | Join, peer loss, restart, delay, duplication, reordering, partial command failure, panic, trace, and passive capture produce safe and actionable results | Fault campaign | `Not run` |
| Physical workflow | Physical input drives complete rounds and results return without stale or duplicated state | Observed campaign | `Not run` |
| Stability and prediction | Required soak passes and M2 remains within accepted six-node functional bounds | Soak and comparison report | `Not run` |

**Invalidation:** Reopen after changes to drill semantics, authority, peer protocol, timing, identity,
radio configuration, scale, app transport, diagnostics, or model envelope.

### M4: EVT Production-Intent Electrical Prototype

**Outcome:** Production-intent electrical prototypes retire the major power, battery, RF, sensor,
feedback, firmware, update, testability, and manufacturing risks before design freeze.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Product hardware and firmware platform engineering
**Depends on:** M3
**Blocks:** M5

#### Entry Gate

M3 is complete; product requirements, preliminary FMEA and compliance plan, ID package, schematic,
BOM, supply risks, test points, and contract-manufacturer feedback are accepted. Currently blocked
by M3 and missing production-intent design files.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Build identity | Each EVT unit, PCB revision, BOM, firmware, fixture, and deviation is traceable | Build record and inspection | `Not run` |
| Power and battery | Charging, protection, runtime, faults, USB-C, thermal, and battery safety meet EVT requirements | Bench and fault report | `Not run` |
| RF and coexistence | BLE, WiFi, and ESP-NOW range, latency, interference, and antenna behavior meet EVT limits | RF/system report | `Not run` |
| Physical functions | Touch, IMU, RGBW optics, audio, haptic, controls, and provisional enclosure meet EVT requirements | Test and demonstration | `Not run` |
| Firmware lifecycle | Factory program, identity, boot, OTA, second boot, rollback, diagnostics, and recovery pass | EVT campaign | `Not run` |
| DFM and testability | CM review, assembly learning, test coverage, supply risk, rework, and failure analysis are dispositioned | Review and build report | `Not run` |
| EVT closure | Typically 10-20 units provide enough evidence to freeze or deliberately revise architecture; every high risk has an owner and disposition | AI evidence audit | `Not run` |

**Invalidation:** Reopen after material schematic, BOM, PCB, RF, power, battery, peripheral,
partition, bootloader, fixture, factory-flow, or requirement changes.

### M5: DVT Frozen Form-Factor Product

**Outcome:** Near-final form-factor units prove the frozen design meets accepted product, user,
environmental, reliability, security, compliance, and six-pod requirements.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Systems validation and product engineering
**Depends on:** M4
**Blocks:** M6

#### Entry Gate

M4 is complete; electrical, mechanical, firmware, manufacturing, and test interfaces are frozen;
accepted deviations are controlled; DVT units and procedures represent the intended product.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Product requirements | Every applicable requirement has direct passing evidence on frozen units | Verification matrix | `Not run` |
| Form factor and durability | Drop, impact, wear, ingress claim, charging/stacking, thermal, and use-surface behavior meet requirements | Laboratory and observed report | `Not run` |
| Full six-pod system | Near-final six-pod kits pass drill, latency, recovery, soak, update, diagnostics, and mobile workflows | System validation report | `Not run` |
| Customer validation | Intended users complete representative workflows and purchase assumptions remain supported | Structured field trial | `Not run` |
| Security and service | Threat controls, authenticated update, rollback, reset, diagnostics, repair, and support flows pass | Security/service review | `Not run` |
| Compliance | Pre-compliance and required formal testing for the launch market pass or have accepted corrective evidence | Laboratory records | `Not run` |
| DVT closure | Typically 30-100 controlled units show no unresolved design issue requiring architecture change | AI evidence audit | `Not run` |

**Invalidation:** Reopen after a design, material, supplier, firmware, requirement, compliance,
security, or manufacturing change that invalidates DVT evidence.

### M6: PVT Repeatable Manufacturing System

**Outcome:** The intended production line repeatedly builds traceable, conforming products at the
ratified yield using controlled materials, fixtures, tests, work instructions, and disposition.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Manufacturing and operations engineering
**Depends on:** M5
**Blocks:** M7

#### Entry Gate

M5 is complete; intended CM, line, approved materials, golden units, fixtures, software, work
instructions, sampling plan, yield target, packaging, and failure process are ready.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Controlled pilot | Typically 100-300 units are built on the intended line with complete lot, component, firmware, operator, fixture, and result traceability | Pilot build record | `Not run` |
| Yield and process | First-pass and final yield meet pre-ratified targets; defects and rework have closed dispositions | Yield and defect report | `Not run` |
| Factory test | Programming, identity, calibration, functional test, data retention, golden-unit checks, and failure routing are repeatable | Capability study | `Not run` |
| Product sampling | Ratified samples pass critical DVT, update, transport, battery, RF, and six-pod regression gates | PVT validation report | `Not run` |
| Fulfillment readiness | Packaging, labels, regulatory marks, serialization, shipping, battery transport, spares, and return flow pass | Inspection and logistics trial | `Not run` |
| PVT closure | No unresolved manufacturing-system issue prevents a controlled release build | AI evidence audit | `Not run` |

**Invalidation:** Reopen after changes to factory, process, fixture, supplier, material, firmware,
test limits, packaging, traceability, or yield assumptions.

### M7: Open Product Release And Sustainment

**Outcome:** One immutable DOMES candidate is reproducibly buildable, compliant, installable,
operable, serviceable, supportable, and openly reproducible as the exact product released.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Release and product operations
**Depends on:** M6
**Blocks:** Product release

#### Entry Gate

M6 is complete; one immutable source, hardware, BOM, firmware, app, CLI, manufacturing, and package
candidate is selected; every required release gate has an owner and evidence plan.

#### Exit Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Immutable candidate | Pinned clean builds reproduce versioned binaries, checksums, SBOM, manufacturing package, and release metadata | Release build record | `Not run` |
| Candidate CI | Required software and trusted production-hardware workflows pass on the exact candidate | CI and hardware runs | `Not run` |
| Product operation | Installation, six-pod use, mobile/network control, update, interruption recovery, second boot, rollback, diagnostics, and soak pass | Release campaign | `Not run` |
| Compliance and security | Required launch-market approvals, Bluetooth obligations, security review, vulnerability intake, and update policy are accepted | Certificates and review records | `Not run` |
| Open-source package | Licenses and third-party obligations are satisfied; editable hardware sources, firmware, CLI, app, BOM, manufacturing files, build/test instructions, and known limitations are published | Release inspection against OSHWA practices | `Not run` |
| Service and launch | Support boundary, spares, repair, warranty, returns, production capacity, documentation, and launch-customer readiness are accepted | Operations review | `Not run` |
| Release acceptance | The milestone manager finds every artifact and gate direct, current, consistent, and bound to the immutable candidate | AI semantic audit | `Not run` |

**Invalidation:** Reopen after changes to the candidate source, dependency, toolchain, hardware, BOM,
factory, artifact, platform, requirement, approval, license, security posture, or support boundary.

<!-- markdownlint-enable MD024 -->

## Active Risks And Decisions

| Phase | Condition | Consequence | Owner | Resolution or decision point |
| --- | --- | --- | --- | --- |
| M1 | Customer, switching, and price hypotheses are unvalidated | A technically successful product may not be wanted or economically viable | Product | Complete discovery and obtain pilot purchase evidence before M1 exit |
| M1 | Launch market, compliance route, economics, and licenses are unset | Product requirements and architecture may omit mandatory constraints | Product and systems | Accept baselines before M1 exit |
| M1 | Audio path and physical peripheral evidence are incomplete | The NFF reference cannot calibrate M2 or de-risk EVT | Firmware/hardware integration | Complete audio, confirm haptic part, run observed checklist |
| M2 | Production timing calls and pod clocks are not fully controlled/correlated | Simulation may be repeatable but not predictive | Firmware architecture | Prove exact replay, then calibrate and validate on held-out data |
| M3 | Only two NFF boards exist | Six-node system behavior cannot be physically evaluated | Systems engineering | Add four economical representative ESP32 nodes after M2 |
| M4 | No checked-in production schematic, PCB, manufacturing BOM, or approved profile exists | EVT cannot start | Product hardware | Develop and review the production-intent input package after M3 |
| M5-M7 | Launch compliance, certification, manufacturing, support, and release evidence do not exist | Product cannot be sold or responsibly sustained | Product operations | Build evidence through EVT, DVT, PVT, and release gates |

## Authority Boundaries

- The schematic/netlist owns physical connectivity; `firmware/domes/main/config.hpp` owns the active
  compiled NFF mapping; [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) reconciles them.
- The current NFF build is RGB on an 8 MB profile with two `0x1E0000` OTA app slots. RGBW and 16 MB
  remain product targets until EVT proves a separate production-intent profile.
- Raw WiFi/TCP image transfer is unsupported. Serial and BLE are the supported CLI OTA paths.
- Current multi-pod trace merge supports local `zero` and `raw` timelines only. Neither correlates
  pod clocks.
- Clean-restart snapshots and panic coredumps are separate diagnostic artifacts.
- Current hardware files do not contain production Gerbers, placement data, or a board-specific
  manufacturing BOM. No production-PCB or form-factor claim is accepted.
