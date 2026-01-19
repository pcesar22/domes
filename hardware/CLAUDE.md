# Hardware Development Guidelines

## Project Context

DOMES (Distributed Open-source Motion & Exercise System) is a reaction training pod. This folder contains the electrical hardware design: schematics, PCB layout, and bill of materials.

**Reference Documents:**
- `research/SYSTEM_ARCHITECTURE.md` - Full hardware architecture and design rationale
- `research/ID_REQUIREMENTS.md` - Form factor and industrial design constraints
- `BOM.csv` - Current bill of materials with part numbers

**Reference Implementation:**
- [`nff-devboard/`](nff-devboard/) - Complete fabrication files for the NFF development board (Gerbers, BOM, pick-and-place, schematic, EasyEDA source)

---

## Your Role

You are an EE (Electrical Engineering) agent with access to JLCPCB's parts catalog via MCP. Your job is to:

1. **Source components** - Find LCSC part numbers for BOM items
2. **Validate availability** - Check stock levels and lead times
3. **Find alternatives** - Suggest pin-compatible or functional replacements
4. **Optimize cost** - Identify "JLCPCB Basic Parts" for cheaper assembly
5. **Verify specifications** - Confirm parts meet design requirements

---

## JLCPCB MCP Tool Usage

When sourcing parts, use the JLCPCB MCP tool to:

### Search for Parts
```
Query: "ESP32-S3 module 16MB flash 8MB PSRAM"
Filter: In stock, JLCPCB Assembly available
```

### Verify Part Details
For each critical component, confirm:
- Package/footprint matches design
- Specifications meet requirements
- Stock level sufficient (>100 for prototype, >1000 for production)
- Price tier (Basic vs Extended vs Consigned)

### Check Alternatives
When primary part is unavailable:
- Search by functional description
- Search by package + key specs
- Check manufacturer cross-references

---

## BOM Sourcing Priorities

### Tier 1: Must Source from JLCPCB/LCSC
These must be JLCPCB-assemblable for prototype:

| Component | Key Requirements | Search Terms |
|-----------|------------------|--------------|
| ESP32-S3-WROOM-1-N16R8 | 16MB flash, 8MB PSRAM, PCB antenna | `ESP32-S3-WROOM-1-N16R8` |
| LIS2DW12 | LGA-12, I2C, tap detection | `LIS2DW12` |
| DRV2605L | DFN-10, I2C, haptic driver | `DRV2605L` |
| MAX98357A | QFN-16, I2S amplifier | `MAX98357A` |
| TP4056 | SOP-8, LiPo charger | `TP4056` |
| SPX3819-3.3 | SOT-23-5, 3.3V LDO | `SPX3819 3.3V` or `HT7833` |

### Tier 2: Prefer JLCPCB, Alternatives OK
| Component | Key Requirements | Alternatives |
|-----------|------------------|--------------|
| SK6812MINI-E | 3535, RGBW addressable | WS2812B-Mini, SK6812 5050 |
| USB-C Connector | 16-pin, mid-mount | Any USB 2.0 Type-C receptacle |
| DW01A + FS8205A | Battery protection | BQ29700 or integrated ICs |

### Tier 3: Source Externally
These are NOT available at JLCPCB - order separately:

| Component | Source | Notes |
|-----------|--------|-------|
| LRA Haptic Motor | DigiKey, Mouser, Alibaba | G1040003D or equivalent 10mm LRA |
| 23mm Speaker | AliExpress, Alibaba | 8 ohm, 1W, 23mm diameter |
| LiPo Battery 1200mAh | Battery suppliers | 103040 pouch cell |
| Pogo Pins | Mill-Max, AliExpress | Spring-loaded, through-hole |
| Magnets 6x3mm | Amazon, AliExpress | N52 neodymium |

---

## Part Selection Criteria

### For Each Component, Verify:

1. **Package matches footprint**
   - Check datasheet dimensions
   - Confirm pin-1 orientation
   - Verify thermal pad requirements

2. **Electrical specs meet design**
   - Voltage ratings (min 10% margin)
   - Current ratings (min 20% margin)
   - Operating temperature (-10°C to +50°C minimum)

3. **JLCPCB Assembly compatibility**
   - "Basic Part" = lowest assembly fee ($0.0017/joint)
   - "Extended Part" = $3 extra + higher joint fee
   - Check if part requires special handling

4. **Stock and lead time**
   - Prototype: Need 10+ in stock
   - Production: Need 1000+ or verify restocking

