# HW-WP-001: NFF Characterization And Product Architecture Downselect

| Control | Value |
| --- | --- |
| State / health | `Ready` / `Amber` |
| Owner | Unassigned; CEO decision required before activation |
| Program phase | P1 Definition and Feasibility |
| Requested start / finish | 2026-08-04 / 2026-09-15 |
| Feeds | HR0 NFF Reference Closure, HR1 Architecture Downselect, HR2 Component Baseline, and G1 |
| Authorizes | Product-hardware definition and risk reduction |
| Stop condition | No PCB layout release, fabrication, assembly, certification, or production claims |

## Objective

Convert the current NFF development evidence and product targets into an evidence-backed
production-intent architecture, selected component set, and complete input package for an EVT
schematic and PCB design.

This request starts now because the development platform is sufficiently controlled to measure the
remaining unknowns, and because architecture/part-selection work must run in parallel with NFF,
firmware, simulation, product, and verification work. Waiting for every product workstream result before
starting hardware definition would waste the highest-value learning window. Freezing selections
before the required evidence arrives would create the opposite failure.

## Inputs

- NFF schematic, BOM, bring-up material, exact populated-board inspection, and retained campaign
  evidence.
- [`../research/PRODUCT_DEFINITION.md`](../research/PRODUCT_DEFINITION.md),
  [`../research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md), and
  [`../research/ID_REQUIREMENTS.md`](../research/ID_REQUIREMENTS.md).
- Active firmware mapping and peripherals in `firmware/domes/main/config.hpp` and firmware source.
- Protocol, OTA, diagnostics, trace, CLI, app, and CI behavior from implementation and tests.
- G1 evidence requirements and current risks in [`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md).

Targets and proposed part choices are inputs to challenge, not decisions to preserve.

In particular, do not carry the proposed TP4056 charger, 500 mA LDO, discrete battery protection,
VBAT-driven RGBW rail, or charging-base assumptions forward by default. Prove or replace them using
system peak load, simultaneous-peripheral operation, power-path behavior, six-pod charging,
protection/safety, runtime, thermal, supply, and compliance evidence.

## Resource Assumptions

The 2026-09-04 operating review adds [HW-WP-002 Development Validation Setup Definition](DEVELOPMENT_SETUP.md)
as a bounded input package. It specifies the instrumented dual-NFF bench, host-only phone simulation,
real-phone acceptance and separately costed six-node expansion. Definition is ready now; equipment
possession, budget and qualified design ownership remain unverified or unassigned. The old Sep 15
date below is a historical baseline, not a reaffirmed forecast; current forecast is unset in
PROGRAM_STATUS.md.

- one qualified hardware design owner with schematic, PCB, power, RF, and design-review authority;
- AI systems, firmware, verification, sourcing, documentation, and program support;
- access to the two NFF boards, bench power/current instrumentation, oscilloscope or logic analyzer,
  thermal measurement, RF pre-scan support, and selection-critical evaluation hardware;
- bounded evaluation/coupon spend and supplier/CM access approved by the CEO; and
- specialist review for battery safety, RF/compliance, mechanical/industrial design, and DFM/DFT
  where the evidence requires professional or supplier accountability.

The 2026-09-15 forecast remains low confidence until the hardware owner, equipment access, budget,
and review availability are recorded.

## Requested Work

### 1. Requirements Allocation And Interfaces

Produce a hardware requirements allocation and interface-control record covering:

- supply rails, voltage/current limits, buses, addresses, pins, interrupts, timing, boot, reset, and
  power states;
- RF, antenna, coexistence, provisioning, identity, debug, programming, update, recovery, test, and
  calibration interfaces;
- mechanical outline, keep-outs, optics, touch stack, acoustic path, haptic coupling, battery,
  connectors, controls, sealing, stacking, and charging interfaces; and
- every value that remains a range, the measurement that closes it, the deadline, and a fallback
  that preserves schematic/layout progress.

### 2. Architecture And Component Decisions

Evaluate and recommend at least the following product subsystems:

| Subsystem | Required decision |
| --- | --- |
| Compute and radio | ESP32-S3 module versus chip-down, flash/PSRAM capacity, antenna approach, certification leverage, debug access |
| Power input and charging | USB-C role, charger/power-path/protection, fuel gauge, stack charging, ESD/surge/reverse protection |
| Battery | Cell/pack chemistry, capacity, protection, connector, sourcing, UN 38.3/IEC 62133-2 evidence route, serviceability |
| Regulation and power switching | Rail topology, peak/idle efficiency, sequencing, measurement, brownout behavior, thermal margin |
| Light output | RGB/RGBW device, quantity, optical/mechanical stack, peak current, thermal/brightness trade, alternate |
| Touch | Electrode/mechanical stack, controller versus native sensing, wet/glove/false-trigger strategy, calibration |
| Motion | IMU performance, interrupt behavior, lifecycle, address/bus, driver support, alternate |
| Audio | Codec/DAC/PWM strategy, amplifier, speaker, acoustic volume/port, loudness, power, protection, alternate |
| Haptic | Driver, actuator, mounting/coupling, effect envelope, closed/open-loop choice, power, alternate |
| Storage and update | Flash layout, OTA/rollback capacity, coredump/trace storage, factory image and recovery constraints |
| Human controls and indicators | Power/reset/service inputs, charging/status indication, accessibility and service behavior |
| Manufacturing and service | Programming, identity, calibration, boundary access, test points, fixture interface, rework and repair |

