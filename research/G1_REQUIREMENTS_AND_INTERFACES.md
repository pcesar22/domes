# G1 Product/System Requirements And Interface Candidate

This document is the controlled candidate input from PS1 to HW1, FS3, VC1, HR1/HR2, and G1. It
translates the product brief into traceable engineering constraints without treating a target,
calculation, accepted command, or NFF result as product evidence.

| Control | Value |
| --- | --- |
| Baseline | `PS-WP-002` candidate 0.1 |
| State | Candidate; not accepted and not a G1 pass |
| Configuration | Six-pod launch hypothesis and current two-board NFF development baseline |
| Owner | AI systems lead |
| Required review inputs | Product, firmware/software, hardware, ME/ID, verification, compliance, manufacturing/service |
| Decision boundary | G1 System Architecture Baseline and Schematic Authorization |
| Last reviewed | 2026-08-04 |
| Does not authorize | Architecture/part freeze, schematic/layout release, spend, fabrication, compliance/product claim, or protocol/runtime change |

The product and workflow authority remains [`PRODUCT_DEFINITION.md`](PRODUCT_DEFINITION.md). Current
software and wire behavior remain owned by implementation and
[`SOFTWARE_ARCHITECTURE.md`](SOFTWARE_ARCHITECTURE.md). Target architecture and ID documents remain
inputs, not evidence. Verification procedures remain in [`../docs/TESTING.md`](../docs/TESTING.md),
and program/gate status remains in [`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md).

## Control Rules

### Meaning Of A Row

| Field | Rule |
| --- | --- |
| Class | `Working decision`, `Hypothesis`, `Target`, or `Current constraint`; none means verified product behavior |
| State | `Candidate` needs acceptance evidence; `Open evidence` also has an unresolved value; `Current constraint` describes the NFF/as-built boundary |
| Owner / allocations | The first allocation is the accountable requirement owner; following allocations are contributing disciplines |
| Verify | Planned method: test, analysis, inspection, demonstration, or a combination |
| Closure | A `CL-*` entry names evidence, owner, decision date, fallback, and invalidation rule |

Every requirement is singular at the system level even when several disciplines contribute. An
allocation does not change the stable ID. A changed value, environment, topology assumption, or
verification method must reopen the requirement and every dependent interface and result.

Owner/allocation tokens name accountable leads as follows; an `unassigned` lead is an open G1 resource, not an
excuse to transfer acceptance to another discipline.

| Allocation | Accountable lead |
| --- | --- |
| `PROD` | CEO/product owner with AI product lead |
| `SYS` | AI systems lead |
| `HW` | Qualified hardware design owner, unassigned |
| `FW` | AI firmware lead |
| `APP` | AI application/software lead |
| `ME/ID` | Mechanical/industrial-design owner, unassigned |
| `VER` | AI verification lead; lab operator for named physical observations |
| `MFG/SVC` | Manufacturing/quality/service owners, unassigned |
| `CEO` | CEO for offer, market, budget, vendor, and license commitments |

### Requirement Status Rule

This candidate intentionally contains open evidence. G1 cannot treat a row as accepted until its
closure record has direct, configuration-bound evidence or an explicit product decision, its
verification mapping is executable, and no material contradiction remains. A bounded fallback
allows definition work to continue; it does not convert missing evidence into a pass. Any fallback
that could change topology, a critical part, safety, compliance, PCB outline, placement, or a
firmware interface blocks the affected G1 commitment.

## Current Evidence Boundary

These are configuration-bound constraints, not product requirements. Live implementation and
retained artifacts outrank this summary if they change.

| Current fact | Authority / evidence | Requirement impact |
| --- | --- | --- |
| The NFF carrier plus ESP32-S3 N8R8 is the only supported firmware board profile. | `firmware/domes/main/config.hpp`; as-built software architecture | A production profile, resource allocation, boot/debug path, and exact pins remain open. |
| NFF uses 16 LEDs in RGB mode, four touch pads, LIS2DW12, DRV2605L, and MAX98357A on the active pins/addresses. | Active firmware configuration and `docs/PIN_REFERENCE.md` | Driver initialization does not select product parts, pins, optics, mechanics, or power. |
| LED refresh is 16 ms, touch polling is 10 ms, and the watchdog is 10 s. | Active firmware configuration | These implementation constants are not end-to-end latency or product-response evidence. |
| Current flash is 8 MB with two `0x1E0000` OTA slots and no factory app partition. | `firmware/domes/partitions.csv` | The proposed 16 MB product layout, factory data, security, recovery, audio, and trace budgets remain open. |
| The retained two-board campaign passed automated serial, BLE, OTA/rollback, ESP-NOW, trace, and soak paths. | `PROGRAM_STATUS.md` evidence register | It proves neither six-node scale nor physical light, touch, motion, vibration, sound, product power, enclosure, or charge behavior. |
| Observed ESP-NOW round trips were 2.6-20.8 ms without correlated one-way clocks. | `PROGRAM_STATUS.md` retained campaign | Sub-millisecond and synchronized timing targets remain unsupported. |
| Exact NFF population and physical peripheral/current/transient/RF characterization remain incomplete. | HR0 ledger and `CL-14` | NFF-dependent architecture assumptions remain provisional until serialized raw evidence closes HR0. |

## Requirement Register

### Product And Workflow

| ID | Candidate requirement | Source / rationale | Class | Owner / allocations | Verify | Environment | State | Closure |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `REQ-PROD-001` | The launch system shall operate as one kit of six interchangeable pods and one supported phone app. | Product brief launch boundary; `CW-01`-`CW-11` | Working decision | PROD, SYS, HW, FW, APP, VER | Inspection + six-node demonstration | Launch environment | Candidate | `CL-01`, `CL-08` |
| `REQ-PROD-002` | After installation, core setup, drill execution, results, export, diagnostics, and device management shall remain usable without cloud availability; installation and explicit update acquisition may use network access. | Local-first differentiator and product-brief boundary | Working decision | PROD, SYS, APP, FW, VER | Network-isolation test | Supported phone + six pods | Open evidence | `CL-01`, `CL-10` |
| `REQ-PROD-003` | The operator shall see stable pod and kit identities rather than transport addresses or mutable aliases. | `CW-02`; replacement and result traceability | Working decision | SYS, FW, APP, MFG/SVC, VER | Inspection + replacement demonstration | Factory, field, service | Open evidence | `CL-02` |
| `REQ-PROD-004` | Preflight shall identify every required pod and report identity, compatibility, health, connectivity, and energy before arming. | `CW-02`, `CW-05`; false-ready prevention | Working decision | SYS, FW, APP, VER | Normal/boundary/fault demonstration | Six-pod launch kit | Open evidence | `CL-02`, `CL-03`, `CL-06` |
| `REQ-PROD-005` | The operator shall be able to identify a selected physical pod with a bounded cue that cannot be mistaken for an active drill cue. | `CW-02`; roster setup | Target | SYS, FW, APP, ME/ID, VER | Demonstration in gym lighting/noise | Launch environment | Open evidence | `CL-03`, `CL-05` |
| `REQ-PROD-006` | Invalid drill values shall be rejected before pods change mode, and the app shall present the complete validated definition before arming. | `CW-03`, `CW-04`; predictable authority | Working decision | SYS, FW, APP, VER | Schema boundary + UI demonstration | Offline, six pods | Open evidence | `CL-09` |
| `REQ-PROD-007` | Loss of a required pod or unambiguous session authority shall pause cues, preserve completed work, disclose the failure, and require an explicit operator decision. | Accepted partial-failure policy; `CW-07`, `CW-08` | Working decision | SYS, FW, APP, VER | Inject disconnect, restart, duplicate, and stale events | Active session | Open evidence | `CL-09` |
| `REQ-PROD-008` | A result shall record drill version, exact active roster, timing provenance, interruptions, recovery decisions, missing data, and completion state. | `CW-08`, `CW-09`; no false precision | Working decision | SYS, FW, APP, VER | Schema inspection + replay/fault test | Complete and partial sessions | Candidate | `CL-04`, `CL-09` |
| `REQ-PROD-009` | The operator shall be able to store results locally and export them under user control without implying a verified participant identity when none was supplied. | `CW-09`; privacy and ownership | Working decision | PROD, APP, VER | Offline persistence/export/privacy test | Supported phone | Candidate | `CL-10` |
| `REQ-PROD-010` | Ending or aborting a session shall return every reachable pod to a defined non-cue safe state and name any pod requiring manual recovery. | `CW-08`, `CW-10` | Working decision | SYS, FW, APP, VER | Abort at every state + unreachable-pod test | Normal and fault cases | Open evidence | `CL-09` |
| `REQ-PROD-011` | Storage/charging shall accept all six pods and expose unambiguous per-pod and whole-kit readiness; topology and individual-charge fallback remain HW1 decisions. | `CW-11`; brief deliberately leaves topology open | Working decision | PROD, SYS, HW, ME/ID, FW, MFG/SVC, VER | Inspection + charge/fault/thermal tests | Accepted charge/storage environment | Open evidence | `CL-06`, `CL-07` |
| `REQ-PROD-012` | The canonical workflow shall have a measured setup/preflight distribution and an accepted time bound before product validation. | Setup speed affects buyer value; no current evidence | Hypothesis | PROD, APP, SYS, VER | Timed representative-user study | Launch kit/environment | Open evidence | `CL-01`, `CL-03` |
| `REQ-PROD-013` | The complete kit shall close inside an accepted price and fully burdened cost model including enclosure, charger/storage, assembly, test, freight, scrap, warranty, compliance, support, and open-product work. | USD 349-439 and <=50% net-price guardrails are hypotheses | Hypothesis | CEO, PROD, HW, MFG/SVC | Costed BOM + channel/support analysis | Launch offer | Open evidence | `CL-13` |
| `REQ-PROD-014` | The release shall include the accepted open-product scope, license, editable sources, source-release bill, build instructions, third-party obligations, and repair/support boundary. | Open-product promise is not yet a distribution claim | Target | CEO, PROD, FW, APP, HW, MFG/SVC, VER | Release-package inspection + clean build | Release candidate | Open evidence | `CL-12` |

### System Behavior And Lifecycle

| ID | Candidate requirement | Source / rationale | Class | Owner / allocations | Verify | Environment | State | Closure |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `REQ-SYS-001` | Pod, kit, session, drill-definition, round, and result identities shall be unique in their scope, persistent for their required lifetime, and explicit about replacement. | Product identity and provenance seeds | Working decision | SYS, FW, APP, MFG/SVC, VER | Lifecycle/replacement/collision tests | Factory through service | Open evidence | `CL-02`, `CL-09` |
| `REQ-SYS-002` | Exactly one authority shall own an active session; duplicate, stale, late, or foreign commands and inputs shall not advance it. | `CW-06`; current topology not selected | Working decision | SYS, FW, APP, VER | Fault injection + deterministic replay | Six-node active session | Candidate | `CL-09` |
| `REQ-SYS-003` | Drill definitions shall be versioned and bounded for roster, cue policy, timing, completion, cancellation, recovery, and resource use. | `CW-03`-`CW-08`; three current app labels share behavior | Working decision | SYS, FW, APP, VER | Schema/property/resource tests | Supported versions | Open evidence | `CL-09` |
| `REQ-SYS-004` | Every accepted cue and input shall be correlated to its session and round, use a declared clock domain, and carry measured uncertainty into scoring and results. | Current phone wall-clock scoring is uncorrelated | Working decision | SYS, FW, APP, VER | Clock/fault campaign + held-out comparison | Six nodes, representative RF load | Open evidence | `CL-04`, `CL-09` |
| `REQ-SYS-005` | The end-to-end interval from an accepted physical touch to local visible feedback shall be <=10 ms or the value shall be revised before architecture freeze from measured user and system evidence. | ID target; current 10 ms polling is not proof | Target | SYS, HW, FW, ME/ID, VER | Instrumented touch-to-light latency distribution | Accepted touch stack/environment | Open evidence | `CL-03`, `CL-05` |
| `REQ-SYS-006` | Cross-pod timing accuracy and allowable uncertainty shall be selected from task/user evidence and proven with correlated clocks; until then no synchronized or one-way latency claim is permitted. | Target +/-1 ms conflicts with uncorrelated 2.6-20.8 ms RTT | Target | SYS, FW, APP, VER | Correlated clock campaign + held-out analysis | Six nodes under RF coexistence | Open evidence | `CL-04`, `CL-08` |
| `REQ-SYS-007` | The system shall detect and bound disconnect, retry, rejoin, resume, reduced-roster, and abort behavior without silently changing roster or scoring. | Partial-failure decision | Working decision | SYS, FW, APP, VER | State-model + network/power fault campaign | Every active state | Candidate | `CL-09` |
| `REQ-SYS-008` | Product identity, calibration, configuration, result, update, diagnostic, and repair data shall have defined ownership, retention, migration, export, reset, and deletion behavior. | Local ownership, service, privacy | Target | SYS, FW, APP, MFG/SVC, VER | Lifecycle/security/privacy tests | Factory through end of service | Open evidence | `CL-02`, `CL-10`, `CL-12` |
| `REQ-SYS-009` | Field updates shall authenticate the release, verify integrity and compatibility, support power-loss-safe rollback/recovery, and expose final health/version state. | Product principle; current raw-image paths are partial | Target | SYS, FW, APP, MFG/SVC, VER | Success, corruption, interruption, rollback, recovery tests | Every supported transport/config | Open evidence | `CL-10` |
| `REQ-SYS-010` | Every pod shall expose bounded health, fault, energy, version, capability, and recovery information sufficient for preflight, support, and factory disposition. | `CW-02`, `CW-05`; current diagnostics incomplete for product | Target | SYS, FW, APP, MFG/SVC, VER | Fault injection + service demonstration | Factory and field | Open evidence | `CL-06`, `CL-10`, `CL-12` |
| `REQ-SYS-011` | Phone-pod and pod-pod topology shall remain replaceable behind the accepted identity, authority, timing, failure, update, and result contracts until FS3/HW1 downselect. | Prevent premature BLE-master/ESP-NOW freeze | Working decision | SYS, FW, APP, HW, VER | Interface review + alternate feasibility analysis | Six-node target | Candidate | `CL-08`, `CL-09` |
| `REQ-SYS-012` | Peak and sustained traffic, memory, CPU, radio airtime, coexistence, and fault recovery shall close for six active pods with stated margin. | Two-board automation cannot prove six-node scale | Target | SYS, FW, HW, VER | Analysis + six-node load/fault/soak test | Representative phone/RF environment | Open evidence | `CL-08`, `CL-09` |
| `REQ-SYS-013` | Power-on, idle, armed, active, charging, storage, update, fault, and service states shall define entry, exit, allowable outputs, energy behavior, and safe recovery. | Product shutdown/charging absent; HW/FW coupling | Target | SYS, HW, FW, APP, MFG/SVC, VER | State review + transition/fault/energy tests | Product hardware | Open evidence | `CL-06`, `CL-09` |
| `REQ-SYS-014` | The product shall have a measured energy budget for peak, active, idle, storage, radio, light, audio, haptic, update, charge, and fault conditions with margin and an accepted runtime/charge-time requirement. | Target power table is calculated and internally incomplete | Target | SYS, HW, FW, VER | Worst-case analysis + instrumented profile | Product power tree/enclosure | Open evidence | `CL-06` |
| `REQ-SYS-015` | GPIO, peripheral instances, buses/addresses, DMA, interrupts, timers, cores, CPU, internal RAM, PSRAM, task stacks, nonvolatile storage, test/debug access, and expansion reserve shall close in typical and worst-case budgets with stated margin. | G1 must prevent a selected architecture from exhausting shared hardware/software resources | Target | SYS, FW, HW, VER, MFG/SVC | Static allocation + worst-case instrumentation/stress | Selected product architecture and firmware profile | Open evidence | `CL-09`, `CL-17` |

### Physical, Quality, Compliance, And Manufacture

| ID | Candidate requirement | Source / rationale | Class | Owner / allocations | Verify | Environment | State | Closure |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `REQ-QUAL-001` | Launch use shall be supervised indoors on flat gym wood, rubber, vinyl, table, or concrete surfaces; other environments require an explicit new baseline. | Product brief operating boundary | Working decision | PROD, ME/ID, HW, VER | Requirement inspection + representative testing | Named launch surfaces | Candidate | `CL-01`, `CL-07` |
| `REQ-QUAL-002` | The pod shall remain stable without sliding or tipping under accepted 1-3 N center and edge touch profiles on every launch surface. | ID target and primary use | Target | ME/ID, HW, VER | Instrumented force/stability test | Named surfaces, clean/dry | Open evidence | `CL-05`, `CL-07` |
| `REQ-QUAL-003` | The touch system shall meet accepted intentional-hit and false-trigger rates across location, user, glove, vibration, temperature, humidity, contamination, and unit variation. | “100%” target is not statistically verifiable as written | Target | SYS, HW, FW, ME/ID, VER | Designed experiment + confidence bounds | Accepted touch stack/environment | Open evidence | `CL-05`, `CL-07`, `CL-14` |
| `REQ-QUAL-004` | Cue and status light shall meet accepted color, brightness, uniformity, viewing-angle, ambient-light, camera-artifact, thermal, and power limits. | Gym visibility and diffuser/LED trade | Target | HW, FW, ME/ID, VER | Optical/power/thermal measurement | Indoor gym lighting | Open evidence | `CL-05`, `CL-06`, `CL-14` |
| `REQ-QUAL-005` | Audio and haptic cues shall meet accepted detectability, loudness/feel, latency, distortion/noise, accessibility, power, and enclosure-coupling limits. | Product capability; current command pass is not physical proof | Target | HW, FW, ME/ID, VER | Instrumented + representative-user test | Accepted mounting/acoustic environment | Open evidence | `CL-05`, `CL-06`, `CL-14` |
| `REQ-QUAL-006` | The product envelope shall remain within 100-120 mm corner width, 40-50 mm height, and 150-200 g unless placement, stability, optics, RF, acoustic, thermal, or user evidence changes it. | ID target; 60 mm/80 mm PCB assumptions unresolved | Target | ME/ID, HW, VER | CAD stack/inspection + prototype test | Form-factor prototype | Open evidence | `CL-07` |
| `REQ-QUAL-007` | The product shall survive the defined 1 m drop sequence and repeated accepted hand strikes without unsafe damage or loss of required function. | ID durability target; sequence/pass criteria missing | Target | ME/ID, HW, VER, MFG/SVC | Preconditioned drop/impact test + inspection | Accepted surfaces/orientations | Open evidence | `CL-07`, `CL-11` |
| `REQ-QUAL-008` | The enclosure shall meet IP54 only if the launch-market need and complete sealed product, including ports, speaker, contacts, and controls, pass the applicable test. | Architecture/ID target, not launch proof | Target | ME/ID, HW, VER | Standard-route ingress test | Complete representative enclosure | Open evidence | `CL-07`, `CL-11` |
| `REQ-QUAL-009` | Product and charging temperatures, component derating, battery limits, and user-touch limits shall close at worst-case simultaneous load and fault with stated margin. | LED/charge/power peaks and enclosure are unresolved | Target | HW, FW, ME/ID, VER | Worst-case analysis + thermal/fault test | Product enclosure and charge system | Open evidence | `CL-06`, `CL-11` |
| `REQ-QUAL-010` | Launch-market RF, EMC, battery transport/safety, electrical safety, environmental, and restricted-substance obligations shall have an identified route, owner, samples, and schedule before G1. | Compliance path can alter architecture/layout | Target | HW, VER, MFG/SVC, CEO | Compliance matrix + qualified review | Named launch markets | Open evidence | `CL-11` |
| `REQ-QUAL-011` | Architecture-driving reliability, storage, cycle-life, wear, repair, warranty, and support bounds shall close or block G1; DVT shall verify the accepted bounds on representative units. | Reliability/service needs can change parts, enclosure, battery, access, and cost | Target | PROD, HW, ME/ID, VER, MFG/SVC | G1 analysis/plan + DVT accelerated/life evidence | Product lifecycle | Open evidence | `CL-07`, `CL-12`, `CL-13`, `CL-16` |
| `REQ-QUAL-012` | Every unit shall support controlled programming, unique identity, calibration, test, traceability, failure disposition, rework, recovery, and post-enclosure service access. | HW-WP-001 DFT/service input; no product interface exists | Target | HW, FW, MFG/SVC, VER | Fixture/service demonstration + record audit | Intended factory/service process | Open evidence | `CL-02`, `CL-12` |
| `REQ-QUAL-013` | Every selected critical component shall have a controlled record covering exact MPN/package, ratings/derating, footprint, lifecycle/supply/cost, credible alternate or mitigation, driver/compliance evidence, manufacturing impact, and invalidation trigger. | HR2 component baseline and hidden-redesign prevention | Target | HW, FW, VER, MFG/SVC | Record inspection + sample/risk evidence | Selected product architecture | Open evidence | `CL-15` |
| `REQ-QUAL-014` | A preliminary cross-functional FMEA shall identify critical failure modes, effects, causes, controls, detection, mitigation, owner, evidence, and residual disposition before G1. | G1 risk input; safety and architecture failures cannot remain hidden | Target | VER, HW, SYS, ME/ID, FW, MFG/SVC | FMEA audit + linked risk evidence | Product lifecycle and misuse boundary | Open evidence | `CL-16` |

## Interface-Control Register

This register controls responsibility and decision boundaries, not wire values. Exact current pins,
protobuf fields, frame bytes, addresses, partitions, and commands remain owned by the linked source.
Changing a wire contract requires updating its source first and then every generated consumer.

| ID | Interface and ownership boundary | Current authority / constraint | Target contract before G1 | Producers / consumers | Closure |
| --- | --- | --- | --- | --- | --- |
| `IF-ID-001` | Factory identity -> pod -> kit/app/result/service | `config.proto` exposes mutable `pod_id` 1-255; CP2102N/MAC identities are development identifiers | Stable product serial, mutable display identity, kit membership, replacement, privacy, label/data carrier, and record migration | MFG/SVC, FW, APP, SYS | `CL-02`, `CL-12` |
| `IF-HOST-001` | Phone/CLI control -> pod runtime | Protobuf config/trace sources plus bounded OTA exception; app is direct single-pod BLE in production UI | Version/capability negotiation, authority, drill, result, diagnostic, update, error, cancellation, and safe-state semantics independent of topology | APP/CLI, FW, SYS, VER | `CL-09`, `CL-10` |
| `IF-PHONE-001` | Supported phone/platform -> install/permissions/storage/export/update acquisition | No accepted OS/device matrix; app persistence and multi-pod production path are incomplete | Supported OS/device versions, installation/update source, BLE/network permissions, offline boundary, storage budget, migration, export/share, reset/deletion, and compatibility lifecycle | PROD, APP, SYS, VER, MFG/SVC | `CL-01`, `CL-10`, `CL-12` |
| `IF-USER-001` | Operator -> physical power/reset/service controls and visible status | NFF development controls are not product controls; product shutdown and charge status are absent | Control type/location, debounce/hold semantics, boot/off/reset/recovery behavior, accidental-operation protection, accessibility, sealing, status meanings, and manual recovery | PROD, ME/ID, HW, FW, APP, VER | `CL-03`, `CL-06`, `CL-07`, `CL-10` |
| `IF-PEER-001` | Pod <-> pod runtime | Internal hand-written ESP-NOW contract supports a fixed two-pod flow | Six-node discovery/roster, session authority, timing, drill, duplicate/stale rejection, recovery, coexistence, and versioning | FW, SYS, VER | `CL-08`, `CL-09` |
| `IF-TIME-001` | Physical event -> pod clock -> correlated result | Touch carries local timestamp; app scores with phone wall time; trace alignment is not clock correlation | Clock domains, sync/correlation procedure, uncertainty propagation, event/round IDs, late/stale bounds, and result provenance | HW, FW, APP, VER | `CL-04` |
| `IF-PWR-001` | Cell/input/protection/power path/rails -> loads | NFF is DevKit/carrier powered and is not the proposed product tree | Voltage/current/transient limits, sequencing, enable ownership, brownout, measurement, grounding, protection, faults, test points, and margin | HW -> every load; FW/VER observe/control | `CL-06` |
| `IF-CHG-001` | External power/storage -> six pods -> user/status | No product battery, charger, protection, stack, or consolidated storage exists | Topology, contact/connector ratings, orientation, current sharing, thermal/fault isolation, charge state, individual fallback, sealing, and service behavior | HW, ME/ID, FW, APP, MFG/SVC, VER | `CL-06`, `CL-07`, `CL-11` |
| `IF-RF-001` | Phone/pod radio -> antenna/enclosure/coexistence | NFF module/placement and two-board results; no enclosure detuning or six-node proof | Radio roles, channels, airtime/load, provisioning, antenna/module choice, keep-outs, enclosure/battery effects, coexistence, range, debug, and compliance route | HW, FW, APP, ME/ID, VER | `CL-08`, `CL-11` |
| `IF-LED-001` | FW LED frames -> electrical LED chain -> optics/user | NFF has 16 RGB-mode LEDs on active `config.hpp` pin; product target proposes RGBW | Device/quantity, voltage/peak current, level shift, timing/order, brightness limits, thermal controls, diffuser geometry, status/cue semantics, test/calibration | FW, HW, ME/ID, VER | `CL-05`, `CL-06` |
| `IF-TOUCH-001` | User/surface -> electrode/mechanics -> sensing/event | NFF has four pads and 10 ms polling; physical response not proven | Active area, stack/materials, controller/electrode, thresholds/calibration, interrupt/polling, hit/false metrics, event timestamp, wet/glove/misuse behavior | ME/ID, HW, FW, VER | `CL-03`, `CL-05`, `CL-07` |
| `IF-MOTION-001` | Product motion -> IMU/interrupt -> event/fusion | NFF LIS2DW12, active address/pin; command/driver tests are not physical proof | Part/range/rate, mounting axes, interrupt, calibration, fusion/authority, false-trigger metrics, power states, factory test | ME/ID, HW, FW, MFG/SVC, VER | `CL-05`, `CL-06` |
| `IF-AUDIO-001` | FW sample/control -> amplifier/speaker/enclosure -> user | NFF MAX98357A/I2S/speaker; no physical loudness or volume acceptance | Source/format/storage, bus/pins, enable, amplifier/speaker ratings, acoustic volume/port/sealing, latency/loudness/distortion, power, factory test | FW, HW, ME/ID, MFG/SVC, VER | `CL-05`, `CL-06`, `CL-07` |
| `IF-HAPTIC-001` | FW effect -> driver/actuator/mount -> user | NFF DRV2605L/LRA candidate; physical behavior not proven | Actuator/driver/mode, bus/address, effect envelope, coupling/mount, detectability/noise, power, calibration and factory test | FW, HW, ME/ID, MFG/SVC, VER | `CL-05`, `CL-06`, `CL-07` |
| `IF-MECH-001` | PCB/battery/antenna/contacts/optics/acoustics -> enclosure | Target dimensions and conflicting 60/80 mm PCB assumptions; no product CAD stack | Datums, outline/height, keep-outs, tolerances, fasteners, material stack, seals, assembly/service, RF/thermal/optical/acoustic zones, drop loads | ME/ID, HW, MFG/SVC, VER | `CL-07`, `CL-11`, `CL-12` |
| `IF-DBG-001` | Development/service host -> boot/reset/debug/recovery | NFF CP2102N runtime plus separate native USB console/JTAG; product interface unselected | USB role, boot/reset access, JTAG/security lifecycle, recovery path, post-enclosure access, ESD, fixtures, permissions, service boundary | HW, FW, MFG/SVC, VER | `CL-10`, `CL-12`, `CL-17` |
| `IF-MEM-001` | Boot/update/runtime/trace/coredump/factory data -> nonvolatile memory | NFF 8 MB layout with two `0x1E0000` OTA slots and no factory app partition | Exact capacity/layout, secure boot/encryption impact, rollback, migration, audio/data budgets, wear, coredump/trace, factory/calibration ownership | FW, HW, MFG/SVC, VER | `CL-09`, `CL-10`, `CL-12`, `CL-17` |
| `IF-MFG-001` | Controlled design -> CM fixture/process -> serialized unit/evidence | NFF design exports are incomplete as a production package | Released BOM/AVL/substitution, programming, identity, calibration, test points, fixture protocol, golden units, traceability, disposition, rework and evidence retention | HW, FW, MFG/SVC, VER | `CL-02`, `CL-12`, `CL-15`, `CL-16` |

## Closure Ledger

The decision date is the latest date at which the input can remain open without invalidating the G1
forecast. `Fallback / stop` is mandatory behavior when evidence is absent or fails.

| ID | Open input and required evidence | Closure owner | Decision by | Fallback / stop | Invalidation rule |
| --- | --- | --- | --- | --- | --- |
| `CL-01` | Customer/operator evidence, supported phone/platform, setup tolerance, launch environment, minimum kit, and purchase evidence | CEO/product owner; AI product lead | G1, 2026-09-15 | Retain six-pod indoor hypothesis; make no demand/fit claim and do not freeze a dependency contradicted by discovery | Credible evidence changes buyer/job, kit, environment, offline boundary, or setup flow |
| `CL-02` | Product identity scopes, persistence, replacement, privacy, factory assignment, label/data carrier, and collision/migration tests | AI systems lead; HW and MFG/SVC owners unassigned | G1, 2026-09-15 | Reserve identity storage, label space, and protocol extensibility; no schematic release if identity/test access is unallocated | Identity cannot survive factory-to-service lifecycle or changes memory, security, label, or fixture architecture |
| `CL-03` | Setup/identify/preflight and touch-to-feedback distributions with representative users and instrumented systems | Product/UX owner unassigned; AI verification lead | G1 for architectural bounds; DVT for user acceptance | Use the explicit preflight and <=10 ms feedback candidates only for feasibility; no performance claim or freeze if budgets cannot support them | Measured need or system latency changes sensing, compute, radio, light, or UX architecture |
| `CL-04` | Clock domains, correlation algorithm, accuracy/uncertainty target, traffic sensitivity, instrumentation, and held-out validation | AI firmware/simulation/verification leads | G1 contract, 2026-09-15; G2 proof | Preserve timestamps and correlation hooks; label results uncorrelated and prohibit synchronized/one-way claims | Held-out error exceeds accepted bound or topology changes clock ownership |
| `CL-05` | Physical light, touch, motion, audio, and haptic measurements on both NFF boards plus selection-critical coupons/user thresholds | AI test lead; lab operator; ME/ID owner unassigned | G1, 2026-09-15 | Treat commands/self-tests as initialization only; do not select sensing, optics, acoustic, or haptic architecture from them | Physical evidence misses target or enclosure stack changes transfer behavior |
| `CL-06` | Product power tree, cell/pack, charger/path/protection, rail/transient budgets, runtime/charge targets, thermals, faults, and margins | HW design owner unassigned; AI FW/VER support | G1, 2026-09-15 | Do not inherit NFF or proposed TP4056/LDO/1200 mAh choices; no schematic release until worst-case budgets close | Any simultaneous peak, fault, thermal, sourcing, safety, or runtime case breaks margin or changes topology |
| `CL-07` | Controlled CAD stack, dimensions/tolerances, touch/optical/acoustic/RF/thermal interfaces, grip, stack/storage, sealing, drop and service concept | ME/ID and HW owners unassigned | G1, 2026-09-15 | Preserve 100-120 x 40-50 mm and 150-200 g as trade space only; no placement freeze without stack evidence | Envelope or material change invalidates placement, antenna, touch, optics, acoustics, thermal, charge, or stability |
| `CL-08` | Phone/pod topology trade, antenna/module path, provisioning, six-node traffic/range/coexistence, RF enclosure effects, and measurement | AI systems/FW leads; HW/RF owner unassigned | G1 architecture, 2026-09-15; G2 validation | Preserve topology-neutral host/peer contracts; do not claim range/sub-ms performance or freeze antenna placement | Six-node, enclosure, coexistence, certification, or phone evidence changes radio/antenna/topology |
| `CL-09` | Unified protobuf-owned drill/session/result/recovery contract, state model, resource budget, compatibility, and six-node feasibility | AI firmware/software/systems leads | G1 interface, 2026-09-15; G2 implementation proof | Reserve versioned extensibility and use no current fixed two-pod or app-label behavior as the product contract | Contract cannot express workflow or exceeds memory/CPU/radio/storage budgets |
| `CL-10` | Threat/privacy model; authenticated update; rollback/recovery; key lifecycle; diagnostics; data retention/export/reset; service permissions | AI security/FW/app leads; service owner unassigned | G1 architecture, 2026-09-15 | No automatic/remote/security claim; preserve recovery/debug capacity and keep local data user-controlled | Threat or key/update/data lifecycle changes hardware root, memory, debug, radio, or service architecture |
| `CL-11` | Named launch markets, regulatory classification, RF/EMC/safety/battery/environment route, qualified reviewers, samples, schedule, and pre-scan plan | Compliance owner unassigned; CEO names markets | G1, 2026-09-15 | No compliance claim or irreversible layout/enclosure/charge choice without route review | Applicable requirement changes architecture, materials, antenna, battery, spacing, shielding, labels, or test samples |
| `CL-12` | Manufacturing/service/open-product scope, license/obligations, DFM/DFA/DFT, fixtures, programming/calibration, traceability, repair/rework and release content | MFG/SVC/quality owners unassigned; CEO owns license scope | G1 design inputs, 2026-09-15 | Preserve test/debug/label space and editable sources; do not claim open source or release a production package | Factory/service/license need changes access, identity, memory, security, components, assembly, or support economics |
| `CL-13` | Customer price evidence and fully burdened cost model with BOM, enclosure, charger, manufacturing, test, freight, scrap, warranty, compliance, support and open-product cost | CEO/product owner; HW/MFG owners unassigned | G1 feasibility, 2026-09-15 | Treat USD 349-439 and <=50% net-price cost as hypotheses; change offer before hiding required cost | Credible price/cost/support evidence cannot close the product economics with required quality and scope |
| `CL-14` | Serialized NFF exact population, source revision, physical peripheral observations, current/transient/timing/RF data, instruments, uncertainty, and raw evidence | AI test lead; lab operator; HW owner unassigned | HR0, 2026-08-24 | Use NFF only as an explicitly incomplete development baseline; no product extrapolation | New as-built or measurement evidence changes a requirement, trade assumption, driver, or test method |
| `CL-15` | HR2 component matrix and preliminary full M-BOM/AVL with exact MPN/package, datasheet/rating/derating, tested sample where critical, footprint, lifecycle/supply/cost, alternate/mitigation, driver/license, compliance and manufacturing evidence | Qualified HW design owner, unassigned; AI FW/VER support | G1, 2026-09-15 | Keep selections provisional and preserve trade space; no critical-part or schematic freeze without a complete controlled record | New lifecycle, sourcing, rating, sample, footprint, driver, compliance, cost, assembly, or alternate evidence defeats the selected part or budget |
| `CL-16` | Preliminary cross-functional FMEA plus safety, reliability, misuse, detection/mitigation, residual-risk, risk-coupon, and escalation records linked to requirements and interfaces | AI verification lead; HW/ME/ID/MFG owners unassigned | G1, 2026-09-15 | No `Conditional Go` for an open failure capable of changing topology, parts, safety, compliance, outline, placement, firmware architecture, or economics | New failure mode, severity, occurrence, detectability, ineffective mitigation, or coupon result reopens the affected requirement/interface and gate evidence |
| `CL-17` | Typical/worst-case allocation and margin for GPIO, peripherals, buses/addresses, DMA, interrupts, timers, cores/CPU, RAM/PSRAM/task stacks, flash/OTA/trace/coredump/factory data, test/debug access, and expansion | AI firmware/systems leads; qualified HW design owner unassigned | G1, 2026-09-15 | Preserve hardware and firmware reserve; no part, pin, memory, or schematic freeze while a shared resource is overcommitted or unmeasured | Selected parts, protocol load, security/update layout, tracing, factory/service access, or measured worst case exhausts budget or margin |

## Workflow Traceability

| Workflow | Primary requirements | Primary interfaces | Verification focus |
| --- | --- | --- | --- |
| `CW-01` Unpack and power | `REQ-PROD-001`, `REQ-SYS-013`, `REQ-SYS-014` | `IF-PWR-001`, `IF-ID-001` | Power, identity, offline start, fault state |
| `CW-02` Identify kit roster | `REQ-PROD-003`-`005`, `REQ-SYS-001` | `IF-ID-001`, `IF-HOST-001`, `IF-RF-001` | Duplicate/missing/incompatible/low-energy pods |
| `CW-03` Choose participant/drill | `REQ-PROD-006`, `REQ-SYS-003`, `REQ-SYS-008` | `IF-HOST-001`, `IF-MEM-001` | Bounds, privacy, stale configuration |
| `CW-04` Configure and validate | `REQ-PROD-006`, `REQ-SYS-002`, `REQ-SYS-003` | `IF-HOST-001`, `IF-PEER-001` | Validation, resources, authority |
| `CW-05` Place and preflight | `REQ-PROD-004`, `REQ-QUAL-002`-`005` | `IF-TOUCH-001`, `IF-MOTION-001`, `IF-LED-001` | Readiness, movement, energy, physical cues |
| `CW-06` Start and play | `REQ-SYS-002`-`006`, `REQ-SYS-012` | `IF-TIME-001`, `IF-PEER-001`, physical-output interfaces | Correlation, latency, duplicates, six-node load |
| `CW-07` Handle partial failure | `REQ-PROD-007`, `REQ-SYS-007` | `IF-HOST-001`, `IF-PEER-001`, `IF-TIME-001` | Disconnect, stale/late data, ambiguous authority |
| `CW-08` Recover/abort/resume | `REQ-PROD-007`, `REQ-PROD-008`, `REQ-PROD-010` | `IF-HOST-001`, `IF-MEM-001` | Durable boundary, safe cleanup, annotated result |
| `CW-09` Review results | `REQ-PROD-008`, `REQ-PROD-009`, `REQ-SYS-004`, `REQ-SYS-008` | `IF-TIME-001`, `IF-MEM-001` | Provenance, missing data, offline store/export |
| `CW-10` End session | `REQ-PROD-010`, `REQ-SYS-013` | `IF-HOST-001`, `IF-PWR-001` | Every-state cleanup and manual recovery |
| `CW-11` Charge and store | `REQ-PROD-011`, `REQ-SYS-014`, `REQ-QUAL-009` | `IF-CHG-001`, `IF-PWR-001`, `IF-MECH-001` | Six-pod readiness, thermal/fault isolation |

## Current-To-Target Conflicts That Must Remain Open

| Conflict | Current constraint | Target/proposal | Required disposition |
| --- | --- | --- | --- |
| Timing | Two-board RTT is 2.6-20.8 ms; clocks are not correlated | Sub-ms radio and +/-1 ms synchronization targets | `CL-04`/`CL-08`; correlated evidence or revise targets |
| Touch latency | NFF polls at 10 ms; physical path is unverified | <10 ms touch-to-feedback target | `CL-03`/`CL-05`; instrument complete path before freeze |
| Power | NFF uses a DevKit/carrier power arrangement | 1200 mAh, TP4056, 500 mA LDO proposals and up to 960 mA LED peak | `CL-06`; new worst-case topology/budgets, not inheritance |
| Product profile | Only NFF N8R8 board profile exists | Preliminary production pins, 16 MB memory, native USB product path | `CL-09`/`CL-10`/`CL-12`; controlled product interface before schematic |
| Topology | App UI is single-pod BLE; peer flow is fixed two-pod ESP-NOW | Six-pod phone-to-master hybrid proposal | `CL-08`/`CL-09`; topology trade behind common contracts |
| Physical design | No product enclosure; 60 mm and ~80 mm PCB assumptions coexist | 100-120 mm x 40-50 mm, 150-200 g, stack/charge/IP54 targets | `CL-07`; one controlled CAD/interface stack |
| Identity/service | Mutable pod ID and development transport identifiers | Stable kit/pod/result identity and factory/service traceability | `CL-02`/`CL-12`; lifecycle and fixture contract |
| Open product | Source exists but no accepted repository license/release bill | Open, repairable product promise | `CL-12`; explicit scope, obligations, support, and release contents |

## G1 Use And Stop Boundary

PS-WP-002 is complete when this candidate is traceable, internally consistent, reviewable, and all
open inputs have an assigned owner or explicitly recorded unassigned ownership decision, dates,
fallbacks, and invalidation rules. That completion is not a G1
technical `Go`. G1 remains `Hold` for any architecture-affecting open item in the closure ledger,
including the unassigned qualified hardware owner, incomplete HR0 evidence, unresolved power/charge
tree, absent compliance route, or missing manufacturing/service interfaces.

G1 may accept only a later immutable revision whose critical requirements, interfaces, budgets,
parts, risk evidence, FMEA, compliance route, and manufacturing/test inputs pass the program gate
contract. Until then, analysis, supplier engagement, evaluation kits, coupons, simulation, protocol
design, and test planning may proceed inside their existing authorization, but schematic/layout
release, fabrication, certification, and product claims may not.
