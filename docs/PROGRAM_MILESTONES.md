# DOMES Program Milestones

This is the delivery map for the program defined in
[the product realization framework](PRODUCT_REALIZATION_FRAMEWORK.md).
[Program status](../PROGRAM_STATUS.md) records current evidence and decisions;
GitHub issues track the remaining work. A milestone definition is not evidence that
its result has been achieved.

## App delivery

| ID | Result | Prerequisites | Acceptance boundary |
| --- | --- | --- | --- |
| APP0 | App transport and provider foundation | None | Generated protocol consumers, connection management and provider tests; physical phone behavior remains separate. |
| FS-WP-004A | Virtual pod lab | APP0 | Explicit two- and six-pod identities exercise real app repositories/providers, commands and notifications; deterministic virtual time/seed; safe connection and disposal behavior. |
| FS-WP-004B | Complete simulated phone journey | FS-WP-004A | Reproducible discovery, selection, drill, result and update/recovery journeys, including inactive touches, timeout, loss, duplicates, disconnect and restart. Every modeled behavior is declared. |
| FS-WP-004C | App and production simulator parity | FS-WP-004B, FS-WP-003A, FS-WP-002F | Command, notification, timing and result traces agree with the shared production contract; causal identity and modeled exceptions remain explicit. |
| FS-WP-004D | Six-node physical app alpha | FS-WP-004B, FS-WP-003A, NFF4, LAB6 | Supported phones complete six-node discovery, control, physical touch, failure/recovery and soak; node peripheral capabilities and timing uncertainty are recorded. |

The app virtual model supports A and B without predictive simulation, calibrated
hardware or six physical nodes. C establishes production parity. D establishes
physical app behavior; C is not an automatic prerequisite for D.

## NFF reference closure

| ID | Result | Prerequisites | Acceptance boundary |
| --- | --- | --- | --- |
| NFF0 | Automated bench foundation | None | Reproducible device communication and evidence procedures; historical campaigns do not establish current readiness. |
| NFF-MEM-001 | Runtime-memory repair candidate | None | Allocation rationale, regression tests, fresh pinned firmware build and independent source review. Installation and measured recovery belong to NFF1. |
| NFF1 | Current identity and recovery baseline | NFF0, LAB0, NFF-MEM-001 | Actual population and safe power checks; exact firmware identity, current diagnostics and all discovered self-tests; two normal boots; serial/BLE update recovery assessed separately. Forced rollback is a separately bounded test. |
| NFF2 | Observe every peripheral | NFF1 | Observed LED colors/brightness, touch and inactive touches, IMU motion, haptic effect, and audio/volume, each linked to its command and test configuration. |
| NFF3 | Measure the operating envelope | NFF1, LAB1 | Exact parts, calibrated instruments, current/voltage/transients/thermal/RF results and timing uncertainty; supply and firmware-resource limits are quantified. |
| NFF4 | Phone and two-pod fault campaign | NFF2 | Fresh disabled radio lifecycles, complementary roles, one peer each, benchmarks both ways with simulation off, a separate traced drill, physical interaction and failure/recovery assertions, update checks and bounded soak. |
| HR0 | Measured NFF reference | NFF2, NFF3 | Requirement-linked characterization dataset, limitations, failures and reproducible methods, with no unowned critical evidence gap. |

HR0 feeds hardware definition directly. It does not wait for the phone alpha or
the complete simulation ladder. Accepted commands, self-tests and software builds
do not substitute for observed effects or instrumented measurements.

## Hardware definition and releases

| ID | Result | Prerequisites | Acceptance boundary |
| --- | --- | --- | --- |
| HW-WP-002 | Specify the next development setup | None | Capability/inventory matrix, measurement needs, loan/buy alternatives, six-node expansion and owner/budget proposal. A proposal is not purchasing authority. |
| HW-WP-001A | Hardware requirements and desk trades | None | Measurable ranges, rationale, owners, fallbacks and discriminating measurements; architecture-changing choices remain provisional until measured inputs exist. |
| HR1 | Architecture downselect | HW-WP-001A, NFF3, PS1, VC1 | Interface allocation and power, thermal, RF and mechanical budgets; selection-critical experiments and qualified design accountability. |
| HR2 | Component baseline | HR1, HR0 | Exact parts/packages/footprints, derating, lifecycle/supply/cost, drivers/licenses, critical sample tests, credible alternatives and measured budget closure. |
| HR3 | Controlled schematic release | G1 | Reviewed schematic/netlist, calculations, pin/power/test/service interfaces and zero unwaived ERC violations. This release permits PCB routing. |
| HR4 | PCB and EVT build package | HR3 | Reviewed layout/stack-up, zero unwaived DRC violations, DFM/DFA/DFT, native EDA, fabrication/assembly files, BOM/AVL and test/traceability package. |
| HR5 | EVT exit | G2 | Traceable product-intent units pass electrical, RF, power, peripheral, recovery and six-node acceptance; architecture-changing defects are closed before G3. |

HR6 DVT exit and HR7 PVT exit follow the release criteria in the product
realization framework. Completing a technical release does not itself authorize
a purchase, fabrication order or product launch.

