# DOMES Integrated Program Status

This is the current product-delivery summary. The
[product realization framework](docs/PRODUCT_REALIZATION_FRAMEWORK.md) defines phases,
decision authority and hardware releases. The
[program milestone map](docs/PROGRAM_MILESTONES.md) defines the delivery packages and
their dependencies. Issues track remaining work; builds, tests and accepted evidence
establish outcomes.

**Evidence review:** 2026-09-05. Historical results remain bound to their source
revision and test configuration. A merged PR or closed issue does not establish an
unexecuted or independently unaccepted verification result.

## Current position

| Control | State |
| --- | --- |
| Active phase | P1 Definition and Feasibility |
| Overall health | Amber |
| Development hardware | Two NFF ESP32-S3 N8R8 development carriers; no product prototype |
| NPI stage | Pre-EVT; no released product schematic, layout or manufacturing package |
| Next decision | G1 System Architecture Baseline and Schematic Authorization: **Hold** |
| G1 schedule | Original baseline 2026-09-15; current forecast unconfirmed, confidence Low |
| Latest bounded software completion | NFF-MEM-001 audio-memory repair, merged in [PR 200](https://github.com/pcesar22/domes/pull/200); physical installation and recovery remain open |
| Next review-ready product delivery | FS-WP-004A virtual pod lab, [PR 201](https://github.com/pcesar22/domes/pull/201); source review and required CI passed, human merge remains |
| Next app delivery | [FS-WP-004B complete simulated phone journey](https://github.com/pcesar22/domes/issues/204) after accepted A integration |
| Parallel definition work | [PS1, VC1 and HW-WP-001A definition](https://github.com/pcesar22/domes/issues/205); [HW-WP-002 development setup](https://github.com/pcesar22/domes/issues/199) |
| Current physical gap | Present NFF readiness, update/recovery, peripheral observations and measured operating envelope |
| Next external decision | Qualified hardware owner, bounded definition budget and measurement capability |
| Design/spend boundary | Definition work may proceed; G1 has not authorized schematic work, HR3 has not authorized routing, and G2 has not authorized EVT fabrication |

The program has parallel app, NFF-reference, hardware-definition and supporting
simulation work. App-model development does not require predictive simulation or
additional physical nodes. HR0 characterization feeds hardware definition without
waiting for the phone alpha. Six physical nodes are required for physical six-node
acceptance, not for a six-identity software test.

## Current delivery evidence

| Package | Current result | Remaining acceptance or limitation |
| --- | --- | --- |
| APP0 app foundation | Existing transport, generated protocol and provider implementation with software tests | Physical phone/device behavior is separate. |
| FS-WP-004A virtual pod lab | [PR 201](https://github.com/pcesar22/domes/pull/201), reviewed revision `0c44908bf23dc79069f3df3002e63fe10ba4ff47`: 228 app tests, Linux release build, independent implementation approval and all eight required [software CI checks](https://github.com/pcesar22/domes/actions/runs/33982094201), including iOS build, passed | Awaiting human merge. Two/six virtual identities, deterministic virtual-only behavior and mixed-target isolation are app-model evidence, not production parity or physical BLE/touch evidence. |
| FS-WP-004B complete simulated phone journey | Not implemented as a complete accepted journey | Starts after A; cover discovery, setup, drill/results, reconnect, inactive touch, timeout, loss/duplicates, restart and update-failure fixtures. |
| NFF-MEM-001 runtime-memory repair | [PR 200](https://github.com/pcesar22/domes/pull/200) merged; persistent audio allocation reduced from 32,000 to 512 bytes, with regression tests and a fresh pinned firmware build | Software saves 31,488 bytes plus allocation overhead. It has not established physical recovery. |
| NFF1 current readiness and recovery | Historical communication/boot evidence exists; latest retained readiness failed the internal-heap floor | Retained free heap was 23,331 bytes against a 30,720-byte floor. Measure the repaired image on both boards, verify current self-tests and two normal boots, and reconcile supported updates. Keep forced-failure rollback separate. [Issue 106](https://github.com/pcesar22/domes/issues/106). |
| FS-WP-003A shared drill/timing contract | Generated peer/drill codec and compatibility work are implemented, but full convergence remains open | Current app reaction measurement and exported results still need shared token, monotonic-time, precision and provenance semantics; retain differential acceptance and separate physical regression. [Issues 154](https://github.com/pcesar22/domes/issues/154) and [155](https://github.com/pcesar22/domes/issues/155). |
| FS-WP-002A/B/D simulation foundation | Historical replay, feasibility and composition packages accepted through [PR 97](https://github.com/pcesar22/domes/pull/97) and [PR 100](https://github.com/pcesar22/domes/pull/100) | Their declared target/runtime profiles do not establish predictive, RF, peripheral or cycle-accurate equivalence. |
| FS-WP-002C scheduler trace | Implementation merged, including [PR 105](https://github.com/pcesar22/domes/pull/105) | Final-head physical exit remains acceptance pending; earlier candidate evidence cannot substitute for deferred or failed final verification. [Issue 101](https://github.com/pcesar22/domes/issues/101). |
| FS-WP-002E production radio seam | Implementation merged through [PR 151](https://github.com/pcesar22/domes/pull/151); [source-bound judgment](https://github.com/pcesar22/domes/issues/123#issuecomment-5380661241) accepted software and two-board radio/trace evidence at `c39f1f9` | Historical scoped acceptance does not establish current NFF readiness or close C's distinct physical criterion. |
| FS-WP-002F DUT and peer backplane | Implementation merged in [PR 189](https://github.com/pcesar22/domes/pull/189) | Independent runtime acceptance remains open: the reported campaign could not be accepted from the available retained evidence. Reproduce/review the full fault, replay, role and build/CI evidence before treating F as a satisfied prerequisite. [Issue 143](https://github.com/pcesar22/domes/issues/143). |
| FS-WP-002G/H and VC-WP-002A | Final concurrency qualification, calibrated candidate and independent held-out verdict remain incomplete | G needs accepted F; H also needs measured HR0 and stable FS3; independent qualification requires the accepted terminal G/H artifacts. An input-rejection tool is not an executed campaign. |
| FS-WP-004C/004D and VC-WP-002B | Production parity and physical six-node alpha evidence remain incomplete | Preserve shared-contract, CLI diagnostic, integrated software and physical acceptance as separate results. [Issue 193](https://github.com/pcesar22/domes/issues/193) tracks the terminal software gap. |

All current-revision PR checks must pass before merge. Historical counts and reports
are useful evidence of a bounded result, not a standing pass for later changes.

## Workstreams and next actions

| Workstream | Current outcome | Next inspectable result | Boundary |
| --- | --- | --- | --- |
| Product/system | Product hypotheses exist; hardware-driving requirement allocation remains open | PS1 requirement/interface baseline with limits, owners, assumptions and verification methods | Do not freeze architecture-changing assumptions as facts. |
| App/software | Virtual lab A is review-ready; shared production semantics and complete journeys remain open | Integrate A, then deliver B independently of predictive simulation; converge FS3 and later parity separately | App-model, production-software and physical verdicts remain distinct. |
| NFF reference | Automated foundation and software memory repair exist; current physical readiness is unverified | LAB0 safe bench, NFF1 recovery, then NFF2 observations and NFF3 measurement | HR0 requires observations and measurement, not phone-alpha completion. |
| Hardware/NPI | Definition is permitted; no released product design | HW-WP-002 setup capability definition and HW-WP-001A desk trades, then measured HR1/HR2 | Owner, budget, instruments and selection-critical measurements remain explicit inputs. |
| Verification/compliance | Software CI and existing evidence tooling exist; VC1 and later phase evidence remain incomplete | VC1 requirement/test/risk matrix and accountable compliance route | A technical result does not authorize spending or claim certification. |
| Simulation | A/B/D foundations and E scoped evidence exist; C/F acceptance gaps remain | Close the named evidence gaps before G/H/held-out qualification | No prediction claim without independent qualification. |

## Program phases and baseline

| Phase | Current state | Exit decision | Original baseline |
| --- | --- | --- | --- |
| P0 Development Foundation | Closed | G0 Development Foundation | 2026-08-03 actual |
| P1 Definition and Feasibility | Active | G1 System Architecture Baseline | 2026-09-15 |
| P2 Integrated Alpha and EVT Design | Not entered | G2 EVT Release to Fab | 2026-11-02 |
| P3 EVT Build and Qualification | Not entered | G3 EVT Exit / DVT Authorization | 2027-02-08 |
| P4 DVT Product Validation | Not entered | G4 DVT Exit / PVT Authorization | 2027-08-02 |
| P5 PVT and Launch Readiness | Not entered | G5 PVT Exit / Release Candidate | 2027-11-01 |
| P6 Open Product Release | Not entered | G6 Product Release | 2027-12-13 |
| P7 Sustainment | Not entered | G7 Sustainment Handoff | Set after launch |

These are retained baselines. Near-term milestones already missed have not been
silently moved. Reforecast G1 and dependent dates after owner/capacity, physical
characterization and requirement inputs are resolved. Future phases are not failed
merely because they have not been entered.

## G1 evidence and decisions

| Required input | Current disposition | Closure result |
| --- | --- | --- |
| Qualified hardware owner, capacity and bounded budget | Open | Named accountable owner and authorized definition scope |
| PS1 product/system requirements and interfaces | Open | Measurable limits or explicit bounded fallbacks |
| HR0 measured NFF reference | Unverified | Actual population, peripheral observations and operating envelope |
| HR1 architecture downselect | Not accepted | Interfaces, mechanical envelope, topology and measured resource budgets |
| HR2 component baseline | Not accepted | Exact parts/alternates, sample evidence, lifecycle/supply and budget closure |
| VC1 verification, FMEA and compliance route | Open | Critical requirements mapped to methods, owners and acceptance criteria |
| Firmware profile, manufacturing/test/service concept | Open | Supportable components, recoverable/programmable/testable design and traceability plan |

G1 remains **Hold**. It may change only on direct evidence under the framework's
criteria. Schematic authorization is distinct from HR3 routing authorization and
G2 fabrication authorization. G2 requires the released design/build package,
product firmware profile, direct six-node alpha evidence and accepted product
workflow, plus separate spend authorization. Predictive simulation is optional
when direct physical evidence closes the relevant risks; unexplained divergence
remains a critical risk.

## Principal risks

| Risk | Consequence | Owner / next resolution |
| --- | --- | --- |
| Current NFF readiness and physical envelope unverified | Hardware choices would rely on assumptions | Firmware/verification and lab operator: close NFF1-NFF3 and HR0 |
| Hardware owner, budget and measurement capability open | G1 date lacks a credible resource basis | CEO and qualified hardware owner: decide bounded definition resources |
| Shared timing/token/result semantics incomplete | App, simulator and firmware can disagree | Software lead: FS-WP-003A implementation and differential acceptance |
| Merged source mistaken for accepted evidence | Later packages consume unqualified inputs | Verification owner: resolve C/F acceptance and preserve artifact lineage |
| Only two physical nodes available | Six-node physical behavior cannot be accepted | Hardware/system owner: LAB6 proposal and separately authorized capacity |
| Critical product, RF/power, compliance or supply assumptions unresolved | Design freeze could encode the wrong constraints | Product/HW/VC owners: PS1, VC1 and measured HR1/HR2 decisions |

Progress is reported as results, evidence gaps and decisions. There is no aggregate
completion percentage. Detailed milestone acceptance and dependencies live in the
[program milestone map](docs/PROGRAM_MILESTONES.md).
