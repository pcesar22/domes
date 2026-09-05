# HW-WP-002: Development Validation Setup Definition

State: Active. Parent: HW-WP-001. Delivery owner: AI systems lead. Qualified electrical design
owner: unassigned. Initial desk inventory/coverage delivered 2026-09-05 in
[the setup record](SETUP_INVENTORY_2026-09-05.md), tracked as GitHub #199. Two-NFF UART access is
directly evidenced; instrument possession/calibration, safe bench and full readiness remain open.

## Outcome and scope

Produce a reviewable, costed setup specification enabling three concurrent paths: phone-app
development without devices, reproducible dual-NFF measurements, and product-hardware risk coupons.
This document defines the package and verifies requirement coverage against repository authorities;
it does not claim the equipment is owned, calibrated, installed or purchased.

The initial record identifies an unresolved naming mismatch: this exit mentions NFF5, while the
reviewed graph defines NFF1–NFF4 then HR0. Keep it explicit pending specification clarification;
do not invent a new milestone or treat this first desk delivery as the full package exit.

## Capability-to-equipment matrix

| Need | Retain or define | Evidence it enables | Gap and acceptance |
| --- | --- | --- | --- |
| Fast app development | Existing Flutter 3.44.8 workflow; injected virtual repository/transport and virtual clock | Two/six virtual pod journeys, faults, repeatable results and app CI | FS-WP-004A/B implementation required; direct sim-pod touch bypass is insufficient |
| Physical phone acceptance | Supported launch phone/platform matrix; signed deployment and BLE permissions | Scan/connect/touch/drill/stop/recovery/OTA on an actual phone | Confirm supported platforms, actual phone access and operator; emulator does not establish BLE |
| Stable NFF stations | Retain both N8R8 NFFs; stable CP2102N protocol identity and separate native USB console/JTAG where connected | Reproducible commands, logs, source/image identity and restoration | Two boards recorded historically; current availability and cabling unverified here |
| Controlled hardware execution | Existing native-Linux hardware host, pinned ESP-IDF 5.4.4 and Rust 1.92.0, recorded BLE adapter, one lab owner | Exclusive exact-candidate regression with logs and reproducible setup | Record tools, runner/port ownership and restoration policy; no host-service change in this package |
| Safe electrical reference | Current-limited supply, DMM, current measurement with adequate transient bandwidth; documented probe points | Rails, idle/peripheral/combined load, peak/transient and brownout margin | Instrument range/bandwidth/calibration selected against the measurement; USB current averages alone insufficient |
| Physical timing | Oscilloscope/logic analyzer plus observable stimulus/output; photodiode or equivalent where light timing matters | Correlated stimulus/command/output times with uncertainty | Define trigger, probe loading and clock correlation; merged trace starts and RTT do not prove one-way latency |
| Thermal and coupled outputs | Temperature instrumentation; LED optical, touch-stack, audio and haptic fixtures | Load/thermal/optical/acoustic/mechanical trade evidence | Select objective measures from requirements; observation and quantitative qualification are separate |
| Six-node radio scale | Four additional economical ESP32-S3 radio nodes with a supported profile plan | Discovery, contention, join/leave, recovery, timing and soak at six nodes | Current firmware only supports NFF physical profile; bare boards need a bounded supported profile before credit |
| Six-node full interaction | Inventory each added node's LED/touch/IMU/haptic/audio; equivalent peripherals or explicit evidence limits | Complete six-pod user workflow where the test requires physical input/output | Six radio-only boards cannot prove six complete physical pods; cost the missing fixtures or equivalent carriers |
| Product power/charging | Selection-critical charger/power-path/protection and regulator coupons; qualified design review | Combined-load, dropout, charge/use, temperature and service trade closure | Candidate TP4056/500 mA LDO/discrete protection/VBAT RGBW are unverified; no battery fault testing authorized |
| RF and mechanical risk | Antenna/enclosure fixtures, pre-scan access and qualified specialist as needed | Coexistence, detuning, enclosure, touch/acoustic/haptic impacts | NFF open-carrier performance cannot qualify an enclosed battery product |
| Evidence retention | Raw data storage, versioned procedures, exact image/tool/config hashes, stable identity map and calibration records | HR0, FS3/FS4, HR1/HR2 release inputs | Name custody, storage, retention, invalidation and golden-baseline restoration owner |

## Deliverable and binary exit

1. An itemized inventory distinguishing possessed, accessible/borrowable, needed and unverified,
   with stable identities, capability/range, calibration and responsible operator. No inferred inventory.
2. Scenario-to-equipment coverage for every NFF1–NFF5, FS3/FS4 and HR1/HR2 measurement, including
   expected result, uncertainty budget, instrument/sample requirements and explicit unsupported cases.
3. A supported-board/profile plan for added nodes, mobile platform matrix and fixture/cable/power
   distribution diagram. Preserve protocol UART versus log/JTAG isolation and exclusive lab scheduling.
   Before powered measurements, a qualified reviewer specifies power injection points, current limits,
   USB/5 V backfeed prevention, common-ground arrangement and the connected CP2102N/native-USB state.
   LAB0 confirms the safe bench; LAB1 commissions calibrated measurement capability. Neither a
   definition document nor a cable's presence proves the bench is safe or the instruments available.
4. Costed retain/borrow/buy alternatives with actual current vendor quotes, quantities, availability,
   lead times, owner and decision-by dates. No prices or delivery dates invented in this review.
5. Independent desk review showing every critical measurement can run on the proposed setup or is
   a named blocker. Feed architecture consequences into HW-WP-001, PS1 and VC1.

The recommended next setup is an **instrumented dual-NFF bench plus a host-only app simulation
lab**, with a **separately costed six-node expansion**. It is not automatically a new custom PCB.
Reuse the two NFFs for peripheral learning; add economical radio nodes for scale only after a
supported profile/capability assessment. Use small coupons where product power or enclosure risks
cannot be learned on NFF. The subsequent product revision is HR1/HR2 → G1 → HR3 → HR4/G2 → EVT.

## Boundaries and invalidation

Definition is authorized by the current user request. Stop before purchases, vendor commitments,
host-service changes, destructive tests, schematic release, PCB routing, fab or product claims.
Only the human commits budget; qualified owners remain accountable for controlled designs.
New measurements, product scope, inventory loss or board/profile changes reopen this setup spec.

Authorities: [hardware request](NEXT_ITERATION_REQUEST.md),
[program status](../PROGRAM_STATUS.md), [testing](../docs/TESTING.md),
[product target](../research/SYSTEM_ARCHITECTURE.md),
[current pins/profile](../docs/PIN_REFERENCE.md), and [mobile app](../ios/domes_app/README.md).
