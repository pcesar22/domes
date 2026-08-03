# DOMES Delivery Milestones

This document is the delivery-status authority for DOMES. It answers what is accepted, what is being
worked on, what comes next, and what evidence supports each claim. Product targets belong in
[`research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md), as-built boundaries in
[`research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md), and verification
procedures in [`docs/TESTING.md`](../docs/TESTING.md).

Use [`docs/MILESTONE_TEMPLATE.md`](../docs/MILESTONE_TEMPLATE.md) and
`$domes-milestone-manager` when creating or reviewing these contracts. Substance and evidence matter;
the layout is only a readable presentation.

**Status reviewed:** 2026-08-03

## Reading Status

Lifecycle and health are separate:

| Field | Values | Interpretation |
| --- | --- | --- |
| Status | `Proposed`, `Ready`, `In progress`, `Acceptance pending`, `Complete`, `Superseded` | Position in the acceptance lifecycle |
| Health | `On track`, `At risk`, `Blocked` | Confidence that the next gate can be reached |
| Gate | `Not run`, `Pass`, `Fail`, `Unverified`, `N/A` | State of direct acceptance evidence |

`Complete` means the milestone manager audited direct evidence for every applicable gate and found
the outcome accepted, not merely implemented. Human measurements and physical observations are
evidence inputs, not approval. A host test is not firmware integration, a successful command is not
physical observation, and a grouped trace is not synchronized cross-pod timing. Hardware evidence
is valid only for the named source, artifacts, boards, transports, and date.

## Evidence Invalidation

The milestone manager reopens a completed milestone when a relevant change makes its evidence stale.
Every status review checks these triggers; individual contracts do not repeat them.

| Milestone | Evidence becomes stale after changes to |
| --- | --- |
| M0 | Required CI, supported toolchains, generated contracts, or authoritative documentation |
| M1 | Runner or board identity, hardware-workflow coverage, firmware, CLI, or transports |
| M2 | Peripherals, pins, board assembly, power, drivers, haptic profile, or audio path |
| M3 | Timing, protocol, scheduling, radio parameters, boards, model, scenarios, or validation envelope |
| M4 | Trace schema, buffers, crash storage, snapshots, symbolization, clocks, transports, or soak thresholds |
| M5 | BLE protocol, fragmentation, permissions, WiFi capability, credentials, OTA, mobile platform, or transports |
| M6 | Drill semantics, authority, peer protocol, timing targets, pod identity, radio configuration, scale, or pod design |
| M7 | Schematic, BOM, PCB, pins, partitions, bootloader, peripherals, RF, power, factory flow, or production profile |
| M8 | Candidate source, dependencies, toolchains, generated contracts, hardware, artifacts, platforms, thresholds, or required documentation |

## Delivery Dashboard

| ID | Outcome | Status | Health | Accepted gates | Next gate | Last reviewed |
| --- | --- | --- | --- | --- | --- | --- |
| M0 | Reproducible repository and software-CI baseline | `Complete` | `On track` | 5/5 | Preserve the baseline on each PR | 2026-08-03 |
| M1 | Automated two-board programming and readiness | `Complete` | `On track` | 6/6 | Rerun after behavior-affecting changes | 2026-08-03 |
| M2 | Physical NFF peripheral qualification | `In progress` | `At risk` | 1/7 | Complete audio workflow, then run the observed checklist | 2026-08-03 |
| M3 | Deterministic predictive Linux system simulation | `Ready` | `On track` | 0/7 | Introduce virtual-time and scheduler interfaces | 2026-08-03 |
| M4 | Actionable diagnostics and field serviceability | `Proposed` | `On track` | 3/7 | Deliberately capture and decode a panic coredump | 2026-08-03 |
| M5 | Qualified network and mobile workflows | `Proposed` | `At risk` | 1/6 | Exercise a WiFi-enabled image with stored credentials | 2026-08-03 |
| M6 | Six-pod product runtime | `Proposed` | `Blocked` | 1/7 | Provide four additional representative pods | 2026-08-03 |
| M7 | Verified production hardware profile | `Proposed` | `Blocked` | 0/6 | Approve the production board and storage profile | 2026-08-03 |
| M8 | Releasable product candidate | `Proposed` | `Blocked` | 0/7 | Complete M2 through M7 | 2026-08-03 |

**Current milestone:** M2, Physical NFF Peripheral Qualification.

**Current focus:** Finish the missing audio sample and volume-control path, then obtain human
observation of LEDs, touch, IMU taps, haptics, and audio on both NFF boards.

