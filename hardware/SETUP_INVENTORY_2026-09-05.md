# Development setup: initial inventory and coverage

HW-WP-002 / issue #199. Initial desk delivery, 2026-09-05 UTC. AI systems coordinator owns this
record; qualified electrical design owner remains unassigned. No measurements or purchases were
performed for this desk delivery. This is not HW-WP-002 acceptance or LAB0/LAB1 commissioning.

## Evidence-backed inventory

| Asset | Evidence and availability | Capability / limits | Owner or next action |
| --- | --- | --- | --- |
| Two NFF N8R8 carriers | Both observed in the September 5 retained campaign; identities and image hashes in the linked report | 8 MB flash / 8 MB PSRAM; UART programming and CLI responses passed; both full readiness checks failed Heap | Coordinator schedules one lab session; Paulo or qualified operator must confirm physical setup |
| CP2102N UART cables | Both stable identities directly used for 6/6 final identity queries | Framed control path verified; cable presence does not prove safe combined powering or peripheral outputs | Preserve board-to-identity mapping; never infer pod ID from port order |
| Native USB console/JTAG cables | Absent in the retained campaign | No separate current boot-console/JTAG access established | Confirm suitable cables/access before a console-dependent experiment |
| ministrom development host | Clean main 3b62a6c, controller validation passed and authenticated workers launched September 5 | Native Linux; pinned firmware/Rust used in retained build. Codex/Flutter resolve through normal login environment; availability does not prove every toolchain check passes | Coordinator; no service changes |
| Local app/development host | Existing app checkout and tool executables; app worker has a separately controlled workspace | Host-only tests and virtual app work; Linux does not establish an iOS build or physical-phone BLE | App worker; exact-version analysis/tests still required |
| Launch phones and Mac build access | Actual inventory, OS versions and owner slots unknown in this review | Needed for signed real-phone behavior and supported Apple build/runtime evidence | Paulo confirms launch platforms and accessible phones/Mac |
| Supply, DMM, current probe/shunt, scope/logic analyzer | Possession, model/range/bandwidth, calibration and safe probes unknown | Electrical/timing acceptance cannot start from assumed instruments | Qualified design/test owner inventories before LAB1 |
| Thermal/optical/acoustic/haptic fixtures | Availability and calibration unknown | Visual observation is distinct from quantitative brightness, latency, sound or vibration evidence | Define requirement-specific measurement and uncertainty first |
| Four additional physical nodes | Not verified or purchased | Two current boards cannot establish six-node physical behavior; bare radio nodes do not establish complete pods | Review retain/borrow/buy proposal and supported profile before spending |
| Power-path/regulator/enclosure/RF coupons | No commissioned inventory or qualified owner | NFF evidence cannot qualify battery charging, enclosure RF, product thermal or safety behavior | Desk specification now; separate qualified review/budget before acquisition or tests |

The NFF evidence is [the retained programming/readiness report](../docs/plans/nff-serial-readiness-2026-09-05.md).
Availability is dated, not a guarantee that devices remain connected. No new device access occurred
in this activation. Historical docs saying all access is unverified are superseded only for the
specific observed UART/build facts; populated-board inspection and instrument safety remain open.

## Scenario-to-capability coverage

| Scenario / decision | Required setup and observable result | Uncertainty / sampling requirement | Current gap |
| --- | --- | --- | --- |
| NFF1 baseline/recovery | Identity-bound image, safe supply inspection, CLI, separate boot evidence and supported recovery | Retain both boards and every test result; define safe rails/current with qualified owner | Heap fails on both; safe-bench inspection, normal-boot and supported-recovery acceptance still open |
| NFF2 peripheral behavior | Stimulus and observer/fixture for every LED, touch pad, IMU, LRA and speaker | Record expected vs observed per board/peripheral; quantitative thresholds come from accepted requirements | Command acceptance is not observation; no complete current peripheral record |
| NFF3 operating envelope | Current-limited power source, characterized current path, voltage/thermal measurement and correlated timing capture | Bandwidth must resolve relevant peaks; record calibration, probe loading, trigger and clock-correlation uncertainty; sample count/test duration must be specified before execution | Instruments, numeric uncertainty budget and safe injection/current limits unassigned |
| NFF4 phone/two-pod faults | Real launch phone, both ready pods, CLI/log capture and exclusive radio session | Separate physical touches, complementary one-peer roles, exact disabled lifecycle, both directions and repeated lifecycle evidence per testing contract | Phone inventory, heap repair/revalidation and physical peripheral prerequisite |
| NFF5 requested coverage | HW-WP-002's exit text names NFF1-NFF5, but the reviewed delivery graph defines NFF1-NFF4 then HR0 | No new milestone, mapping or threshold may be invented to conceal the mismatch | Specification clarification required; NFF5 remains undefined and HR0 retains its existing independent release meaning |
| FS3 shared contract | Generated schema, real consumers and deterministic fixtures plus ticketed two-board regression | Bind exact versions and clock provenance; no synthetic evidence substituted for physical regression | Shared-contract acceptance and ready physical fleet remain open |
| FS4 phone workflow | Two/six-identity virtual lab; later real-phone six-node campaign with supported profiles | Reproducible seed/time in the virtual model; distinct physical timing/fault/soak evidence | App lab is executing; four extra nodes and actual phones are not verified |
| HR0 release | Raw NFF2/NFF3 data, exact setup and procedures, calibration, failures and accepted limits | Preserve measured envelope and invalidation triggers; do not generalize to enclosed battery product | Not due; no accepted measurement bundle |
| HR1 architecture | Product envelope, measured loads/timing, memory budget, interface record and topology trade-offs | Quantitative margins and uncertainty must close before downselect | Design owner, product envelope and measurements missing |
| HR2 components/alternates | Requirement-linked candidates, current manufacturer/distributor evidence and bench/coupon results where needed | Record lifecycle, availability, margins and alternate compatibility; quotes are dated, not guarantees | No current quote package or frozen selection; do not carry candidate charger/LDO choices forward automatically |