For every selected and alternate part, record:

- requirement IDs and decision rationale;
- manufacturer part number, package, lifecycle status, temperature/voltage/current margin;
- distributor availability, lead time, MOQ, unit cost at relevant quantities, and credible alternate;
- driver/API maturity, license, ESP-IDF support, memory/CPU impact, and required firmware work;
- relevant certification, battery transport, safety, environmental, and restricted-substance status;
- known errata, single-source exposure, assembly/test difficulty, and invalidation trigger; and
- evidence date and source. Distributor page availability alone is not lifecycle evidence.

### 3. Engineering Budgets

Deliver typical and worst-case budgets for:

- peak, active, idle, sleep, radio, LED, audio, haptic, charging, and fault current;
- battery runtime, charge time, cycle-life assumptions, and power-path behavior;
- regulator/charger/component dissipation and enclosure thermal rise;
- flash, PSRAM, task stack, OTA slots, rollback, trace, coredump, and factory data;
- GPIO, peripherals, buses, DMA/interrupt resources, and test/debug access; and
- preliminary BOM, PCB, assembly, test, enclosure, certification, warranty, and scrap cost.

Every budget states margin and identifies the measurement or analysis behind it.

### 4. Risk Prototypes And Measurements

Use the NFF boards, evaluation kits, or small breadboards to close only selection-critical risks,
including as applicable:

- LED brightness, diffusion, peak current, thermal behavior, and camera-visible artifacts;
- touch sensitivity/false activation through candidate mechanical stacks;
- speaker/amplifier loudness, acoustic port, distortion, and current;
- haptic actuator coupling, detectability, current, and audible noise;
- battery/charger/power-path peaks, brownout, runtime, thermals, and fault response;
- antenna/module placement, enclosure detuning, BLE/WiFi/ESP-NOW coexistence; and
- six-node traffic assumptions that change radio, memory, or power selection.

Retain procedure, exact parts, instruments, raw data, result, uncertainty, date, and decision impact.

### 5. Design For Verification And Manufacture

Define before schematic release:

- preliminary FMEA with detection and mitigation for critical failure modes;
- launch-market compliance and pre-compliance plan;
- DFM/DFA review inputs, PCB technology assumptions, supply-risk strategy, and substitution control;
- programming, unique identity, calibration, factory test, golden-unit, traceability, and failure
  disposition concepts;
- required test points and recovery/debug access that remain usable after enclosure assembly; and
- EVT quantity, variants, split lots, fixtures, evaluation boards, long-lead buys, and budgetary quote
  assumptions.

## Deliverables

| Deliverable | Required result |
| --- | --- |
| Hardware requirements allocation and interface-control record | Every product-hardware interface has an owner, value/range, evidence, and fallback |
| Architecture decision record | Major topology choices and rejected alternatives are explicit |
| Component decision matrix and AVL candidate | Selected/alternate parts satisfy requirements, supply, lifecycle, cost, compliance, driver, and manufacturing constraints |
| Measured engineering budgets | Power, energy, thermal, memory, pins/resources, RF, and cost close with stated margin |
| Risk-prototype report | Selection-critical unknowns are measured or explicitly block G1 |
| Preliminary FMEA and compliance plan | Critical risks and launch-market evidence routes have owners and timing |
| Manufacturing/test concept | Programming, identity, calibration, test, debug, fixture, traceability, and disposition affect the design before layout |
| EVT input package | Block diagram, preliminary BOM/AVL, interface record, placement/keep-out constraints, and open-item list are ready for schematic/layout |

## G1 Acceptance

The milestone manager audits direct evidence for each deliverable and records `Go`, `Conditional Go`,
`Hold`, `Recycle`, or `Stop` as the G1 technical verdict. G1 may pass only when:

1. no unbounded hardware-driving product or interface decision remains;
2. selection-critical NFF/risk-prototype evidence is direct and current;
3. every selected critical part has a credible availability/lifecycle basis and an alternate or an
   explicitly accepted single-source risk;
4. budgets close with margin and agree with the selected architecture;
5. firmware, mechanical, RF, compliance, verification, manufacturing, and service owners can execute
   their interfaces without discovering a hidden architectural decision; and
6. the open-item list contains no issue capable of invalidating schematic capture or PCB placement.

A G1 `Go` authorizes controlled schematic capture, layout planning, DFM engagement, and EVT quote
preparation. HR3 authorizes PCB routing from the released schematic and interfaces. Only G2
authorizes release of manufacturing files and an EVT fabrication/assembly purchase.