**Following milestone:** M3 is ready once M2 establishes a trusted physical reference. It turns the
existing host simulator into a deterministic, hardware-calibrated predictor for the bounded two-pod
system behavior needed by later milestones.

## Evidence Register

### Current Software Baseline

| Evidence | Source | Result | Boundary |
| --- | --- | --- | --- |
| Main software CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30782073994](https://github.com/pcesar22/domes/actions/runs/30782073994) | Passed | Builds, tests, generated artifacts, lint, documentation, Flutter Linux, and ESP-IDF release checks |
| Repository effectiveness acceptance | [PR 85](https://github.com/pcesar22/domes/pull/85), merged by project owner on 2026-08-03 | Accepted | Repository instructions, verification orchestration, pinned toolchains, and CI behavior |
| Automated hardware CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30785241480](https://github.com/pcesar22/domes/actions/runs/30785241480) | Passed | Two NFF boards, serial/BLE/ESP-NOW/OTA/diagnostics/trace paths; no physical observation |
| Hardware runner | `domes-hardware-ministrom`, Linux x64, `domes-hardware` label | Online when reviewed | Runner availability can change and must be checked before dispatch |

The current software baseline passed 271 host firmware tests, 100 Rust CLI tests, 161 Flutter tests,
generated-binding checks, a clean ESP-IDF v5.4.4 build, and the aggregate `CI Gate`. Use live test
discovery and CI output instead of copying these counts into other documents.

### Retained Manual Hardware Campaign

The 2026-08-02 campaign used two NFF ESP32-S3 N8R8 boards, their CP2102N UART bridges, and an Intel
AX210 BLE adapter. The following exact artifacts were built from
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
The command/acknowledgment round-trip observations were:

| Direction | Minimum | Per-session mean range | Maximum |
| --- | --- | --- | --- |
| Pod 1 to Pod 2 | 2.644 ms | 4.975-5.272 ms | 18.910 ms |
| Pod 2 to Pod 1 | 2.688 ms | 5.062-5.226 ms | 20.780 ms |

These are round-trip observations, not one-way latency, and they do not demonstrate the target
architecture's sub-millisecond goal.

## Milestone Contracts

<!-- markdownlint-disable MD024 -->

### M0: Reproducible Repository And Software-CI Baseline

**Outcome:** Every pull request can be evaluated from documented sources of truth with pinned
toolchains, reproducible builds, and one trustworthy aggregate software-CI result.

**Status:** `Complete`
**Health:** `On track`
**Owner:** Repository engineering
**Last reviewed:** 2026-08-03
**Depends on:** None
**Blocks:** M1

#### Scope

Included: repository authority, documentation lifecycle, firmware/CLI/Flutter builds, host tests,
generated protocols, lint, release-package checks, and the required aggregate CI gate.

Excluded: proof of physical device behavior and product-scale qualification.

#### Constraints

- Current code and generated artifacts outrank proposals when describing implemented behavior.
- ESP-IDF v5.4.4, Rust 1.92.0, and Flutter 3.44.8 remain pinned until deliberately changed.
- Protocol definitions remain owned by `firmware/common/proto/` except documented bounded legacy
  binary contracts.
- A pull request is not review-ready while its required `CI Gate` is failing or missing.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Repository authority and scoped agent guidance | `AGENTS.md`, nested instructions, `docs/README.md` | `Accepted` |
| Change-aware verification entry point | `scripts/verify.sh`, `docs/TESTING.md` | `Accepted` |
| Software-CI workflow with aggregate gate | `.github/workflows/firmware-ci.yml` | `Accepted` |
| Reproducible firmware, CLI, Flutter, and protocol builds | PR 85 verification record and run 30782073994 | `Accepted` |
| Current architecture and stale-document separation | `research/SOFTWARE_ARCHITECTURE.md`, `research/architecture/README.md` | `Accepted` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Full repository verification | Every required `scripts/verify.sh` section passes | PR 85 verification record | `Pass` |
| Pull-request CI | Aggregate `CI Gate` passes on the accepted head | Run 30781526810 | `Pass` |
| Main CI | Aggregate `CI Gate` passes after merge | Run 30782073994 | `Pass` |
| Documentation and generated artifacts | Links and generated bindings show no drift | PR 85 verification record | `Pass` |
| Evidence acceptance | Milestone manager finds the repository, CI, generated contracts, and documents consistent | PR 85 evidence and merged main CI | `Pass` |

#### Current State

**Delivered:** A clean, reproducible software baseline with documented authority and CI parity.
**Now:** Preserve it as later milestones change behavior.
**Next:** Rerun affected verification on every pull request and keep `CI Gate` required.
**Blockers:** None.
**Decisions needed:** None.

### M1: Automated Two-Board Programming And Readiness

**Outcome:** A trusted Linux runner can take two identified NFF boards from source checkout through
programming, communication, update, rollback, peer, diagnostics, trace, and deterministic cleanup
without manual command intervention.

**Status:** `Complete`
**Health:** `On track`
**Owner:** Firmware and CI engineering
**Last reviewed:** 2026-08-03
**Depends on:** M0
**Blocks:** M2, M3

#### Scope

Included: two stable CP2102N board identities, firmware and CLI build, flashing, deterministic pod
identity, UART, native Linux BLE, serial/BLE OTA and recovery, forced rollback, registry fan-out,
ESP-NOW exchange, health, self-test, trace artifacts, and cleanup.

Excluded: visual/tactile/acoustic observations, WiFi-enabled runtime, synchronized one-way latency,
six-pod behavior, and production hardware.

#### Constraints

- Hardware CI runs only on trusted code with the `hw-test` gate or explicit dispatch.
- Device identity uses `/dev/serial/by-id/`, never mutable kernel port numbers.
- ESP-NOW tests use fresh exact `disabled` lifecycles and separate simulation-off benchmarks from
  the simulated drill.
- Successful initialization and command responses remain distinct from physical confirmation.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Registered labeled runner | `domes-hardware-ministrom`, Linux x64, online at review | `Accepted` |
| Stable two-board discovery and identity provisioning | Hardware run 30785241480 | `Accepted` |
| End-to-end serial and BLE OTA workflows | Hardware run 30785241480 | `Accepted` |
| Forced failed-self-test rollback workflow | Hardware run 30785241480 | `Accepted` |
| Two-board registry, ESP-NOW, diagnostics, and trace workflow | Hardware run 30785241480 | `Accepted` |
| Deterministic idle cleanup and retained artifacts | Hardware run 30785241480 | `Accepted` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Runner readiness | Required runner version, labels, toolchains, two serial devices, and BLE host are available | Run 30785241480, steps 1-8 | `Pass` |
| Build and programming | Firmware and CLI build; both boards flash and receive unique identities | Run 30785241480, steps 5-10 | `Pass` |
| Communication and diagnostics | UART, BLE, registry, self-test, health, and capability checks pass on both boards | Run 30785241480, steps 11-12 and 22-26 | `Pass` |
| Update and recovery | Invalid/interrupted recovery, serial OTA, BLE OTA, second-boot checks, and forced rollback pass | Run 30785241480, steps 13-20 | `Pass` |
| Peer and trace behavior | Two-way ESP-NOW workflow passes and non-empty traces are retained | Run 30785241480, steps 21-28 | `Pass` |
| Cleanup and evidence closure | Both boards return to deterministic idle and the workflow concludes successfully | Run 30785241480, steps 29-30 | `Pass` |

#### Current State

**Delivered:** A successful 43-minute main-branch hardware run against both registered boards.
**Now:** Keep the runner online and rerun the workflow after relevant changes.
**Next:** Use the boards for the observed M2 peripheral campaign.
**Blockers:** None for automated two-board readiness.
**Decisions needed:** None.

### M2: Physical NFF Peripheral Qualification

**Outcome:** Both NFF development boards demonstrate correct observable LED, touch, motion, haptic,
and audio behavior under the complete bring-up checklist while remaining healthy afterward.

**Status:** `In progress`
**Health:** `At risk`
**Owner:** Firmware and hardware integration
**Last reviewed:** 2026-08-03
**Depends on:** M1
**Blocks:** M3, M4

#### Scope

Included: all 16 NFF LEDs and patterns, four physical touch pads, orientation and physical IMU tap,
confirmation of the populated haptic actuator and felt output, audio samples and volume control,
speaker output, repeated board health, and retained human observations for both boards.

Excluded: WiFi/mobile workflows, six-pod timing, production RGBW/16 MB behavior, and simulation
predictiveness.

#### Constraints

- CLI acceptance, driver initialization, and simulated input cannot satisfy a physical gate.
- Human observations identify the board, firmware revision, date, action, and observed result.
- Haptic settings stay within the bounded LD0832AA-0099F profile unless the populated part proves
  different and the design is reviewed.
- Failed or ambiguous physical behavior is recorded as a failure, not softened to partial success.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Automated electrical/control baseline on both boards | Run 30785241480; ten on-device self-tests per board | `Accepted` |
| LED visual acceptance record | Pending | `In progress` |
| Physical touch and IMU acceptance record | Pending | `Not started` |
| Populated haptic part and physical-output record | Pending | `Not started` |
| Complete audio sample, playback, and volume workflow | Driver exists; sample/volume path incomplete | `In progress` |
| Two-board bring-up record and final health evidence | Pending | `Not started` |

#### Acceptance Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Automated baseline | Both boards flash, self-test, report healthy, and accept peripheral commands | Run 30785241480 | `Pass` |
| LED observation | Every LED displays correct red, green, blue, white, breathing, cycle, brightness, and off behavior on both boards | Human observation using `domes-cli` and bring-up checklist | `Unverified` |
| Touch observation | Every physical pad is detected on both boards with no persistent false activation | Physical interaction plus CLI/log evidence | `Unverified` |
| IMU observation | Orientation changes and physical tap interrupt produce the expected response on both boards | Physical interaction plus CLI/log evidence | `Unverified` |
| Haptic observation | Populated part matches the configured profile and commanded effects are felt correctly on both boards | BOM/part inspection and human observation | `Unverified` |
| Audio observation | Known samples play through both speakers and volume control changes output across its supported range | CLI/app action and human observation | `Not run` |
| Campaign closure | Complete checklist is retained and both boards finish with health and all self-tests passing | Bring-up record plus final CLI output | `Not run` |

#### Current State

**Delivered:** Drivers initialize, control commands round-trip, simulated touch works, and both boards
pass automated health/self-tests.
**Now:** Complete sample storage and the host volume-control workflow.
**Next:** Run one observed checklist across both boards and retain the results.
**Blockers:** Physical measurements need an observer; audio acceptance also needs the incomplete
sample/volume workflow. The milestone manager will evaluate the retained observations and health
evidence.
**Decisions needed:** Confirm the populated haptic actuator identity if it differs from the schematic.

### M3: Deterministic Predictive Linux System Simulation

**Outcome:** A deterministic Linux simulation reuses production logic and predicts two-pod DOMES
protocol, game-state, failure-handling, and timing behavior within a measured operating envelope.

**Status:** `Ready`
**Health:** `On track`
**Owner:** Firmware architecture and test engineering
**Last reviewed:** 2026-08-03
**Depends on:** M2
**Blocks:** M4, M5, M6

#### Scope

Included: game, mode, feature, orchestration, and protocol state machines; two-pod ESP-NOW delivery;
deadlines, retries, loss, duplication, reordering, and peer loss; measured coexistence effects;
abstract touch/IMU/output completion; and functional two-to-six-pod design scenarios.

Predictive timing qualification is initially limited to two NFF ESP32-S3 boards, the tested firmware
profile, and documented RF/coexistence conditions. Six-pod functional simulation is included, but
six-pod timing qualification belongs to M6.

Excluded: arbitrary RF environments, cycle-accurate ESP32 execution, unmeasured FreeRTOS timing,
LED appearance, haptic feel, audio quality, touch sensitivity, battery, thermal, and mechanical
behavior.

#### Constraints

- Simulation uses virtual time only; wall-clock reads, sleeps, and host scheduling cannot advance it.
- Production state and protocol logic is reused rather than reimplemented.
- Time, scheduling, randomness, delivery, input, and output enter through explicit interfaces.
- Equal-time events have stable ordering and stochastic behavior uses recorded independent seeds.
- Identical revision, scenario, model, configuration, and seed produces an identical normalized trace.
- The same declarative scenario can drive simulation and hardware using existing protobuf commands.
- Hardware comparison uses validated clock correlation; capture-start alignment is not timing proof.
- Model parameters come from versioned hardware measurements, never aspirational targets.
- Calibration and held-out validation datasets remain separate.
- CI reports drift and never updates calibration or expected results automatically.
- Predictive claims remain bounded to the published validation envelope.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Injected virtual clock, scheduler, and deterministic RNG | Existing simulator advances mock time; production paths still read ESP/FreeRTOS clocks directly | `In progress` |
| Production-logic Linux scenario runner for two to six pods | Existing `SimOrchestrator` and five-pod trace generator provide a partial foundation | `In progress` |
| Deterministic fault and calibrated ESP-NOW/coexistence model | Pending | `Not started` |
| Shared scenario execution on Linux and physical boards | Pending | `Not started` |
| Truthful multi-board clock-correlation and comparison report | Pending | `Not started` |
| Pull-request deterministic suite and hardware drift report | Pending | `Not started` |
| Versioned simulation validation record | Pending | `Not started` |

#### Acceptance Gates

| Gate | Required result | Verification and evidence | State |
| --- | --- | --- | --- |
| Repeatability | Repeated supported Linux runs produce byte-identical normalized traces | Fixed revision/scenario/model/config/seed replay | `Not run` |
| Production fidelity | In-scope state and protocol behavior executes shared production logic with virtual dependencies | Source review plus focused tests | `Not run` |
| Functional agreement | Every held-out scenario preserves required protocol and state invariants on simulation and hardware | Shared scenario comparison | `Not run` |
| Safety agreement | No held-out critical scenario passes simulation while hardware violates a required deadline or outcome | Fault and boundary campaign | `Not run` |
| Central timing accuracy | Hardware and simulation p50/p95 latency differ by no more than `max(1 ms, 15%)` | Held-out statistical report | `Not run` |
| Tail and delivery accuracy | p99 differs by no more than `max(2 ms, 25%)`; delivery differs by no more than one percentage point or defined intervals overlap | Held-out statistical report | `Not run` |
| CI and model record | PR deterministic suite passes and a versioned report states envelope, datasets, error bounds, limitations, and invalidation rules | CI plus validation document review | `Not run` |

#### Current State

**Delivered:** The milestone manager's semantic audit returns `Meets intent`, so this contract is
`Ready`. Deterministic host drills, in-memory pod routing, hits/misses/timeouts, and trace export
exist, but the simulator explicitly does not establish radio or device timing and no acceptance gate
has passed.
**Now:** Await M2 completion, then introduce common virtual-time and scheduler interfaces at the
production boundary.
**Next:** Demonstrate exact replay before adding calibrated timing or radio variability.
**Blockers:** M2 physical reference is not accepted; production timing calls are not fully injected;
pod clocks are not truthfully correlated.
**Decisions needed:** Ratify the proposed statistical tolerances after the first calibration dataset.

### M4: Actionable Diagnostics And Field Serviceability

**Outcome:** A developer can detect, capture, correlate, decode, and act on runtime failures using
version-bound evidence without confusing clean restarts, local traces, or initialization status with
the underlying failure.

**Status:** `Proposed`
**Health:** `On track`
**Owner:** Firmware reliability engineering
**Last reviewed:** 2026-08-03
**Depends on:** M2, M3
**Blocks:** M5, M6, M8

#### Scope

Included: system health, bounded memory profiles, restart snapshots, exact-ELF symbolization, panic
coredump capture/decode, trace capture/export, truthful clock correlation, non-resetting passive
capture, failure reproduction, and release-retained diagnostic artifacts.

Excluded: production observability services, cloud telemetry, and unsupported claims of synchronized
timing before correlation exists.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| UART/BLE health, self-test, memory, and ESP-NOW diagnostics | Manual campaign and hardware run 30785241480 | `Accepted` |
| CRC- and exact-ELF-bound restart snapshots | Manual campaign on both boards | `Accepted` |
| Trace recording, dump, live stream, names, and Perfetto export | Host tests and two-board captures | `Accepted` |
| Deliberate panic coredump retrieval and exact-ELF decode | Storage/build configuration exists; runtime capture pending | `In progress` |
| Validated cross-pod clock correlation | Current `zero`/`raw` merge is not correlation | `Not started` |
| Non-resetting live capture/mirror topology | Passive reader cannot share command UART | `Not started` |
| Extended failure and memory soak report | 620-second two-board baseline exists | `In progress` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Health and self-test | Both boards report actionable health and all self-tests over UART and BLE | Campaign plus run 30785241480 | `Pass` |
| Restart diagnosis | Both restart snapshots validate, bind the exact ELF, symbolize, reject corruption, and clear explicitly | 2026-08-02 campaign | `Pass` |
| Trace transport | Non-empty named traces capture, dump, merge without false clock claims, and open in Perfetto | Host tests and hardware captures | `Pass` |
| Panic diagnosis | A deliberate panic is stored, retrieved, and decoded with its exact ELF | Hardware procedure | `Unverified` |
| Cross-pod correlation | A validated synchronization marker supports bounded timing comparison | Hardware trace comparison | `Not run` |
| Passive capture | A documented topology observes live protocol without resetting or stealing the command transport | Hardware capture procedure | `Not run` |
| Stability | Required-duration soak meets restart, memory, task-health, and trace-drop thresholds | Release-oriented soak report | `Not run` |

#### Current State

**Delivered:** Health, self-test, memory profiling, restart snapshots, and trace export are usable.
**Now:** Panic storage is enabled, but deliberate capture and decode remain unverified.
**Next:** Trigger a controlled panic, retain the exact ELF and flash dump, and prove decoding.
**Blockers:** No truthful cross-clock marker and no defined passive mirror topology.
**Decisions needed:** Select the synchronization mechanism and physical capture topology.

### M5: Qualified Network And Mobile Workflows

**Outcome:** Supported users can discover, control, diagnose, and update pods through the documented
WiFi and mobile BLE paths with capability-aware failure handling and recovery.

**Status:** `Proposed`
**Health:** `At risk`
**Owner:** Host and mobile integration
**Last reviewed:** 2026-08-03
**Depends on:** M3, M4
**Blocks:** M6, M8

#### Scope

Included: build-gated WiFi/TCP config on port 5000, dedicated WiFi trace endpoint, credentialed device
testing, native mobile BLE discovery/control/notifications/OTA/abort/recovery, and multi-device app
orchestration consistent with firmware capabilities.

Excluded: raw WiFi/TCP image transfer, which is unsupported; generic trace commands over config port
5000; and simulated or Linux-only Flutter tests as mobile hardware proof.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Capability-aware CLI rejection for unsupported default WiFi and raw TCP OTA | Host tests and two-board hardware checks | `Accepted` |
| Credentialed WiFi-enabled firmware profile and test procedure | Pending | `Not started` |
| On-device TCP config and dedicated trace validation | Pending | `Not started` |
| Flutter BLE discovery, control, touch notification, and diagnostics validation | Implementation and host tests exist; device workflow pending | `In progress` |
| Flutter BLE OTA, abort, recovery, and second-boot validation | Implementation and tests exist; mobile hardware workflow pending | `In progress` |
| Multi-pod mobile orchestration acceptance record | Direct-BLE orchestration exists; full device workflow pending | `In progress` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Capability truthfulness | Default builds and unsupported transports fail before misleading connection or transfer behavior | Host tests and run 30785241480 | `Pass` |
| WiFi config | Credentialed image performs system, feature, mode, and diagnostic commands over TCP port 5000 | On-device WiFi campaign | `Not run` |
| WiFi trace | Dedicated endpoint streams/dumps valid traces without implying config-port support | On-device WiFi campaign | `Not run` |
| Mobile BLE control | Supported phone discovers both boards and completes control, diagnostics, and physical-touch notification workflows | Native mobile campaign | `Unverified` |
| Mobile BLE update | Full OTA, abort/recovery, expected-version boot, health/self-test, and second boot pass | Native mobile campaign | `Unverified` |
| Multi-device UX | App selects devices, handles partial failure, and completes a representative drill without stale state | Native mobile multi-device campaign | `Unverified` |

#### Current State

**Delivered:** The Rust CLI accurately exposes current capability boundaries; Flutter analysis and
host tests pass.
**Now:** No WiFi-enabled on-device or supported-phone acceptance campaign is retained.
**Next:** Build with `CONFIG_DOMES_WIFI_AUTO_CONNECT`, provision credentials, and exercise TCP config
and the dedicated trace endpoint.
**Blockers:** Credentials and network environment are not part of the default clean-board workflow;
native mobile validation requires a supported phone.
**Decisions needed:** Select the supported mobile platform/device matrix and credential-provisioning
procedure.

### M6: Six-Pod Product Runtime

**Outcome:** Six physical pods execute phone-defined drills with deterministic authority, bounded
timing, physical input, result collection, and recovery from peer and transport failures.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Distributed runtime engineering
**Last reviewed:** 2026-08-03
**Depends on:** M3, M4, M5
**Blocks:** M7, M8

#### Scope

Included: six physical pods, app-selected ESP-NOW authority, a general validated drill definition,
physical touch/input, synchronized start within an approved bound, timing and interference evidence,
peer join/loss/rejoin, partial failure, result collection, soak, and extension of simulation
qualification to the six-pod envelope.

Excluded: production-PCB qualification and claims based only on the existing fixed two-pod drill.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Two-pod discovery, roles, benchmark, fixed drill, and simulation baseline | Manual campaign and run 30785241480 | `Accepted` |
| Six representative physical pods and stable identity registry | Only two boards currently available | `Not started` |
| Phone-selected authority and general drill interpreter | Target design only | `Not started` |
| Six-pod synchronization, failure recovery, and result collection | Pending | `Not started` |
| Physical-input and coexistence campaign | Pending | `Not started` |
| Six-pod long-duration soak | Pending | `Not started` |
| Six-pod simulation calibration and validation extension | Pending | `Not started` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Two-pod prerequisite | Fresh discovery, complementary roles, two-way exchange, drill, health, and cleanup pass | Campaign and run 30785241480 | `Pass` |
| Six-pod inventory | Six representative pods flash, identify uniquely, self-test, and register reliably | Hardware campaign | `Not run` |
| General drill | Phone-defined valid drills execute; invalid definitions are rejected consistently | App/firmware end-to-end campaign | `Not run` |
| Timing and interference | Start, event, and completion timing meet approved product bounds under defined coexistence conditions | Correlated six-pod trace report | `Not run` |
| Failure recovery | Peer loss, delayed join, restart, duplicate/out-of-order traffic, and partial command failure resolve safely | Fault campaign | `Not run` |
| Physical interaction and results | Physical inputs drive correct rounds and all results return without stale or duplicated state | Observed six-pod campaign | `Not run` |
| Scale stability and prediction | Required soak passes and the M3 model meets ratified accuracy bounds for the six-pod envelope | Soak plus held-out comparison report | `Not run` |

#### Current State

**Delivered:** Two-pod fixed behavior is well exercised; it is not product-scale evidence.
**Now:** Work cannot enter six-pod physical qualification with only two boards.
**Next:** Manufacture or provide four additional representative pods, then establish stable identity
and health across all six.
**Blockers:** Four additional pods, general drill semantics, and truthful synchronized timing are
missing.
**Decisions needed:** Ratify product timing bounds, peer-recovery policy, and the general drill schema.

### M7: Verified Production Hardware Profile

**Outcome:** A separately selectable production hardware profile builds, programs, updates, and
passes electrical, peripheral, storage, RF, power, and manufacturing acceptance on representative
production boards.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Product hardware and firmware platform engineering
**Last reviewed:** 2026-08-03
**Depends on:** M6
**Blocks:** M8

#### Scope

Included: reviewed production schematic and BOM, explicit board selection, production GPIO and
peripheral constants, 16 MB partition/profile decision, RGBW behavior, factory image, OTA/rollback,
power and thermal limits, RF performance, manufacturing programming, and production bring-up.

Excluded: treating the current NFF RGB/8 MB profile as production or silently changing its verified
configuration.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Approved production schematic, BOM, and board identifier | Target architecture exists; final profile not approved | `Not started` |
| Explicit firmware board/profile selection | Only the compiled NFF profile exists | `Not started` |
| Reviewed 16 MB partition and release-image contract | Candidate budget exists; implementation pending | `Not started` |
| Production RGBW and peripheral implementation | NFF currently drives RGB behavior | `Not started` |
| Manufacturing programming and identity workflow | Pending | `Not started` |
| Representative production-board qualification report | Pending | `Not started` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Design approval | Schematic, BOM, pinout, power, storage, RF, and test points are reviewed and frozen for the candidate | Design review record | `Not run` |
| Build separation | NFF and production profiles select explicitly and build without configuration leakage | Clean build matrix | `Not run` |
| Programming and update | Factory programming, normal boot, OTA, second boot, and forced rollback pass on representative boards | Production hardware campaign | `Not run` |
| Physical peripherals | RGBW, touch, IMU, haptic, audio, and power behavior meet approved production requirements | Observed production checklist | `Not run` |
| RF and scale | BLE, WiFi, and ESP-NOW meet approved range, coexistence, timing, and six-pod requirements | RF/system campaign | `Not run` |
| Manufacturing readiness | Programming, identity, self-test, traceability, and failure disposition are repeatable | Pilot manufacturing report | `Not run` |

#### Current State

**Delivered:** The NFF development profile and a candidate 16 MB target budget are documented.
**Now:** No production profile or representative production board exists.
**Next:** Approve the production schematic/BOM/profile and implement explicit board selection.
**Blockers:** Production design decisions and representative boards are unavailable.
**Decisions needed:** Final flash layout, RGBW device, pinout, power architecture, haptic actuator,
audio path, manufacturing identity, and RF acceptance limits.

### M8: Releasable Product Candidate

**Outcome:** One immutable release candidate can be built, installed, updated, rolled back,
operated, diagnosed, and supported on the approved production system with every required software,
hardware, mobile, scale, and documentation gate accepted.

**Status:** `Proposed`
**Health:** `Blocked`
**Owner:** Release engineering
**Last reviewed:** 2026-08-03
**Depends on:** M2, M3, M4, M5, M6, M7
**Blocks:** Product release

#### Scope

Included: immutable versioned artifacts, software CI, trusted production hardware qualification,
simulation drift status, network/mobile acceptance, six-pod soak and recovery, release OTA and forced
rollback, diagnostics, documentation, licensing, known-risk disposition, and evidence-based release
acceptance.

Excluded: development-only NFF evidence presented as production proof and any capability outside the
approved release matrix.

#### Deliverables

| Deliverable | Evidence | State |
| --- | --- | --- |
| Versioned release source, binaries, metadata, checksums, and SBOM/package | Pending | `Not started` |
| Green software and production hardware CI on the exact candidate | Pending | `Not started` |
| Accepted M2-M7 evidence package with no stale dependencies | Pending | `Not started` |
| Release OTA, recovery, rollback, and diagnostics record | Pending | `Not started` |
| Supported-platform, operations, and user documentation | Pending | `Not started` |
| License, security, risk, and known-limitation disposition | Repository license not selected | `Not started` |
| Milestone acceptance audit | Pending | `Not started` |

#### Acceptance Gates

| Gate | Required result | Evidence | State |
| --- | --- | --- | --- |
| Upstream completion | M2 through M7 are complete and their evidence is valid for the candidate | Milestone review | `Not run` |
| Immutable build | Clean pinned toolchains reproduce exact versioned binaries, checksums, metadata, and package | Release build record | `Not run` |
| Candidate CI | Required software and trusted hardware workflows pass on the exact candidate | CI and hardware run links | `Not run` |
| Update and recovery | Installation, serial/BLE supported updates, interruption recovery, second boot, and forced rollback pass | Release hardware campaign | `Not run` |
| Product operation | Supported mobile/network and six-pod workflows meet timing, recovery, physical, and soak requirements | Release acceptance campaign | `Not run` |
| Service and governance | Diagnostics, documentation, security/risk review, licensing, and support boundaries are accepted | Release review record | `Not run` |
| Evidence acceptance | Milestone manager audits the immutable candidate and accepts it only when every gate is direct, current, and consistent | AI milestone audit | `Not run` |

#### Current State

**Delivered:** No production release candidate is claimed.
**Now:** Upstream development and qualification milestones remain open.
**Next:** Complete M2 through M7 in sequence and assemble evidence against one immutable candidate.
**Blockers:** Every upstream milestone, production hardware, six-pod inventory, mobile/network
qualification, and a repository license.
**Decisions needed:** Release platform matrix, release thresholds, licensing, support policy, and
risk-disposition policy.

<!-- markdownlint-enable MD024 -->

## Active Decisions And Blockers

| Milestone | Item | Required decision or work | Owner |
| --- | --- | --- | --- |
| M2 | Audio path | Complete sample storage and host volume control before observed playback | Firmware integration |
| M2 | Physical peripherals | Observe LEDs, touch, IMU taps, haptics, and audio on both boards | Project owner with firmware/hardware integration |
| M2 | Haptic identity | Confirm the populated actuator matches the LD0832AA-0099F profile | Hardware integration |
| M3 | Prediction thresholds | Ratify statistical tolerances after the first calibration dataset | Project owner and test engineering |
| M4 | Panic diagnostics | Deliberately capture and decode an ELF flash coredump | Firmware reliability |
| M4 | Clock and capture | Select truthful clock correlation and a non-resetting capture topology | Firmware architecture |
| M5 | Network/mobile matrix | Define credentials, supported phone, OS, and BLE permission matrix | Product/mobile integration |
| M6 | Pod inventory | Provide four additional representative pods | Project owner |
| M6 | Product orchestration | Approve drill schema, authority selection, timing, and recovery policy | Product and firmware architecture |
| M7 | Production profile | Approve schematic, BOM, 16 MB storage, RGBW, pins, power, and RF targets | Product hardware |
| M8 | Release governance | Select repository license, support matrix, and release decision policy | Project owner |

## Authority Boundaries

- The schematic/netlist owns physical connectivity; `firmware/domes/main/config.hpp` owns the active
  compiled NFF mapping; [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) reconciles them.
- The current NFF build is RGB on an 8 MB profile with two `0x1E0000` OTA app slots. RGBW and 16 MB
  remain production targets until M7 proves a separate profile.
- Raw WiFi/TCP image transfer is unsupported. Serial and BLE are the supported CLI OTA paths.
- Current multi-pod trace merge supports local `zero` and `raw` timelines only. Neither correlates
  pod clocks.
- Clean-restart snapshots and panic coredumps are separate diagnostic artifacts and must remain
  described separately.
