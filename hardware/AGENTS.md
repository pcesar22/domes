# Hardware Development Guidelines

## Project Context

This folder contains the DOMES electrical hardware design: schematics, PCB layout, fabrication
outputs, and bill of materials.

Reference documents:

- `../PROGRAM_STATUS.md`
- `hardware/NEXT_ITERATION_REQUEST.md`
- `research/SYSTEM_ARCHITECTURE.md`
- `research/ID_REQUIREMENTS.md`
- `hardware/BOM.csv`
- `hardware/nff-devboard/`

## Role

When asked to source, validate, or review hardware components, act as an electrical engineering
agent. Use available JLCPCB/LCSC tools if configured. If no catalog MCP is available, use current
catalog/vendor web data and cite the source in the response.

Primary tasks:

1. Source components and find LCSC part numbers.
2. Validate stock, lead time, package, and assembly compatibility.
3. Find pin-compatible or functionally equivalent alternatives.
4. Prefer JLCPCB Basic Parts when they meet requirements.
5. Update `hardware/BOM.csv` when the user asks for BOM changes.

## Candidate Sourcing Inputs

The parts below are current candidates, not a frozen product architecture or AVL. For the next
product iteration, `hardware/NEXT_ITERATION_REQUEST.md` owns the trade and evidence required before
selection. Do not optimize catalog sourcing around a candidate that has not passed HR1/HR2.

Tier 1 parts should be JLCPCB-assemblable for prototypes:

| Component | Key requirements | Search terms |
| --- | --- | --- |
| ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, PCB antenna | `ESP32-S3-WROOM-1-N16R8` |
| LIS2DW12 | LGA-12, I2C, tap detection | `LIS2DW12` |
| DRV2605L | DFN-10, I2C, LRA haptic driver | `DRV2605L` |
| MAX98357A | QFN-16, I2S amplifier | `MAX98357A` |
| TP4056 | SOP-8, LiPo charger | `TP4056` |
| SPX3819-3.3 | SOT-23-5, 3.3 V LDO | `SPX3819 3.3V`, `HT7833` |

Tier 2 parts should prefer JLCPCB but may have alternatives:

| Component | Key requirements | Alternatives |
| --- | --- | --- |
| SK6812MINI-E | 3535 RGBW addressable LED | Only pin- and footprint-compatible 3535 parts after timing/color-order review; 5050 is not a drop-in footprint |
| LD0832AA-0099F | 8 mm LRA, 1.8 Vrms, 235 Hz | Current NFF schematic part; JLCPCB `C2682305` |
| USB-C connector | 16-pin, USB 2.0 Type-C receptacle | Equivalent footprint-compatible part |
| DW01A + FS8205A | Battery protection | BQ29700 or integrated protection IC |

Tier 3 parts are usually externally sourced:

| Component | Source | Notes |
| --- | --- | --- |
| Alternate LRA haptic motor | DigiKey, Mouser, Alibaba | Requires a matching firmware voltage/frequency profile |
| 23 mm speaker | AliExpress, Alibaba | 8 ohm, 1 W |
| 1200 mAh LiPo | Battery suppliers | 103040 pouch cell |
| Pogo pins | Mill-Max, AliExpress | Spring-loaded, through-hole |
| 6x3 mm magnets | Amazon, AliExpress | N52 neodymium |

## Selection Criteria

For each component, verify:

- Package and footprint compatibility, including pin 1 orientation and thermal pad needs.
- Electrical ratings with appropriate voltage and current margin.
- Operating temperature of at least -10 C to +50 C.
- JLCPCB assembly status: Basic, Extended, Consigned, or unavailable.
- Stock and lead time: 10+ for prototype, 1000+ or restock confidence for production.
- Cost at relevant price breaks.

## Critical Review Rules

- Evaluate the ESP32-S3 module/chip-down, memory, and antenna choice against product, RF,
  certification, service, manufacturing, cost, and supply requirements before freezing it.
- LIS2DW12 must be LGA-12 and support tap detection; do not substitute LIS2DH12 without review.
- DRV2605L must support LRA motors and fixed I2C address 0x5A.
- The current NFF actuator is LD0832AA-0099F; do not substitute another LRA without updating and
  validating the DRV2605L profile.
- MAX98357A must support standard Philips I2S input.
- SK6812 alternative must be RGBW if the design expects a separate white die.
- Do not carry TP4056, the candidate 500 mA LDO, discrete battery protection, or VBAT-driven RGBW
  forward without closing power-path, simultaneous peak load, charging, thermal, protection,
  runtime, sourcing, and compliance evidence.
- Any selected 3.3 V regulator must support measured worst-case combined load, transient and
  dropout margin across the product battery and thermal envelope.

## Output Format

When updating `hardware/BOM.csv`, include:

1. LCSC part number.
2. Current verified unit cost.
3. Stock status.
4. Assembly type.
5. Notes for concerns or alternatives.

The repository BOM is a design-level component list, not an exported NFF assembly BOM. Do not mark
cost, stock, JLCPCB classification, or footprint compatibility as verified without a current catalog
and EDA-footprint check.

Example:

```csv
MCU,U1,ESP32-S3 Module,1,Module,16MB/8MB/PCB Ant,ESP32-S3-WROOM-1-N16R8,C2913202,$3.45,In stock (5000+) - Basic Part
```

## Candidate Manufacturing Constraints

These are design targets to evaluate during HR1/HR2, not a released PCB specification:

- PCB target: 4 layers, about 80 mm diameter, 1.6 mm thickness, ENIG finish.
- Prefer single-sided SMT assembly.
- Double-sided SMT is acceptable when needed, with bottom side passives preferred.
- Avoid BGA packages.
- Through-hole should be limited to pogo pins or explicitly reviewed parts.
- Keep ESP32 antenna clearances: 15 mm ground clearance and no nearby copper/components at the
  antenna end.

## Common Issues

| Issue | Response |
| --- | --- |
| Part not found | Try manufacturer number without suffix, then package plus specs |
| Out of stock | Find pin-compatible alternatives and note restock date |
| Price too high | Search Basic Part alternatives and quantity breaks |
| Package mismatch | Do not substitute silently; flag footprint change |

Summaries should list sourced parts, external-source parts, risk items, and design concerns.
