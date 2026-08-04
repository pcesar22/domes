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

**As of:** 2026-08-04, `main` after merged PR 90 plus the PS-WP-001 and PS-WP-002 stacked review
packages

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
| Current execution package | `PS-WP-002` review-ready in stacked [PR 94](https://github.com/pcesar22/domes/pull/94); [issue 93](https://github.com/pcesar22/domes/issues/93) owns execution evidence |
| Next AI-owned action | `VC-WP-001` G1 Verification Matrix, FMEA, And Compliance Plan; reserved for the next directive |
| Current AI execution blocker | None for a bounded VC-WP-001 candidate; qualified compliance/HW owners, launch-market decisions, and physical evidence remain explicit external inputs |
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
| Deterministic simulation foundation | `Ready now` | Shared interfaces and exact replay; predictive claims require held-out hardware validation |
| General drill/protocol convergence | `Ready now` | One protobuf-owned drill, timing, authority, result, and recovery contract across firmware/sim/app |
| Four economical ESP32-S3 alpha nodes | `Ready now` | System-scale development inventory, not product hardware |
| FMEA, compliance, supply, CM, DFM/DFT, and test planning | `Ready now` | Planning and risk closure; no approval or production claim |

### Current Package And Reserved Next Work

#### PS-WP-001: Product Brief And Canonical Six-Pod Workflow

**Objective:** Turn the current product hypotheses into one internally consistent product brief and
end-to-end six-pod reference workflow that can drive measurable requirements without presenting
uncollected customer evidence as fact.

| Contract | Current state |
| --- | --- |
| Owner | AI product/system lead |
| State / health | `Review ready` / `Green` |
| Actual start / review boundary | 2026-08-04 / 2026-08-04 |
| Inputs | Product definition, target system/ID architecture, as-built software architecture, current program evidence |
| Dependencies/blockers | None for the hypothesis baseline; customer interviews and purchase evidence remain open PS0 evidence |
| Gate/risk unlocked | Feeds PS1 hardware-driving requirements, FS3 runtime semantics, HW1 trades, VC1 verification planning, and G1 |
| Execution authority | AI-owned repository research and documentation |
| Execution issue | [Issue 91](https://github.com/pcesar22/domes/issues/91); [PR 92](https://github.com/pcesar22/domes/pull/92) is the one review package |
| Stop condition | No protocol/runtime change, part or architecture freeze, spend, market validation, or product/compliance claim |

Acceptance requires the repository to distinguish evidence, hypothesis, and decision; name the buyer,
user, job, kit, environment, economic bounds, and unresolved discovery questions; and define one
canonical workflow covering setup, roster, drill configuration, play, partial failure, recovery,
results, shutdown, and charging/storage. Conflicts with current architecture and program authority
must be reconciled or recorded explicitly. Documentation and repository checks must pass.

At selection, PR 90 was merged, its `main` CI completed green, and no open execution issue, PR, or
plan existed, so no integrity repair or resumable work outranked new work. PS-WP-001 was selected
because PS0 was active with the earliest forecast and its output unlocks named work in every G1
workstream. PS1 depends on
that product boundary; FS1/HW0 acceptance depends on physical observation and instrumentation; and
VC1 has a later forecast and consumes the workflow and requirements. The higher program dependency,
HW owner and budget, remains a separate CEO decision and does not stall this AI-owned package.

The review package establishes the evidence-labeled brief, a dated USD 349-439 planning hypothesis,
one canonical workflow, an explicit pause/preserve/disclose recovery policy, current app/firmware
gaps, and downstream requirement seeds. It does not complete customer, purchase, cost, licensing,
support, six-node, or product-hardware evidence.

#### PS-WP-002: Hardware-Driving Requirements And Interface Baseline

**Objective:** Convert the PS-WP-001 workflow and current/target architecture into one traceable G1
requirements and interface candidate without inventing evidence or freezing the solution.

| Contract | Current state |
| --- | --- |
| Owner | AI systems lead |
| State / health | `Review ready` / `Green` |
| Actual start / review boundary | 2026-08-04 / 2026-08-04 |
| Inputs | Product brief/workflow, target system/ID architecture, as-built software, HW-WP-001 inputs, G1 evidence contract |
| Dependencies/blockers | None for a bounded candidate; customer, HW-owner, measurement, compliance, and cost evidence remain explicit open inputs |
| Gate/risk unlocked | Supplies the product/system allocation and interface input required by HW1, FS3, VC1, HR1/HR2, and G1 |
| Execution authority | AI-owned repository research, requirements, interface, and traceability documentation |
| Execution issue | [Issue 93](https://github.com/pcesar22/domes/issues/93); stacked [PR 94](https://github.com/pcesar22/domes/pull/94) is the one review package |
| Stop condition | No runtime/protocol change, architecture/part freeze, spend, schematic/layout release, or product/compliance claim |

Acceptance requires stable candidate IDs with source/rationale, measurable value or explicit bounded
TBD/fallback, allocation across product/hardware/firmware/app/mechanical/test, verification method,
owner, status, and invalidation rule. It must include a current/target interface-control candidate and
make every unresolved G1 input visible. The candidate is written in
[`research/G1_REQUIREMENTS_AND_INTERFACES.md`](research/G1_REQUIREMENTS_AND_INTERFACES.md). It contains
43 candidate requirements, 18 interface boundaries, and 17 bounded closure packages. Two independent
semantic reviews closed all findings, and content commit `011b949` passed all seven Software CI checks
in [run 30892936374](https://github.com/pcesar22/domes/actions/runs/30892936374).

The next bounded AI package is VC-WP-001 because it converts these stable IDs into the missing G1
verification, FMEA, compliance, qualification, and evidence-ownership plan. It is ahead of FS2/FS3
on the G1 critical path and can expose blocked physical/owner inputs without fabricating them. HW1
still requires the CEO to name a qualified owner, while FS1/HW0/HR0 requires lab evidence. Reserving
VC-WP-001 does not start it or change any hardware, compliance, spend, or gate authorization.

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
| G2 | EVT Release to Fab | Critical six-node alpha paths; HR3-HR4; released BOM/AVL/build/test package; firmware EVT profile; closed critical design risks | 2026-11-02 | `Planned` | Order approximately 10-20 traceable EVT units |
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

## Workstream Status

### Product And System (`PS`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| PS0 | Product brief and launch hypothesis | CEO/product owner with AI product lead | `Active` | `Amber` | 2026-08-14 | Hypothesis baseline review-ready; execute customer, competitive, economic, licensing and support discovery |
| PS1 | Hardware-driving product/system baseline and traceability | AI systems lead | `Active` | `Amber` | 2026-09-07 | Candidate review-ready in PR 94; close its bounded G1 evidence inputs through PS/HW/FS/VC work |
| PS2 | App-driven six-node system alpha | AI systems lead | `Ready` | `Amber` | 2026-10-19 | Acquire four nodes; unify drill, authority, timing, failure, result and coexistence requirements |
| PS3 | DVT user/product validation | Product/UX owner, unassigned | `Not due` | `Not rated` | 2027-08-02 | Starts on representative frozen units; continues customer/economic validation before it |
| PS4 | Launch offer, price, channel, support and warranty | CEO/product owner | `Not due` | `Not rated` | 2027-12-13 | Close from customer, DVT, PVT, cost and support evidence |

### Firmware, Software, CLI, App, And Simulation (`FS`)

| ID | Outcome | Owner | Status | Health | Forecast | Now / next |
| --- | --- | --- | --- | --- | --- | --- |
| FS0 | Reproducible CI and automated two-board platform | AI firmware/software lead | `Complete` | `Green` | 2026-08-03 actual | Preserve required CI and rerun hardware evidence after behavioral change |
| FS1 | Complete physical NFF reference and product-interface inventory | AI firmware/software lead | `Active` | `Amber` | 2026-08-24 | Finish audio/volume; observe peripherals; capture current/power/timing; confirm exact parts |
| FS2 | Deterministic predictive Linux model | AI simulation lead | `Ready` | `Amber` | 2026-10-19 | Inject virtual dependencies and exact replay now; calibrate and validate on held-out NFF data before G2 |
| FS3 | One production-owned drill/runtime contract across firmware, simulator and app | AI firmware/software lead | `Ready` | `Red` | 2026-10-19 | Replace fixed two-pod, separate `SimMessage`, and host-wall-clock scoring divergence with protobuf-owned semantics and correlated timing |
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
| VC2 | Six-node alpha verification and simulation comparison | AI verification/simulation leads | `Ready` | `Amber` | 2026-10-19 | Define clock correlation, fault campaign, soak and acceptance bounds |
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
| Hardware-driving product/system requirements and interface allocation | `Candidate drafted`; acceptance evidence open | Stable values or explicit ranges/fallbacks across product, HW, FW, mechanical and test |
| NFF physical, exact-population, current, power, timing and RF record | `Unverified` | Measured reference and known limitations |
| Architecture block, interface-control record and mechanical/ID envelope | `Interface candidate drafted`; architecture and controlled ME/ID envelope open | Every rail, bus, pin, timing, boot, debug, test, RF and physical interface has an owner |
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
| 2026-08-04 | HW-WP-001 released; deterministic-interface work, VC planning and four-node sourcing start | Parallel definition begins without PCB commitment |
| 2026-08-14 | Product brief hypothesis review baseline completed 2026-08-04; hardware-driving requirement draft, current/target interface inventory and initial risk register remain | Conflicts and missing measurements surface early |
| 2026-08-24 | NFF physical/peripheral/electrical characterization baseline | Guesses are replaced before selection freeze |
| 2026-08-31 | Architecture/component shortlist and bounded risk-coupon review | Weak candidates are removed |
| 2026-09-07 | Requirements/interface baseline candidate, budgets, RF/compliance and manufacturing/test concepts | G1 package becomes auditable |
| 2026-09-15 | G1 System Architecture Baseline | Schematic capture may start only on `Go`; PCB routing still requires HR3 |
| 2026-10-19 | Critical six-node alpha paths and unified drill/timing contract pass | Product behavior informs final EVT release |
| 2026-11-02 | G2 EVT Release To Fab | First product-intent board order may be placed |
| 2026-12-14 | Traceable EVT units available | Product-board bring-up starts |
| 2027-02-08 | G3 EVT Exit | Corrected frozen DVT design may proceed |
| 2027-08-02 | G4 DVT Exit | Intended-line pilot may proceed |
| 2027-11-01 | G5 PVT Exit | Immutable release candidate may proceed |
| 2027-12-13 | G6 Open Product Release | Product may ship and enter sustainment |

## Evidence Register

### Accepted Software And Automated NFF Baseline

| Evidence | Source | Result | Boundary |
| --- | --- | --- | --- |
| Current main software CI | Commit `0378730fbe4773453a0f276a62aed0a7549b672d`, [run 30889560443](https://github.com/pcesar22/domes/actions/runs/30889560443) | Passed | Builds, tests, generated artifacts, lint, docs, Flutter Linux/iOS and ESP-IDF release checks |
| PS-WP-001 product baseline | Commit `628a5250d28aa9581001c0e164e596887067f858`, [PR 92](https://github.com/pcesar22/domes/pull/92), [run 30890512299](https://github.com/pcesar22/domes/actions/runs/30890512299) | Review-ready content commit passed all seven Software CI checks | Product hypothesis and workflow authority; customer, economics, licensing, product implementation, and hardware proof remain open |
| PS-WP-002 requirements/interface candidate | Commit `011b949c55964aaf1821ad7db737c11d26300673`, [PR 94](https://github.com/pcesar22/domes/pull/94), [run 30892936374](https://github.com/pcesar22/domes/actions/runs/30892936374) | Review-ready content commit passed all seven Software CI checks | Candidate requirements, interfaces, traceability, conflicts, and closure inputs; G1 acceptance and all named open evidence remain open |
| Repository effectiveness acceptance | [PR 85](https://github.com/pcesar22/domes/pull/85), merged 2026-08-03 | Accepted | Instructions, verification orchestration, pinned toolchains and CI behavior |
| Automated hardware CI | Commit `76d312af1710a14102beeeeaeab716a02a0a4e70`, [run 30785241480](https://github.com/pcesar22/domes/actions/runs/30785241480) | Passed | Two NFF boards, serial/BLE/ESP-NOW/OTA/diagnostics/trace; no physical observation |

The accepted software baseline passed 271 host firmware tests, 100 Rust CLI tests, 161 Flutter
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
| Only two NFF boards exist | Six-node behavior cannot inform EVT release | PS/FS | Procure four inexpensive alpha nodes by 2026-08-07 |
| Candidate power/charging/RGBW/battery topology is unproved | Unsafe or underpowered architecture could be carried into EVT | HW/VC | Challenge by analysis/coupons; do not inherit proposal parts by default |
| Launch market/economics are unvalidated and license/support are unset | Hardware or release scope may miss mandatory constraints | PS/VC | Test PS-WP-001 guardrails, bound at G1, and close before DVT |

## Reporting Contract

Every status report answers, in order:

1. Active program phase, current development hardware, NPI stage, revision/date and overall health.
2. Next program gate, baseline/forecast/confidence, authorization, open critical evidence and
   recommended disposition.
3. Current execution package, next AI-owned action, acceptance boundary, blocker and execution issue.
4. Immediate hardware authorization: definition, schematic/layout, EVT, DVT or PVT; never say only
   “hardware can start.”
5. `PS`, `FS`, `HW`, and `VC` workstream outcome, delivered/now/next, health and forecast.
6. Hardware release-ladder position and the exact next evidence release.
7. Critical path and top risks with owner, mitigation and decision-by date.
8. CEO decisions required, team recommendation, alternatives and consequence of delay.
9. Changes to scope, schedule, cost, requirements, configuration, evidence or risk since last review.

No percentage rollup is permitted. Gate readiness, hardware release state, evidence and dated
workstream outcomes are the program truth.