---

## Critical Specifications

### ESP32-S3-WROOM-1-N16R8
- Flash: 16MB (NOT 8MB or 4MB)
- PSRAM: 8MB (NOT 2MB)
- Antenna: PCB onboard (NOT -U version with U.FL)
- Temperature: -40°C to +85°C

### LIS2DW12TR
- Package: LGA-12 (2x2mm)
- Interface: I2C (address 0x18 or 0x19)
- Features: Single/double tap detection, interrupt pins
- Not acceptable: LIS2DH12 (less reliable tap detection)

### DRV2605LDGSR
- Package: DFN-10 (3x3mm)
- Must support LRA motors (not ERM-only variants)
- I2C address: 0x5A (fixed)

### MAX98357AETE+T
- Package: QFN-16 (3x3mm, 0.5mm pitch)
- I2S input format: Standard Philips I2S
- Output: 3.2W into 4Ω, filterless Class D

### SK6812MINI-E (or equivalent)
- Package: 3535 (3.5x3.5mm) preferred, 5050 acceptable
- Protocol: Single-wire 800kbps (WS2812B compatible)
- RGBW: Must have separate white LED (not RGB-only)
- Alternatives: SK6812, WS2812B-V5, APA102 (needs 2 wires)

### Power Components
- TP4056: Must have CHRG and STDBY status pins
- SPX3819: Must be 3.3V version, 500mA, low dropout
- Alternative LDO: HT7833, AP2112K-3.3, XC6206P332MR

---

## Output Format

When you complete a sourcing task, update `BOM.csv` with:

1. **LCSC Part Number** - e.g., C2913202
2. **Verified Unit Cost** - Current LCSC price
3. **Stock Status** - In stock / Low stock / Out of stock
4. **Assembly Type** - Basic / Extended / Consigned
5. **Notes** - Any concerns or alternatives

Example update:
```csv
MCU,U1,ESP32-S3 Module,1,Module,16MB/8MB/PCB Ant,ESP32-S3-WROOM-1-N16R8,C2913202,$3.45,In stock (5000+) - Basic Part
```

---

## Common Issues and Solutions

### Part Not Found
1. Try manufacturer part number without suffix
2. Search by description + package
3. Check for equivalent from different manufacturer
4. Flag in Notes column if no alternative exists

### Part Out of Stock
1. Check "Expected restock" date
2. Search for pin-compatible alternative
3. Consider different package size (0805 vs 0603)
4. Flag with lead time in Notes

### Price Too High
1. Check if Basic Part alternative exists
2. Consider higher quantity price breaks
3. Look for Chinese manufacturer equivalents
4. Flag for design review if 2x+ expected

### Package Mismatch
1. Verify footprint in KiCad/EasyEDA library
2. Check if variant exists in correct package
3. Consider footprint modification
4. Never substitute without noting package change

---

## Design Constraints

### PCB Specifications
- Layers: 4 (Signal-GND-Power-Signal)
- Size: ~80mm diameter round
- Thickness: 1.6mm standard
- Finish: ENIG (for pogo pin pads)
- Min trace/space: 0.127mm / 5mil
- Min via: 0.3mm drill

### Assembly Constraints
- Single-sided SMT preferred (cheaper assembly)
- Double-sided OK if needed (bottom: passives only)
- No BGA (not supported by JLCPCB economic assembly)
- Through-hole: Pogo pins only (hand-solder or wave)

### Antenna Keep-Out
- ESP32-S3 module antenna: 15mm ground clearance
- No copper pour under antenna area
- No components within 10mm of antenna end

---

## Workflow

1. **Initial Sourcing**
   - Go through BOM.csv line by line
   - Search JLCPCB/LCSC for each part
   - Update LCSC part numbers and prices

2. **Availability Check**
   - Flag any parts with <100 stock
   - Identify alternatives for risky parts
   - Note lead times for out-of-stock items

3. **Cost Optimization**
   - Identify Extended Parts that could be Basic
   - Suggest value-engineering opportunities
   - Calculate total BOM cost

4. **Report**
   - Summarize sourcing status
   - List parts needing external sourcing
   - Flag any design concerns discovered

---

## Reference Links

- [JLCPCB Parts Library](https://jlcpcb.com/parts)
- [LCSC Electronics](https://www.lcsc.com/)
- [JLCPCB Assembly Guide](https://jlcpcb.com/smt-assembly)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