## Safe-bench prerequisites

Before any new powered measurement, the qualified owner must specify injection points, allowed
voltage/current limits, USB/5 V backfeed prevention, ground arrangement, connected UART/native-USB
paths and safe probe loading. Record the actual populated carrier and actuator. The coordinator
must reserve the exclusive fleet and retain its known firmware/configuration state and recovery
procedure. This document intentionally supplies no guessed electrical limits or wiring approval.

## Decision-ready next actions

1. **Hardware accountability:** name a qualified electrical design owner. Recommendation: retain an
   owner capable of power/embedded/RF review, with specialist battery/RF support when required.
   Alternative: keep work at desk definition. Delay leaves G1 forecast unset; no paid engagement
   or controlled design responsibility is inferred from AI preparation.
2. **Product and phone envelope:** confirm or revise six-pod indoor offline-first use, launch phone
   platforms and first market before freezing the architecture. Alternative: keep those as explicit
   hypotheses. App virtual-lab work can continue meanwhile.
3. **Instrument access:** Paulo/owner should supply owned or borrowable instrument models, calibration
   status, phones/Mac access and operator slots. Recommendation: retain the two NFFs, instrument the
   existing bench, then separately price missing capabilities and four-node expansion. Alternative:
   borrow a qualified lab. Without inventory, a costed buy list risks redundant or unsuitable spend.

No vendor quote, price, delivery date, calibration or equipment ownership is invented. The next
HW-WP-002 slice turns this record into supported-board/profile options and a current-source costed
proposal, after narrowing the unknown inventory and requirements. Schematic capture remains behind
G1, routing behind HR3, and ordering behind HR4/G2 plus separate budget approval.

## Added-node profile contract: desk refinement at 01:22 UTC

This comparison refines the existing setup package; it selects no board or supplier and authorizes
no build, purchase or physical test. An independent repository review confirmed that only the NFF
physical profile is currently supported. Preserve the distinction between proposed capability and
accepted evidence.

| Path | Prerequisite before counting evidence | Explicit limit |
| --- | --- | --- |
| Retain two NFFs | Exact existing profile, LAB0 safe setup, NFF1 readiness/recovery, then applicable campaign prerequisites | Two nodes cannot close six-node evidence; the present heap failure remains open |
| Four radio-only additions | Complete selected board profile, clean pinned build, identities and authorized physical validation | Can support later radio-scale campaigns; cannot prove six complete interactive pods |
| Four full-interaction additions | Same profile proof plus per-node LED, touch, IMU, haptic and audio inventory/equivalence | Physical workflow still requires actual launch phones and accepted test prerequisites |
| Product-risk coupons | Requirement-specific design/measurement plan, qualified owner and separate budget/test authority | Power/charging, enclosure or RF evidence is separate from fleet expansion and is not a product release |

Every added-node proposal must fill these fields before a firmware/profile ticket is accepted:

- Stable board/revision identity and actual MCU, flash and PSRAM geometry.
- Complete pin/peripheral inventory, with missing or non-equivalent capabilities explicit.
- Profile-selection mechanism, SDKCONFIG defaults, partition layout and generated-protocol compatibility.
- Protocol UART versus console/JTAG route, cable/USB state and safe-power review owner; no guessed limits.
- Exact clean-build source, toolchain/config/image identities and numerical memory/image-size record.
- Required authorized hardware tests, expected observations, instruments/uncertainty and restoration plan.
- A per-node radio-only/full-interaction capability statement; no capability inferred from chip family.

The existing NFF defaults specify 8 MB flash, 8 MB octal PSRAM and an NFF-sized partition layout.
Changing an isolated pin or substituting a generic development board does not establish a supported
profile. See [physical profile rules](../firmware/AGENTS.md),
[current defaults](../firmware/domes/sdkconfig.defaults),
[partition layout](../firmware/domes/partitions.csv) and [pin authority](../docs/PIN_REFERENCE.md).
Inventory, calibration, numerical electrical limits, quotes and the NFF5 naming conflict remain open.

Authorities: [setup package](DEVELOPMENT_SETUP.md), [hardware request](NEXT_ITERATION_REQUEST.md),
[program ledger](../PROGRAM_STATUS.md), [verification](../docs/TESTING.md).
