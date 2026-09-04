# DOMES Integrated Program Status

**As of:** 2026-09-04. Repository evidence review; no new hardware campaign was run.

<!-- domes-control: {"phase":"P1","nextGate":"G1","verdict":"Hold","hardwareCount":2} -->

DOMES is in **P1 Definition and Feasibility**, on **two NFF ESP32-S3 N8R8 development carriers**,
at **Pre-EVT**, with **Amber** health. The next decision is **G1 System Architecture Baseline and
Schematic Authorization**. Its technical disposition is **Hold**: critical inputs remain open.
The baseline is **2026-09-15**; the current forecast is **unset / Low confidence**. The elapsed
Aug 24 characterization and Aug 31 downselect dates are missed planning targets, not completions.

This review supersedes the old executive panel and stale package pointers. The
[dated historical ledger](docs/program/archive/status-2026-08-15.md) preserves earlier evidence,
hashes, board identities and forecasts; it is not current authority. The
[review record](docs/program/review-2026-09-04.md) describes conflicts and claim limits.

## How to read and operate the program

- [Private development dashboard](https://domes-product-status.pcesar22.chatgpt.site):
  three concurrent delivery tracks, dependency graph, selected-milestone evidence and human steer.
- [Detailed milestone ledger](docs/program/milestones.json): machine-readable work-package states,
  dependencies, acceptance, owner, evidence source and invalidation rules. It projects this authority;
  conflicts must stop publishing.
- [Product realization framework](docs/PRODUCT_REALIZATION_FRAMEWORK.md): P0–P7, G0–G7,
  PS/FS/HW/VC outcomes and HR0–HR7 authorization. The three dashboard tracks are delivery views,
  not replacement program phases.
- [Testing contract](docs/TESTING.md): reproducible software and hardware checks.
- [Next hardware request](hardware/NEXT_ITERATION_REQUEST.md) and
  [development setup definition](hardware/DEVELOPMENT_SETUP.md): HR0–HR2 inputs and resource needs.

### Operating choices

Keep all three tracks open. App and host-simulation programming do not wait for NFF closure or a
fully predictive QEMU model. Hardware desk definition does not wait for a finished app.
One registered two-board lab session at a time prevents conflicting physical campaigns.
Preparation can run concurrently; evidence-producing exits follow their actual dependencies.

**Current management package:** this product-control reset. No device execution is active in this
review. **Next autonomous execution delivery: FS-WP-004A**, the protocol-backed virtual pod lab,
subject to the normal live issue/PR reconciliation before execution. Start at the real app
repository/transport boundary, retain generated protocol ownership, and use virtual time. It must
leave executable app scenarios and regression evidence, not only interfaces or another plan.

**Next program action: HW-WP-002**, a bounded development-setup definition under HW-WP-001, alongside
PS1 requirements and VC1 risk/test work. Desk definition is ready. Procurement, supplier commitments,
and controlled hardware design ownership require separately recorded human authority.

## Concurrent workstreams

| Delivery track | Delivered | Now | Next | Owner / forecast |
| --- | --- | --- | --- | --- |
| Phone app and simulation (FS/VC) | BLE controller/drill prototype, host replay and target runtime foundation; historical software tests | Protocol-backed virtual pod lab; reconcile shared drill and trace acceptance | Complete offline simulated journeys, production-backplane parity, then six-node physical alpha | AI app/software lead; forecast after first executable slice, not a fabricated date |
| Dual-NFF validation (FS1/HW0/VC) | Historical automated serial/BLE/ESP-NOW/OTA/recovery/trace and soak campaigns | Current identity, as-built/safe-power and recovery baseline | Observed peripherals, operating-envelope measurements, phone/two-pod fault campaign, HR0 release | AI verification lead + lab operator; lab slot and instruments unconfirmed |
| Next setup and hardware (HW/PS/VC) | NFF design sources and candidate product architecture; no product-board release | HW-WP-002 setup spec and parallel requirements/trades | HR1/HR2 package, G1, controlled HR3/HR4, G2, then EVT | AI systems lead; qualified HW design owner unassigned; forecast unset |

PS1 and VC1 remain parallel inputs to G1. FS4/VC2 require **six physical development nodes** for
system-scale evidence. Two NFFs can substantially derisk integration but cannot satisfy that exit.
The only supported physical firmware profile is the NFF 8 MB profile. A planned 16 MB product
profile, battery/charging circuit, enclosure and RGBW design remain targets.

## Current milestone ledger

States below distinguish historical completion, current executable readiness, incomplete acceptance,
and future sequencing. A historical pass is limited to its recorded configuration.

| ID | Outcome | State |
| --- | --- | --- |
| APP0 | Controller foundation | Complete |
| FS-WP-004A | Virtual pod lab | Ready |
| FS-WP-004B | Complete simulated phone journey | Not due |
| FS-WP-003A | Shared drill and timing contract | Acceptance pending |
| FS-WP-004C | App ↔ production simulator parity | Not due |
| FS-WP-004D | Six-node physical app alpha | Not due |
| NFF0 | Automated bench foundation | Complete |
| NFF1 | Current identity & recovery baseline | Not due |
| NFF2 | Observe every peripheral | Not due |
| NFF3 | Measure the operating envelope | Not due |
| NFF4 | Phone + two-pod fault campaign | Not due |
| HR0 | HR0 · Measured NFF reference | Not due |
| HW-WP-002 | Specify the next development setup | Ready |
| HW-WP-001A | Hardware requirements & desk trades | Ready |
| HR1 | HR1 · Architecture downselect | Not due |
| HR3 | HR3 · Controlled schematic release | Not due |
| HR4 | HR4 · PCB & EVT build package | Not due |
| HR5 | HR5 · EVT exit | Not due |
| PS1 | Product and interface baseline | Ready |
| VC1 | Verification and risk plan | Ready |
| LAB6 | Six physical development nodes | Blocked |
| FS-WP-002A | Deterministic host replay | Complete |
| FS-WP-002B | QEMU feasibility | Complete |
| FS-WP-002D | Production runtime composition | Complete |
| FS-WP-002C | Scheduler trace acceptance | Acceptance pending |
| FS-WP-002E | Production radio seam | Not due |
| FS-WP-002F | One DUT + virtual peer backplane | Not due |
| FS-WP-002G | Concurrency and fault qualification | Not due |
| FS-WP-002H | Calibrated prediction candidate | Not due |
| VC-WP-002A | Independent held-out qualification | Not due |
| HR2 | HR2 · Component baseline | Not due |
| LAB0 | Confirm the safe two-NFF bench | Ready |
| LAB1 | Commission measurement capability | Not due |
| FS-WP-005A | Build the EVT firmware profile | Not due |
| VC-WP-002B | Six-node alpha evidence release | Not due |
| PS2 | Accept the alpha product workflow | Not due |

The simulation dependency remains A → B → D → C → E, with FS-WP-003A parallel; then
(E + FS-WP-003A) → F → G; (G + measured FS1/HR0 + stable FS3) → H → independent VC-WP-002A.
C and FS-WP-003A are acceptance pending. E/F have prototype artifacts but remain Not due until their entry dependencies pass; implementation is disclosed separately from acceptance.
The operational qualification entry report explicitly rejects predictive entry because terminal
G/H evidence and controller attestation are absent. No predictive claim is authorized.

## Sequential dual-NFF milestone exits

1. **LAB0, then NFF1 — current baseline:** lock both identities and exact candidate/profile; inspect actual
   populated parts, orientation, safe rails, idle current and heating before powered load tests.
   Retain diagnostics, all currently discovered self-tests (record the count), two normal boots and separate supported update/recovery evidence.
   Forced rollback is a separate explicitly authorized destructive test, not inferred from OTA success.
2. **NFF2 — physical peripherals:** observe all LEDs, four touch pads, IMU motion/tap, LRA and
   speaker/audio-volume behavior on each board; pair stimuli with device and command identities.
3. **NFF3 — measured envelope:** after NFF1 and commissioned LAB1 instruments, this can run
   alongside NFF2 under one exclusive lab schedule; current/transients at idle, radio, LED, audio, haptic and combined
   load; rail/thermal/resource margin, RF/coexistence, correlated physical timing and uncertainty.
   Instrument and firmware versions belong in the evidence record.
4. **NFF4 — integrated two-pod/phone faults:** eligible after NFF2, independent of full NFF3 closure; fresh exact disabled radio lifecycle, complementary
   roles and one peer each; simulation-off benchmarks in both directions, three fresh lifecycles,
   then a separate traced drill. Real phone scan/connect, active/inactive physical touch,
   stop/timeout/disconnect/reconnect, updates and soak/fault recovery must retain results.
5. **HR0 — NFF reference release:** depends on NFF2 and NFF3, without waiting for NFF4 phone
   integration; bind raw data, exact boards and artifacts, procedures, calibration, result,
   uncertainty, failed criteria and design consequences. Release only the measured NFF envelope.

Current position is **between NFF0 historical automated foundation and LAB0/NFF1 current requalification**.
LAB0 is Ready to verify access; NFF1 is Not due until that prerequisite passes.
FS1/HW0/HR0 remain active at program level; no new physical execution is claimed today.

## G1 evidence and hardware authorization

| Criterion | State |
| --- | --- |
| Qualified HW owner, capacity and bounded budget | Open |
| Hardware-driving requirements and interfaces | Not run |
| Measured NFF reference and exact populated parts | Unverified |
| Architecture, mechanical envelope and interface record | Not run |
| Selected parts, alternates and preliminary BOM | Not run |
| Power, thermal, runtime and memory budgets | Not run |
| FMEA, RF/compliance route and risk coupons | Not run |
| Firmware board profile and service interface plan | Not run |
| Manufacturing, test and traceability concept | Not run |

**Technical verdict: Hold.** There is no immutable accepted HR0–HR2 release package. This review
does not convert the open criteria into failed physical tests; they are open/not run/unverified.
No conditional go is appropriate for topology, parts, interface, power, safety or resource unknowns.

Definition and analysis may proceed. G1 may authorize controlled schematic capture after a passing
package. HR3 separately authorizes PCB routing. HR4 plus G2 and the CEO's separate budget authority
permit one EVT order. EVT/DVT/PVT/production, certification, battery safety and shipment claims
remain unauthorized. Technical readiness cannot manufacture spend approval.

### G2 cross-functional exit

G2 requires separately released HR3/HR4, an exact EVT firmware profile (FS-WP-005A), direct
six-node timing/fault/soak/recovery evidence (VC-WP-002B), accepted PS2 product behavior, a controlled
manufacturing/test/traceability package, and no unresolved critical design or simulator/hardware
divergence. An immutable technical package and separately recorded CEO spend authority are required.
The six-node physical app campaign does not depend on a predictive simulation pass: direct physical
evidence can close those risks. App-to-production-simulator parity remains a separate valuable exit.

## Human steer and critical path

| Decision | Recommendation | Timing / consequence |
| --- | --- | --- |
| Qualified hardware design owner and bounded definition/instrumentation budget | Name the accountable owner; approve only a reviewed scoped budget | Needed before paid work or controlled design responsibility; G1 forecast remains unset |
| Initial product envelope | Confirm or revise the six-pod indoor offline-first kit hypothesis, launch phone platforms and first market | Needed before requirements and hardware freeze; PS1 remains open |
| Expanded lab capacity | Review HW-WP-002's costed retain/borrow/buy proposal before acquiring four extra nodes or instruments | Six-node physical alpha needs real inventory; simulation cannot substitute |

Critical path: **product envelope + HW owner/capacity + NFF measurements + verification/risk plan
→ HR1/HR2 → G1**. App simulation is a parallel productivity investment, not a G1 gating department.
After G1, HR3/HR4, product profile and independently passing six-node alpha evidence feed G2.
Original later gate dates remain historical baselines; current forecasts await resource and evidence
reconciliation. A predictive model is optional for G2 only where direct physical evidence closes
the same critical risks; unexplained model/hardware divergence remains a design risk.

## Evidence, conflicts and refresh policy

- Local reviewed baseline: 0f1659c6a32288fa3478969586e54a81599c4453. Its Software CI
  [run 32621664440](https://github.com/pcesar22/domes/actions/runs/32621664440) passed Aug 23.
- Remote main's Aug 29 software repair passed
  [run 33274700318](https://github.com/pcesar22/domes/actions/runs/33274700318) on 3b62a6c.
  This checkout has not imported that code. Its activity is advisory, not current local evidence.
- PR 105 is merged, contradicting the old pending-merge pointer. Its candidate lineage is not
  consistent between the local plan, PR description and reported final head. C stays acceptance
  pending until the exact-artifact chain is audited.
- E's plan explicitly defers physical radio regression. FS3's compatibility plan explicitly leaves
  two-board physical exit unverified. Issue-142 replay artifacts show candidate F work; they do not
  satisfy the whole F/G/H/VC ladder by proximity.
- Product-definition review remains dated Aug 3 and its requirements are still hypotheses.
  Re-reading it today does not refresh its substantive evidence age.
- The new graph and executive source must change together. Deterministic refresh validates the
  reviewed source receipt and rejects missing files, hash drift, cycles, invalid dependencies,
  unsupported completion, source/ledger conflicts and gate crossings. It never edits authoritative
  states. A new review must state what changed before a receipt is accepted.
- Scheduled refresh checks every two hours while its host is available. No reviewed change means
  no source rewrite or deployment. On failure the last validated private publication stays live,
  reports expose the exact gap, and the page displays its actual review age. GitHub is never an
  automatic gate or milestone authority.

## Invalidation and reporting

A source, protocol, firmware, board, instrument, configuration, dataset or acceptance-rule change
reopens affected evidence. Calibration and held-out validation stay separate. Every package retains
owner, measurable exit, source identity, resource limit and stop condition in the detailed ledger.
Report current work, recorded work, gaps, next dependencies and human decisions; never percentages.