## Shared inputs and system acceptance

| ID | Result | Prerequisites | Acceptance boundary |
| --- | --- | --- | --- |
| PS1 | Product and interface baseline | None | Stable requirement IDs, measurable limits, interfaces, assumptions, verification methods and accountable owners. |
| VC1 | Verification and risk plan | None | Each critical requirement has a method, environment, owner and criterion; safety/compliance responsibilities and unresolved risks are explicit. |
| LAB0 | Confirm the safe two-NFF bench | None | Traceable inventory, exclusive access and an approved power/cabling plan covering current limits, supply injection, USB backfeed, grounding and debug connections. |
| LAB1 | Commission measurement capability | HW-WP-002, LAB0 | Instrument range, bandwidth and calibration; timing triggers/clock uncertainty; safe cabling and a dry run for each NFF3 measurement. |
| LAB6 | Six physical development nodes | HW-WP-002 | Six traceable radio nodes on supported profiles; inventory explicitly identifies missing touch, LED, audio or haptic capabilities. |
| FS-WP-003A | Shared drill and timing contract | None | Generated schema-owned semantics agree across firmware, simulator, CLI and app; scoring, authority, diagnostics and recovery coverage; required software and physical exits remain separately visible. |
| FS-WP-005A | EVT firmware profile | G1, HR2 | Exact selected parts/pins, partitions, update/rollback and resource limits, generated protocol compatibility and a clean product-profile build without NFF configuration leakage. |
| VC-WP-002B | Six-node alpha evidence release | FS-WP-004D | Current physical timing, fault, recovery and soak results close critical risks with uncertainty and artifact identity; unexplained simulator/hardware divergence is investigated. |
| PS2 | Accept the alpha product workflow | PS1, G1, FS-WP-004D | Physical alpha behavior meets accepted requirements; deviations, tradeoffs and resulting design changes are recorded. |

## Supporting simulation ladder

The detailed contracts and quantitative limits remain in
[architecture record 13](../research/architecture/13-deterministic-virtual-platform.md).

| ID | Result | Prerequisites | Acceptance boundary |
| --- | --- | --- | --- |
| FS-WP-002A | Deterministic host replay | None | Explicit host time, reproducible faults, delivery identity and exact replay. |
| FS-WP-002B | QEMU feasibility | FS-WP-002A | Pinned engine, repeatable target execution, fidelity inventory and bounded adoption cost. |
| FS-WP-002D | Production runtime composition | FS-WP-002B | Physical/QEMU composition roots, deterministic inputs and declared production/adapted/modeled/disabled behavior. |
| FS-WP-002C | Scheduler trace acceptance | FS-WP-002D | Stable scheduler/ISR/causal identity, fail-closed normalization, measured overhead and separately retained default-image physical checks. |
| FS-WP-002E | Production radio seam | FS-WP-002C | Production radio path and callback/ring/dequeue correlation with preserved wire format and bounded two-board regression. |
| FS-WP-002F | One DUT and virtual peer backplane | FS-WP-002E, FS-WP-003A | Production transport/task/codec execution, complete deterministic fault/replay matrix and role rotation within the adopted patch budget. |
| FS-WP-002G | Concurrency and fault qualification | FS-WP-002F | Separate scheduling, concurrency, mutant, fault, repeatability and shadow-CI evidence meeting the package's numerical limits. |
| FS-WP-002H | Calibrated prediction candidate | FS-WP-002G, HR0, FS-WP-003A | Calibration-only tuning, clock uncertainty, frozen bounds/envelope and explicit drift/invalidation rules. |
| VC-WP-002A | Independent held-out qualification | FS-WP-002H | Independent corpus and complete lineage, critical-mutant coverage, fixed metric bounds and a published trust verdict. |

Historical implementation can be merged while a package's evidence remains
incomplete. Closed issues or merged PRs do not satisfy missing acceptance criteria.
Later package evidence does not automatically close a different earlier physical
criterion.

## Program decisions

G1 requires HR0, HR1, HR2, PS1 and VC1, with all architecture-driving software and
interface risks resolved. It authorizes controlled schematic work; routing still
requires HR3. G2 requires HR4, FS-WP-005A, VC-WP-002B and PS2, no unresolved critical
design risk, and separately recorded spend authority for the immutable EVT package.

Predictive simulation may support a decision only inside its independently
qualified envelope. Direct physical evidence may close the relevant G2 risks
without a predictive-model claim. A failed prediction verdict does not erase useful
deterministic tests or passing physical evidence.

## Tracking

GitHub milestones group delivery by program phase: [P1 through G1](https://github.com/pcesar22/domes/milestone/1) and
[P2 through G2](https://github.com/pcesar22/domes/milestone/2). Package IDs above remain stable across issue consolidation. Issues
state the remaining result, prerequisites, acceptance and exclusions; PRs link the
issue and report evidence for their exact revision. Completed implementation,
pending verification, superseded work and future work remain distinct.

Later phases retain the framework's baseline schedule. Dates are forecasts, not
acceptance evidence; missed dates require an explicit reforecast rather than a
silent reset.
