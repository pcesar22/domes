# DOMES Integrated Program Status

This is the CEO-level delivery authority for DOMES. It separates program phases, functional
workstreams, hardware releases, and cross-functional investment decisions so progress in one
discipline cannot masquerade as product readiness.

The operating model is defined in
[`docs/PRODUCT_REALIZATION_FRAMEWORK.md`](docs/PRODUCT_REALIZATION_FRAMEWORK.md). Product hypotheses
belong in [`research/PRODUCT_DEFINITION.md`](research/PRODUCT_DEFINITION.md), target architecture in
[`research/SYSTEM_ARCHITECTURE.md`](research/SYSTEM_ARCHITECTURE.md), as-built software in
[`research/SOFTWARE_ARCHITECTURE.md`](research/SOFTWARE_ARCHITECTURE.md), and verification procedures
in [`docs/TESTING.md`](docs/TESTING.md).

**As of:** 2026-08-05, FS-WP-002B and FS-WP-002D are consolidated in one simulation delivery,
[PR 100](https://github.com/pcesar22/domes/pull/100), against `main`. FS-WP-002D is `Complete` /
`Green`: required [Software CI run 31039047667](https://github.com/pcesar22/domes/actions/runs/31039047667)
rebuilt runtime implementation head `f36447f931f9216b7733ff4685ffc5ccaab895ce`, passed 100 identical
QEMU runtime processes, and passed the aggregate `CI Gate`. Its qualified `firmware/domes` source tree is
`f0db7c5516879de37a5a04ec2ee052ace8ebe0f2`. Generated qualification output is not stored in Git;
the PR records the manual campaign outcomes and CI retains the exact-checkout result. Every later PR
head, including status-only changes, remains subject to the same required checks before merge.

## CEO Control Panel

| Control | Current state |
| --- | --- |
| Active program phase | P1 Definition and Feasibility, `Active` |
| Overall health | `Amber` |
| Current development hardware | Two NFF ESP32-S3 N8R8 boards; development carriers, not product prototypes |
| NPI stage | Pre-EVT; no production-intent schematic, layout, M-BOM/AVL, or build package |
| Next program gate | G1 System Architecture Baseline and Schematic Authorization |
| Gate baseline / forecast | 2026-09-15 / 2026-09-15 |
| Forecast confidence | `Low` until HW owner, NFF characterization, and requirements inputs are established |
| Latest completed execution package | `FS-WP-002D`, `Complete` / `Green`; consolidated [PR 100](https://github.com/pcesar22/domes/pull/100), issue [99](https://github.com/pcesar22/domes/issues/99), [plan](docs/plans/qemu-simulation.md), and required [Software CI](https://github.com/pcesar22/domes/actions/runs/31039047667) passed |
| Next program action | Select and start VC1 now; in parallel, record the HW owner/budget decision and then start HW-WP-001 while active PS1 and FS1 work continues |
| Next autonomous execution delivery | `FS-WP-002C` is eligible and next in the simulation ladder; it remains unselected and requires a new execution cycle |
| Current AI execution blocker | None for selecting `FS-WP-002C`; its bounded package has not been started |
| PR merge condition | The final PR head must pass required Software CI, including 100 fresh QEMU runtime processes and aggregate `CI Gate`; live GitHub checks are authoritative |
| Next CEO/external decision | Name the HW design owner and approve the bounded HW-WP-001 definition/risk-prototype budget |
| Immediate hardware work authorization | [`HW-WP-001`](hardware/NEXT_ITERATION_REQUEST.md), `Ready now` |
| First product-hardware purchase authorization | G2 EVT Release to Fab, forecast 2026-11-02 |

### Executive Answer

The AI-driven team can start the next hardware request now. The request is **hardware definition and
risk reduction**, not an EVT board order. It authorizes architecture trades, exact part selection,
supplier and CM engagement, preliminary BOM/AVL, power/RF/battery/mechanical budgets, evaluation
kits/coupons, test architecture, and an EVT input package.

The team may start that work now because the NFF platform is controlled enough to measure remaining
unknowns and because waiting for complete software or customer validation would waste the definition
window. The team may not freeze selections that depend on missing evidence, release a schematic,
start PCB layout, or order EVT boards until the corresponding gates pass.

### What Starts Now

| Work | Authorization | Immediate boundary |
| --- | --- | --- |
| HW-WP-001 NFF Characterization and Product Architecture Downselect | `Ready now` | HR0-HR2 evidence; analysis, supplier work, evaluation kits, and coupons only |
| Product/system requirement allocation | `Active` | Stabilize hardware-driving values or explicit ranges/fallbacks for G1 |
| Physical NFF closure | `Active` | Both boards, all peripherals, exact populated parts, current/power/timing/RF evidence |
| Deterministic virtual platform | `Active` | A, B, and D are complete; C is eligible but unselected; predictive claims still require the remaining FS2 ladder and independent held-out qualification |
| General drill/protocol convergence | `Ready now` | One protobuf-owned drill, timing, authority, result, and recovery contract across firmware/sim/app |
| Four economical ESP32-S3 alpha nodes | `Ready now` | System-scale development inventory, not product hardware |
| FMEA, compliance, supply, CM, DFM/DFT, and test planning | `Ready now` | Planning and risk closure; no approval or production claim |

### Current Execution Delivery

#### FS-WP-002D: Simulation Composition And Platform Inputs

**Objective:** Run the shared production runtime through mutually exclusive physical and QEMU roots,
with deterministic platform inputs, exhaustive fidelity declarations, fail-closed build/runtime
validation, and preserved physical behavior.

| Contract | Current state |
| --- | --- |
| Owner | AI simulation lead |
| State / health | `Complete` / `Green` |
| Execution issue / review package | Issue [99](https://github.com/pcesar22/domes/issues/99); one consolidated [PR 100](https://github.com/pcesar22/domes/pull/100) |
| Inputs | FS-WP-002B feasibility foundation in the same PR; ESP-IDF v5.4.4; two registered NFF boards and Intel AX210 |
| Dependencies/blockers | None for D's technical exit; runtime implementation head `f36447f931f9216b7733ff4685ffc5ccaab895ce` passed 100 fresh QEMU runtime processes and aggregate `CI Gate` in [run 31039047667](https://github.com/pcesar22/domes/actions/runs/31039047667); the final PR head still requires green CI before merge |
| Gate/risk unlocked | Scheduler/ISR/causality observability package `FS-WP-002C` is eligible but unselected |
| Stop condition | Met; C and later radio/predictive work remain outside this delivery |

The prior manual technical campaign passed with 100/100 fresh QEMU target processes, one normalized
readiness signature, exact configured and linked closure, and a two-board
serial/BLE/ESP-NOW/trace regression against the same `firmware/domes` source tree retained by this
PR. The campaign completed 600/600 radio benchmark rounds and restored device state. Current host
tooling and independent review pass. Required Software CI then rebuilt the exact checkout, executed
100 fresh production-runtime QEMU processes with one readiness signature, and passed the aggregate
gate. This is a declared production/adapted/modeled/disabled profile, not scheduler coverage,
RF/peripheral simulation, hardware equivalence, or predictive evidence.

### FS2 Deterministic Virtual Platform Ladder

The implementation contract and claim boundaries are authoritative in
[`research/architecture/13-deterministic-virtual-platform.md`](research/architecture/13-deterministic-virtual-platform.md).
These forecasts are planning targets, not evidence. Entry criteria, not date, authorize a package.

| Package | Bounded outcome | State / health | Entry | Binary exit | Baseline | Forecast |
| --- | --- | --- | --- | --- | --- | --- |
| FS-WP-002A | Deterministic host clock, network faults, identity, and exact delivery replay | `Complete` / `Green` | Existing host simulator | Merged [PR 97](https://github.com/pcesar22/domes/pull/97), exact replay and green required CI | 2026-08-04 | 2026-08-04 actual |
| FS-WP-002B | ESP32-S3 QEMU feasibility and adoption decision | `Complete` / `Green` | `A` complete | `Viable`: 100/100 fixed runs, HMP/GDB, immutable pinned engine, complete fidelity inventory and numeric adoption budget | 2026-08-14 | 2026-08-04 actual |
| FS-WP-002D | Physical/QEMU composition roots and deterministic platform inputs | `Complete` / `Green` | `B` is `Viable`; issue [99](https://github.com/pcesar22/domes/issues/99) | 100/100 QEMU runs, source-equivalent two-board regression, and required exact-checkout 100-process QEMU runtime CI and aggregate gate passed | 2026-08-25 | 2026-08-05 actual |
| FS-WP-002C | Stable scheduler, ISR, synchronization, causality, and trace normalization | `Ready` / `Amber` | `D` passed; package eligible but not selected or started | QEMU and hardware use one bounded raw/normalized trace contract with stable IDs, full causal graph, overflow failure, and measured overhead | 2026-09-04 | 2026-09-04 |
| FS-WP-002E | Production `IEspNowRadio` seam and trace correlation below `EspNowTransport` | `Not due` / `Not rated` | `C` passes and this package is selected | Physical path passes two-board regression; causal tokens cross callback/ring/dequeue without wire or pending-frame-capacity change | 2026-09-11 | 2026-09-11 |
| FS-WP-003A | Portable protobuf-owned peer/drill codec and role semantics, required FS3 input | `Ready` / `Red` | Current physical/simulator/app contracts and compatibility baseline identified | Generated nanopb/prost/Dart contract replaces duplicated semantics and passes host/app/CLI/build/two-board migration regression | 2026-09-15 | 2026-09-15 |
| FS-WP-002F | One real QEMU DUT plus deterministic in-process peer backplane | `Not due` / `Not rated` | `E` and `FS-WP-003A` pass | Production transport/task/codec path passes complete deterministic fault and exact-replay matrix with role rotation inside patch budget | 2026-09-28 | 2026-09-28 |
| FS-WP-002G | Scheduling, concurrency, mutation, fault-campaign, and CI tiers | `Not due` / `Not rated` | `F` passes | Critical mutants all detected; evidence modes remain distinct; 1,000 repeats and 20 shadow jobs meet zero-flake and p95 runtime limits | 2026-10-12 | 2026-10-12 |
| FS-WP-002H | Hardware-calibrated model and frozen prediction candidate | `Not due` / `Not rated` | `G`, FS1, and stable FS3 evidence | Calibration-only tuning, clock uncertainty, frozen bounds/envelope, drift and invalidation rules | 2026-10-26 | 2026-10-26 |
| VC-WP-002A | Independent held-out predictive qualification | `Not due` / `Not rated` | `H` frozen and independent corpus fixed | 100% critical and >=95% complete in-envelope mutant detection, fixed metric bounds, no unexplained held-out divergence, published verdict | 2026-10-30 | 2026-10-30 |

`FS-WP-003A` is shown because it is a hard software dependency for the target peer backplane. It
advances FS3, not FS2, and does not count as simulation progress by proximity.

Dependency order is `A -> B -> D -> C -> E`, with `FS-WP-003A` able to proceed independently;
then `(E + FS-WP-003A) -> F -> G`, then `(FS1 + FS3 + G) -> H -> VC-WP-002A`.
FS2 completes only on the independent `VC-WP-002A` pass. That pass closes the simulation criterion
inside VC2; VC2 also requires separate six-node alpha, fault, soak, and timing evidence. Earlier
packages are valuable deterministic test infrastructure but do not authorize the word "predictive."

### What Is Not Authorized

- Final architecture or component freeze before G1.
- Product schematic or PCB layout release before G1.
- Manufacturing-file release or an EVT purchase order before G2.
- Carrying candidate NFF/proposal circuits into the product by default.
- Production, safety, certification, reliability, simulation-predictiveness, or launch claims based
  on a build, accepted command, NFF automation, or unchecked model.

### Decisions Required From The CEO

| Needed by | Decision | Team recommendation | Consequence if late |
| --- | --- | --- | --- |
| Now | Name HW design owner and approve HW-WP-001 definition/risk-prototype budget | `Authorize` | G1 forecast immediately loses credibility |
| 2026-08-07 | Approve four inexpensive alpha nodes and bounded evaluation/coupon spend | `Authorize` | Six-node and selection-critical evidence misses G2 |
| 2026-09-15 | G1 disposition and any explicit exceptions | `Go` only on passing evidence | Schematic/layout remains unauthorized |
| 2026-11-02 | G2 EVT release and approximately 10-20-unit build spend | Decide from immutable release package | No product-intent hardware order |

AI owns evidence audit, traceability, inconsistency detection, the technical gate verdict, and the
resulting evidence-status transition. The CEO owns budget, vendor, and market commitments. A qualified
design owner is accountable for the controlled hardware design; passing evidence does not manufacture
that accountability by implication.

## Program Model

### Terms

| Object | ID | Meaning |
| --- | --- | --- |
| Program phase | `P0`-`P7` | A bounded cross-functional execution interval with one entry and one exit gate |
| Decision gate | `G0`-`G7` | A zero-duration decision that authorizes a specific technical, spend, manufacturing, or market commitment |
| Workstream | `PS`, `FS`, `HW`, `VC` | Continuous Product/System, Firmware/Software, Hardware/NPI, or Verification/Compliance work |
| Work package | e.g. `HW-WP-001` | A bounded functional assignment with inspectable outputs and stop conditions |
| Hardware release | `HR0`-`HR7` | A hardware evidence checkpoint; it does not replace a cross-functional program gate |

One program phase is active at a time, but all workstreams run concurrently inside it. A work package
may be pulled forward to retire a named risk when its stop condition prevents premature commitment.
Future phases are `Not entered` and `Not rated`, not falsely reported as blocked.

### Program Phases

| Phase | Status | Health | Execution interval | Workstream objective | Exit gate |
| --- | --- | --- | --- | --- | --- |
| P0 Development Foundation | `Closed` | `Green` | Through 2026-08-03 | Reproducible CI and controlled two-board development platform | G0 `Passed` |
| P1 Definition and Feasibility | `Active` | `Amber` | 2026-08-03 to 2026-09-15 | Product/system baseline, NFF characterization, architecture/parts, interfaces, verification and risk plan | G1 |
| P2 Integrated Alpha and EVT Design | `Not entered` | `Not rated` | 2026-09-16 to 2026-11-02 | Six-node critical paths plus released schematic/layout/manufacturing package | G2 |
| P3 EVT Build and Qualification | `Not entered` | `Not rated` | 2026-11-03 to 2027-02-08 | Build, bring up, correct, and qualify product-intent electrical prototypes | G3 |
| P4 DVT Product Validation | `Not entered` | `Not rated` | 2027-02-09 to 2027-08-02 | Frozen form-factor product, full V&V, user, reliability, security, and compliance evidence | G4 |
| P5 PVT and Launch Readiness | `Not entered` | `Not rated` | 2027-08-03 to 2027-11-01 | Intended-line process, yield, traceability, logistics, support, and release candidate | G5 |
| P6 Open Product Release | `Not entered` | `Not rated` | 2027-11-02 to 2027-12-13 | Immutable product/software/open-source package and market readiness | G6 |
| P7 Sustainment | `Not entered` | `Not rated` | Starts after G6 | Quality, security, updates, spares, returns, continuing compliance, and product learning | G7 handoff |

### Decision Gates

| Gate | Decision | Inputs from all workstreams | Baseline | Current state | Authorization |
| --- | --- | --- | --- | --- | --- |
| G0 | Development Foundation | CI, two-board automation, configuration/evidence identity | 2026-08-03 | `Go` actual | Enter P1 and run parallel definition work |
| G1 | System Architecture Baseline and Schematic Authorization | Hardware-driving product/system requirements; HR0-HR2; software/interfaces; V&V, FMEA, RF/compliance, supply and manufacturing concepts | 2026-09-15 | `Active inputs` | Freeze bounded architecture/parts and start controlled schematic capture/layout planning; PCB routing requires HR3 |
| G2 | EVT Release to Fab | Critical six-node alpha paths and physical timing/fault/soak evidence; simulation verdict when used as evidence; HR3-HR4; released BOM/AVL/build/test package; firmware EVT profile; closed critical design risks | 2026-11-02 | `Planned` | Order approximately 10-20 traceable EVT units |
| G3 | EVT Exit / DVT Authorization | Product-intent electrical, power/battery, RF, peripherals, firmware lifecycle, factory test, DFM/DFT and defect closure | 2027-02-08 | `Planned` | Freeze corrected design and build approximately 30-100 DVT units |
| G4 | DVT Exit / PVT Authorization | Requirements V&V, six-pod product, reliability/environment, security, user validation, compliance and manufacturing readiness | 2027-08-02 | `Planned` | Pilot intended line with approximately 100-300 PVT units |
| G5 | PVT Exit / Release Candidate | Yield, traceability, process capability, factory test, logistics, regression, support and immutable candidate evidence | 2027-11-01 | `Planned` | Produce final release package and launch inventory |
| G6 | Open Product Release | Green candidate CI/hardware, approvals, security, licensing, editable design/manufacturing sources, support and launch readiness | 2027-12-13 | `Planned` | Release product and enter sustainment |
| G7 | Sustainment Handoff | Stable ownership of quality, vulnerabilities, updates, spares, returns, evidence retention and continuing compliance | Set after launch | `Planned` | Close initial realization program |

Technical gate verdicts are `Go`, `Conditional Go`, `Hold`, `Recycle`, or `Stop`. `Conditional Go`
requires an explicit exception, affected evidence, accepted consequence, owner, and closure date. The
AI milestone manager records that verdict from direct evidence. Where a gate enables spend, a vendor
commitment, or a market commitment, the CEO records a separate authorization; that business action
cannot upgrade a failing technical verdict.

FS2 predictiveness is not by itself a mandatory G2 pass criterion. When `VC-WP-002A` fails, no model
prediction may support the G2 verdict; the physical six-node timing, fault, soak, and recovery
evidence must independently close every affected critical risk. Any unexplained simulator/hardware
divergence that could indicate a firmware or hardware design defect remains a critical open risk and
prevents G2 `Go` until it is resolved. This preserves a hardware-evidence path to EVT without
laundering a failed model into gate evidence.

## Workstream Status

### Product And System (`PS`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| PS0 | Product brief and launch hypothesis | CEO/product owner with AI product lead | `Active` | `Amber` | 2026-08-14 | Separate evidence-backed value from assumptions; name buyer, user, job, kit, environment and economic bounds |
| PS1 | Hardware-driving product/system baseline and traceability | AI systems lead | `Active` | `Amber` | 2026-09-07 | Allocate measurable requirements/interfaces and verification methods; use bounded fallback where discovery is incomplete |
| PS2 | App-driven six-node system alpha | AI systems lead | `Ready` | `Amber` | 2026-10-19 | Acquire four nodes; unify drill, authority, timing, failure, result and coexistence requirements |
| PS3 | DVT user/product validation | Product/UX owner, unassigned | `Not due` | `Not rated` | 2027-08-02 | Starts on representative frozen units; continues customer/economic validation before it |
| PS4 | Launch offer, price, channel, support and warranty | CEO/product owner | `Not due` | `Not rated` | 2027-12-13 | Close from customer, DVT, PVT, cost and support evidence |

### Firmware, Software, CLI, App, And Simulation (`FS`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| FS0 | Reproducible CI and automated two-board platform | AI firmware/software lead | `Complete` | `Green` | 2026-08-03 actual | Preserve required CI and rerun hardware evidence after behavioral change |
| FS1 | Complete physical NFF reference and product-interface inventory | AI firmware/software lead | `Active` | `Amber` | 2026-08-24 | Finish audio/volume; observe peripherals; capture current/power/timing; confirm exact parts |
| FS2 | Layered deterministic virtual platform with a measured prediction envelope | AI simulation lead | `Active` | `Amber` | 2026-10-30 | `A`, `B`, and `D` are complete; C is eligible but unselected; confidence remains `Low` until the full ladder and independent qualification pass |
| FS3 | One production-owned drill/runtime contract across firmware, simulator and app | AI firmware/software lead | `Ready` | `Red` | 2026-10-19 | Deliver `FS-WP-003A` portable protobuf peer/drill contract by 2026-09-15, then close fixed two-pod and host-wall-clock scoring divergence |
| FS4 | Six-node runtime, mobile/control, diagnostics, failure recovery and soak | AI systems/software lead | `Ready` | `Amber` | 2026-10-19 | Execute on NFF plus economical alpha nodes; feed critical results into G2 |
| FS5 | EVT BSP, board profile, factory/service tooling and bring-up | AI firmware lead | `Not due` | `Not rated` | 2027-02-08 | Scaffold after G1; exact profile must build before G2 and remain separate from NFF |
| FS6 | DVT/PVT/release software candidates | AI firmware/software lead | `Not due` | `Not rated` | 2027-12-13 | Bind each candidate to exact hardware, app, CLI, tests, update and recovery evidence |

### Hardware And NPI (`HW`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| HW0 | NFF as-built and measured characterization reference | AI test lead; lab operator for physical observations | `Active` | `Amber` | 2026-08-24 | Measure physical/peripheral/current/power/timing/RF behavior; do not infer it |
| HW1 | Product architecture and component baseline | HW design owner, unassigned | `Ready` | `Amber` | 2026-09-15 | Issue HW-WP-001 now; close architecture trades, exact parts, alternates, budgets and risk coupons |
| HW2 | Controlled EVT schematic, PCB, M-BOM/AVL and build/test package | HW design owner, unassigned | `Not due` | `Not rated` | 2026-11-02 | Starts after G1; no manufacturing release before G2 |
| HW3 | Traceable EVT fabrication and assembly | HW design owner/CM, unassigned | `Not due` | `Not rated` | 2026-12-14 | Starts after G2 with controlled revisions, substitutions and deviations |
| HW4 | EVT electrical bring-up and design correction | HW design owner with AI firmware/test leads | `Not due` | `Not rated` | 2027-02-08 | Close architecture-changing issues and prepare design freeze |
| HW5 | DVT form-factor build and qualification | HW/ME/CM owners, unassigned | `Not due` | `Not rated` | 2027-08-02 | Near-final enclosure, charging, reliability, compliance and six-pod product evidence |
| HW6 | PVT manufacturing-system proof | Operations/CM/quality owners, unassigned | `Not due` | `Not rated` | 2027-11-01 | Intended CM, process, fixtures, traceability, yield, packaging and failure disposition |

### Verification, Compliance, Reliability, And Operations (`VC`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| VC0 | Trustworthy software and automated NFF evidence pipeline | AI verification lead | `Complete` | `Green` | 2026-08-03 actual | Preserve evidence identity and aggregate CI |
| VC1 | Verification matrix, preliminary FMEA, compliance and qualification plan | AI verification lead; compliance owner unassigned | `Ready` | `Amber` | 2026-09-15 | Start now; define launch-market route, experts, critical tests and evidence ownership |
| VC2 | Six-node alpha verification and independent simulation qualification | AI verification/simulation leads | `Ready` | `Amber` | 2026-10-30 | Close six-node fault/soak/timing evidence by 2026-10-19 and the independent `VC-WP-002A` verdict by 2026-10-30 |
| VC3 | EVT verification and defect closure | AI verification lead with HW design owner | `Not due` | `Not rated` | 2027-02-08 | Validate design intent and testability on traceable EVT units |
| VC4 | DVT reliability, security, user and formal compliance evidence | Quality/compliance owners, unassigned | `Not due` | `Not rated` | 2027-08-02 | Execute frozen verification matrix on near-final units |
| VC5 | PVT process capability, release regression and operations acceptance | Quality/operations owners, unassigned | `Not due` | `Not rated` | 2027-11-01 | Prove factory, logistics, support, update and release evidence |

## Hardware Release Ladder

Hardware work progresses through its own technical releases. These checkpoints feed program gates;
they are not product phases.

| Release | State | Required evidence | Authorizes |
| --- | --- | --- | --- |
| HR0 NFF Reference Closure | `Active` | Both serialized boards; as-built identity; observed LED/touch/IMU/haptic/audio; idle/radio/LED/audio/haptic/combined current and transient data | Final measured architecture inputs |
| HR1 Architecture Downselect | `Ready` | Product/system allocation; ID/mechanical envelope; power/thermal/runtime, RF/antenna, charging, service/programming trades; preliminary FMEA; selection-critical coupons | Architecture recommendation |
| HR2 Component Baseline | `Ready` | Exact MPN/package/datasheet; derating; tested sample where critical; footprint; lifecycle/supply/cost; alternate/mitigation; driver/compliance evidence | G1 part freeze and controlled schematic work |
| HR3 Schematic Release | `Not due` | Controlled schematic/netlist; power tree; pin map; partitions/profile; calculations; test points; programming/calibration; zero unwaived ERC; cross-discipline review | PCB layout, not fabrication |
| HR4 PCB Release To Fab | `Not due` | CM stack-up; RF/power/return/touch/LED/thermal constraints; 3D clearance; zero unwaived DRC; DFM/DFA/DFT; native EDA, Gerbers/drill, drawings, CPL, M-BOM/AVL, fixture plan, checksums | G2 decision on one EVT order |
| HR5 EVT Exit | `Not due` | Traceable units; power/battery/fault/thermal, RF/coexistence, physical peripherals, product profile, factory programming, OTA/rollback/recovery and DFT; six working nodes; no architecture-changing defect | G3 DVT decision |
| HR6 DVT Exit | `Not due` | Frozen units meet product, six-pod, reliability/environment, charging, security/service, user and launch-market requirements | G4 PVT decision |
| HR7 PVT Exit | `Not due` | Intended-line traceability, ratified yield/process capability, zero critical escapes, sampled DVT regression and logistics evidence | G5 release-candidate decision |

[`hardware/NEXT_ITERATION_REQUEST.md`](hardware/NEXT_ITERATION_REQUEST.md) covers HR0-HR2. It does not
authorize HR3, HR4, or a fabrication purchase.

## G1 Critical Path And Evidence

The critical path to the next irreversible design commitment is:

`HW owner/budget -> NFF characterization + product/system allocation + HW trades + VC plan -> HR1/HR2 -> G1`

| Required input | Current state | Pass result |
| --- | --- | --- |
| Named HW owner, definition budget, supplier/CM access and dated plan | `Open` | HW-WP-001 becomes active with accountable delivery |
| Hardware-driving product/system requirements and interface allocation | `Not run` | Stable values or explicit ranges/fallbacks across product, HW, FW, mechanical and test |
| NFF physical, exact-population, current, power, timing and RF record | `Unverified` | Measured reference and known limitations |
| Architecture block, interface-control record and mechanical/ID envelope | `Not run` | Every rail, bus, pin, timing, boot, debug, test, RF and physical interface has an owner |
| Component matrix and preliminary M-BOM/AVL | `Not run` | Selected/alternate parts close requirement, lifecycle, supply, cost, compliance, driver and manufacturing needs |
| Power/battery/charging/thermal/runtime and resource budgets | `Not run` | Typical/worst-case budgets close with stated margin |
| Preliminary FMEA, RF/compliance route and risk-coupon results | `Not run` | No hidden critical risk capable of invalidating schematic/layout |
| Firmware BSP/profile and factory/service interface plan | `Not run` | Selected parts are supportable without NFF/product configuration leakage |
| Manufacturing/test/fixture/traceability concept and budgetary EVT plan | `Not run` | Design will be programmable, observable, testable, recoverable and sourceable |

G1 may use `Conditional Go` only for an item that cannot change topology, selected critical parts,
PCB outline/stack-up, placement, interfaces, safety, compliance route, or firmware architecture.

## Near-Term Integrated Plan

| Date | Integrated result | Decision impact |
| --- | --- | --- |
| 2026-08-05 | FS-WP-002B and D consolidated into one reviewable simulation delivery with reproducible tooling, concise qualification, independent reviews, and passing exact-checkout QEMU CI | D is complete and C is eligible but unselected; no scheduler, hardware-equivalence, or predictive claim is created |
| 2026-08-14 | Product brief, hardware-driving requirement draft, and initial risk register | Conflicts and missing measurements surface early while simulation implementation proceeds on its separate ladder |
| 2026-08-24 | NFF physical/peripheral/electrical characterization baseline | Guesses are replaced before selection freeze |
| 2026-08-31 | Architecture/component shortlist and bounded risk-coupon review | Weak candidates are removed |
| 2026-09-07 | Requirements/interface baseline candidate, budgets, RF/compliance and manufacturing/test concepts | G1 package becomes auditable |
| 2026-09-15 | G1 System Architecture Baseline | Schematic capture may start only on `Go`; PCB routing still requires HR3 |
| 2026-10-19 | Critical six-node alpha paths and unified drill/timing contract pass | Physical product behavior and stable FS3 semantics unlock final calibration |
| 2026-10-30 | Independent simulation trust verdict | Passing creates the bounded prediction envelope; failure removes predictive claims but does not erase deterministic test value or independently passing physical evidence |
| 2026-11-02 | G2 EVT Release To Fab | First product-intent board order may be placed |
| 2026-12-14 | Traceable EVT units available | Product-board bring-up starts |
| 2027-02-08 | G3 EVT Exit | Corrected frozen DVT design may proceed |
| 2027-08-02 | G4 DVT Exit | Intended-line pilot may proceed |
| 2027-11-01 | G5 PVT Exit | Immutable release candidate may proceed |
| 2027-12-13 | G6 Open Product Release | Product may ship and enter sustainment |

## Evidence Register

### Software And Automated NFF Evidence

| Evidence | Source | Result | Boundary |
| --- | --- | --- | --- |
| Current main software CI | Commit `c0691f34f68c8e671f1023f1dabc05cea1526344`, [run 30943047671](https://github.com/pcesar22/domes/actions/runs/30943047671) | Passed | Builds, tests, generated artifacts, lint, docs, Flutter Linux/iOS, ESP-IDF release checks, and aggregate `CI Gate` |
| Deterministic replay foundation | [PR 97](https://github.com/pcesar22/domes/pull/97), merged 2026-08-04 | Accepted | FS-WP-002A only: explicit host time, deterministic faults, delivery identity, and exact delivery replay; no trace-normalization, target-scheduler, or predictive claim |
| ESP32-S3 QEMU simulation delivery | [PR 100](https://github.com/pcesar22/domes/pull/100), [Software CI run 31039047667](https://github.com/pcesar22/domes/actions/runs/31039047667) | `B` is `Viable`; `D` is `Complete` / `Green`; exact-checkout CI rebuilt runtime implementation head `f36447f931f9216b7733ff4685ffc5ccaab895ce`, executed 100 identical fresh production-runtime QEMU processes, and passed aggregate `CI Gate`; manual 100/100 campaigns, linked closure, source-equivalent two-board regression, current host tooling, and independent review also passed; every later PR head remains gated before merge | Target execution and declared production/adapted/modeled/disabled runtime profile only; successful results stay in CI logs and failure diagnostics are uploaded outside Git; no scheduler-trace, radio/RF, peripheral-actuation, cycle-accuracy, hardware-equivalence, or predictive claim |
| Repository effectiveness acceptance | [PR 85](https://github.com/pcesar22/domes/pull/85), merged 2026-08-03 | Accepted | Instructions, verification orchestration, pinned toolchains and CI behavior |
| Automated hardware CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30785241480](https://github.com/pcesar22/domes/actions/runs/30785241480) | Passed | Two NFF boards, serial/BLE/ESP-NOW/OTA/diagnostics/trace; no physical observation |

The accepted software baseline passed 283 host firmware tests, 100 Rust CLI tests, 161 Flutter
tests, generated-binding checks, a clean ESP-IDF v5.4.4 build, and the aggregate `CI Gate`. Live CI
and test discovery outrank historical counts.

### Retained Two-Board Campaign

The 2026-08-02 campaign used two NFF ESP32-S3 N8R8 boards, CP2102N UART bridges, and an Intel AX210
BLE adapter. Exact artifacts built from `99db4b77cc58a6695b86b7122ea5ee77fa9cbecb`:

| Purpose | Embedded version | `domes.bin` SHA-256 | `domes.elf` SHA-256 |
| --- | --- | --- | --- |
| Baseline and factory programming | `v0.0.0-0-g99db4b77cc58` | `9b27881a78d0d800277dc1cf6900b4e96f6e8ac3d221614355281ae43e176122` | `e01eb0fc66fe9efe34fd24eb47967bee5c792fa9c17e0f6059c911ad00fc0831` |
| Accepted serial/BLE OTA and runtime | `v0.0.0-1-g99db4b77cc58` | `cafb9c480f04b8d67f599977f84fd437fb2d0d0786c52a50e1205f4dc26f510b` | `e2fe59021b24c09b4cfc53ca961b5f1389762fd1160a55c96670ff86f0fb1ce7` |
| Forced rollback | `v0.0.0-2-g99db4b77cc58` | `d065b384e95e06251d48e6e3ee2590af81c6895afd449d39ddac56c78ecc03b8` | `5831c1206a22c4ab9f8264c0c3ac85985e00ea53cadca1a0a42435ad4c03be78` |

| Board | Stable CP2102N identity | Campaign identity |
| --- | --- | --- |
| Pod 1 | `5edf3f45576def11a245cea7c169b110` | Pod 1; WiFi `94:a9:90:0a:eb:c0`; BLE `94:A9:90:0A:EB:C2` |
| Pod 2 | `002a9f8e536def119f38c1a7c169b110` | Pod 2; WiFi `94:a9:90:0a:ea:50`; BLE `94:A9:90:0A:EA:52` |

The campaign verified erase/factory programming, UART/BLE diagnostics, control, registry fan-out,
serial/BLE OTA and recovery, forced rollback, two-way ESP-NOW, a traced drill, restart-snapshot
symbolization and a 620-second soak. It did not physically confirm light, touch, motion, vibration,
or sound.

Across three fresh ESP-NOW lifecycles per direction, both boards received 300/300 benchmark packets.
Observed command/acknowledgment round trips were 2.644-18.910 ms and 2.688-20.780 ms. They are not
synchronized one-way measurements and do not prove sub-millisecond behavior.

## Top Program Risks

| Risk | Consequence | Owner | Required resolution |
| --- | --- | --- | --- |
| HW owner/budget/supplier access not recorded | HW1 cannot become active and G1 date is low confidence | CEO/program | Authorize HW-WP-001 now |
| Physical NFF and engineering baseline incomplete | Part/architecture decisions may optimize against guesses | FS/HW | Close FS1/HW0/HR0 by 2026-08-24 |
| Product/system allocation incomplete | G1 cannot bound interfaces or selection | PS/VC | Produce PS1 candidate by 2026-09-07 |
| Firmware, simulator and Flutter drill/timing paths diverge | System alpha and model cannot prove product behavior | FS | Establish protobuf-owned semantics, shared production logic and correlated timing before G2 |
| QEMU peripheral gaps or future patch maintenance exceed the adopted budget | Target execution may bypass production behavior or become uneconomic to sustain | FS | Enforce FS-WP-002B's 10-file/2,500-line/path/effort ceilings; reopen the engine decision on any breach or unexplained divergence |
| FS3 stability precedes calibration and leaves three days between simulation qualification and G2 | The prediction envelope may miss the G2 review window | FS/VC | Deliver FS-WP-003A by 2026-09-15, preserve direct physical G2 evidence, and reforecast FS2 immediately on dependency variance |
| Deterministic single-thread QEMU serializes two vCPUs | A repeatable run can miss true parallel races and cannot imply cycle accuracy | FS/VC | Keep schedule sweeps, MTTCG stress, native sanitizers, and hardware differential evidence separate and mandatory in their declared envelopes |
| Only two NFF boards exist | Six-node behavior cannot inform EVT release | PS/FS | Procure four inexpensive alpha nodes by 2026-08-07 |
| Candidate power/charging/RGBW/battery topology is unproved | Unsafe or underpowered architecture could be carried into EVT | HW/VC | Challenge by analysis/coupons; do not inherit proposal parts by default |
| Launch market, economics, license and support are unset | Hardware or release scope may miss mandatory constraints | PS/VC | Bound at G1; close before DVT |

## Reporting Contract

Every status report answers, in order:

1. Active program phase, current development hardware, NPI stage, revision/date and overall health.
2. Next program gate, baseline/forecast/confidence, authorization, open critical evidence and
   recommended disposition.
3. Current execution package, separate next program action and autonomous execution delivery,
   acceptance boundary, blocker and execution issue.
4. Immediate hardware authorization: definition, schematic/layout, EVT, DVT or PVT; never say only
   “hardware can start.”
5. `PS`, `FS`, `HW`, and `VC` workstream outcome, delivered/now/next, health and forecast.
6. Hardware release-ladder position and the exact next evidence release.
7. Critical path and top risks with owner, mitigation and decision-by date.
8. CEO decisions required, team recommendation, alternatives and consequence of delay.
9. Changes to scope, schedule, cost, requirements, configuration, evidence or risk since last review.

No percentage rollup is permitted. Gate readiness, hardware release state, evidence and dated
workstream outcomes are the program truth.
