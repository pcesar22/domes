# Development Board - Schematic & PCB Design Specification

**Document Version:** 1.0
**Date:** 2026-01-09

---

## Project Overview

Design a 2-layer development board that serves as a carrier/breakout for an ESP32-S3 development kit. The board provides LED feedback, motion sensing, haptic feedback, audio output, and capacitive touch inputs.

**Deliverables Required:**
1. Schematic (PDF + native format)
2. PCB layout (Gerber + native format)
3. BOM with LCSC part numbers
4. Pick & place file (CSV)

---

## 1. Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 100mm × 80mm |
| Shape | Rectangle with 3mm corner radius |
| Layers | 2 |
| Thickness | 1.6mm |
| Copper | 1 oz (35µm) both layers |
| Surface Finish | HASL lead-free |
| Min Trace/Space | 0.2mm / 0.2mm |
| Min Via | 0.3mm drill, 0.6mm pad |

---

## 2. Bill of Materials

### 2.1 Integrated Circuits

| Ref | Description | LCSC # | Package | Qty |
|-----|-------------|--------|---------|-----|
| U1 | LIS2DW12TR - 3-axis accelerometer | C189624 | LGA-12 (2×2mm) | 1 |
| U2 | DRV2605LDGSR - Haptic motor driver | C527464 | MSOP-10 | 1 |
| U3 | MAX98357AETE+T - I2S audio amplifier | C910544 | TQFN-16 (3×3mm) | 1 |
| U4 | 74AHCT125PW - Quad buffer/level shifter | C7464 | TSSOP-14 | 1 |

### 2.2 LEDs

| Ref | Description | LCSC # | Package | Qty |
|-----|-------------|--------|---------|-----|
| D1-D16 | SK6812MINI-E - Addressable RGBW LED | C5149201 | 3535 | 16 |

### 2.3 Electromechanical Components (On-Board)

| Ref | Description | LCSC # | Package | Qty |
|-----|-------------|--------|---------|-----|
| SPK1 | GSPK2307P-8R1W - 23mm 8Ω 1W Speaker | C530531 | 23mm round, wire leads | 1 |
| MOT1 | LD0832AA-0099F - LRA Vibration Motor | C2682305 | 8×3.2mm, wire leads | 1 |

**Note:** Speaker and LRA motor are mounted directly on-board for complete testing without external components.

### 2.4 Connectors

| Ref | Description | LCSC # | Package | Qty |
|-----|-------------|--------|---------|-----|
| J1 | Female header 1×20, 2.54mm pitch | C2905423 | Through-hole | 1 |
| J2 | Female header 1×20, 2.54mm pitch | C2905423 | Through-hole | 1 |
| J3 | JST XH 2-pin vertical (Speaker backup) | C158012 | Through-hole | 1 |
| J4 | JST XH 2-pin vertical (LRA backup) | C158012 | Through-hole | 1 |

**Note:** J3 and J4 are backup connectors for testing alternative speakers/motors.

### 2.5 Capacitors (0402, X5R)

| Ref | Value | Voltage | LCSC # | Qty |
|-----|-------|---------|--------|-----|
| C1, C2, C4, C6, C8-C11 | 100nF | 16V | C1525 | 8 |
| C3 | 1µF | 10V | C52923 | 1 |
| C5, C7 | 10µF | 10V | C15525 | 2 |

### 2.6 Resistors (0402, 1%)

| Ref | Value | LCSC # | Qty |
|-----|-------|--------|-----|
| R1, R2 | 4.7kΩ | C25900 | 2 |
| R3 | 330Ω | C25105 | 1 |
| R4 | 1MΩ | C26083 | 1 |

---

## 3. Schematic - Block Diagram

```
                              ┌─────────────────────────────┐
                              │     MCU Development Kit     │
                              │   (plugs into J1 and J2)    │
                              │                             │
    ┌─────────────────────────┤  3.3V  5V  GND             │
    │                         │                             │
    │  Power                  │  GPIO pins used:           │
    │  ──────                 │    38 - LED data           │
    │  3.3V rail ◄────────────┤     8 - I2C SDA            │
    │  5V rail ◄──────────────┤     9 - I2C SCL            │
    │  GND plane ◄────────────┤    11 - I2S LRCLK          │
    │                         │    12 - I2S BCLK           │
    │                         │    13 - I2S DIN            │
    │                         │     7 - Audio shutdown     │
    │                         │     5 - IMU interrupt      │
    │                         │   1-4 - Touch pads         │
    │                         └─────────────────────────────┘
    │
    │
    ├───► U4 (74AHCT125) Level Shifter ───► D1-D16 (LED Ring)
    │         VCC = 5V
    │
    ├───► U1 (LIS2DW12) Accelerometer
    │         I2C address: 0x19
    │
    ├───► U2 (DRV2605L) Haptic Driver ───► J4 (Motor connector)
    │         I2C address: 0x5A
    │
    ├───► U3 (MAX98357A) Audio Amp ───► J3 (Speaker connector)
    │         I2S input
    │
    └───► T1-T4 (Touch pads) - Bare copper areas
```

---

## 4. Detailed Schematic - U1 (LIS2DW12TR)

**Function:** 3-axis accelerometer with tap detection
**Interface:** I2C at address 0x19
**Datasheet:** https://www.st.com/resource/en/datasheet/lis2dw12.pdf

```
                    LIS2DW12TR (LGA-12, Top View)
                    Pin 1 indicator at corner dot

                    ┌──────────────────────────┐
                    │  ●                       │
             Pin 1  │ VDD_IO            INT2   │ Pin 12  → NC
                    │                          │
             Pin 2  │ RES (NC)          INT1   │ Pin 11  → Net: IMU_INT (GPIO 5)
                    │                          │
             Pin 3  │ SCL               CS     │ Pin 10  → 3.3V
                    │   ↑                      │
             Pin 4  │ SDA               GND    │ Pin 9   → GND
                    │   ↑↓                     │
             Pin 5  │ SA0             VDD_IO   │ Pin 8   → 3.3V + C2
                    │   ↑                      │
             Pin 6  │ VDD               GND    │ Pin 7   → GND
                    │   ↑                      │
                    └──────────────────────────┘

Connections:
- Pin 1 (VDD_IO): 3.3V
- Pin 3 (SCL): Net I2C_SCL (to GPIO 9)
- Pin 4 (SDA): Net I2C_SDA (to GPIO 8)
- Pin 5 (SA0): 3.3V (sets address to 0x19)
- Pin 6 (VDD): 3.3V
- Pin 7, 9 (GND): Ground
- Pin 8 (VDD_IO): 3.3V
- Pin 10 (CS): 3.3V (enables I2C mode)
- Pin 11 (INT1): Net IMU_INT (optional, to GPIO 5)
- Pin 12 (INT2): No connection

Capacitors:
- C1: 100nF between Pin 6 (VDD) and GND, place <2mm from pin
- C2: 100nF between Pin 8 (VDD_IO) and GND, place <2mm from pin
```

---

## 5. Detailed Schematic - U2 (DRV2605LDGSR)

**Function:** Haptic motor driver with built-in effects library
**Interface:** I2C at address 0x5A (fixed)
**Datasheet:** https://www.ti.com/lit/ds/symlink/drv2605l.pdf

```
                    DRV2605LDGSR (MSOP-10, Top View)
                    Pin 1 indicator at corner dot

                    ┌──────────────────────────┐
                    │  ●                       │
             Pin 1  │ REG               OUT-   │ Pin 10  → J4 Pin 2 (Motor -)
                    │   ↓                      │
             Pin 2  │ VDD               OUT+   │ Pin 9   → J4 Pin 1 (Motor +)
                    │   ↑                      │
             Pin 3  │ SCL               VDD    │ Pin 8   → 3.3V
                    │   ↑                      │
             Pin 4  │ SDA               GND    │ Pin 7   → GND
                    │   ↑↓                     │
             Pin 5  │ IN/TRIG           EN     │ Pin 6   → 3.3V
                    │                          │
                    └──────────────────────────┘

Connections:
- Pin 1 (REG): C3 (1µF to GND) - internal 1.8V regulator output
- Pin 2 (VDD): 3.3V
- Pin 3 (SCL): Net I2C_SCL (to GPIO 9)
- Pin 4 (SDA): Net I2C_SDA (to GPIO 8)
- Pin 5 (IN/TRIG): No connection (I2C mode only)
- Pin 6 (EN): 3.3V (always enabled)
- Pin 7 (GND): Ground
- Pin 8 (VDD): 3.3V
- Pin 9 (OUT+): J4 Pin 1
- Pin 10 (OUT-): J4 Pin 2

Capacitors:
- C3: 1µF between Pin 1 (REG) and GND, place <3mm from pin

On-board LRA Motor (MOT1):
- Part: LD0832AA-0099F (LCSC C2682305)
- Specs: 8×3.2mm Linear Resonant Actuator, 1.8V rated, 80mA
- Mounting: Solder pads for wire leads, motor adhered to PCB

Backup connector J4:
- 2-pin JST XH connector for testing alternative motors
- Parallels MOT1 - both can be connected simultaneously
```

---

## 6. Detailed Schematic - U3 (MAX98357AETE+T)

**Function:** I2S digital audio to Class-D amplifier
**Interface:** I2S (no MCLK required)
**Datasheet:** https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf

```
                    MAX98357AETE+T (TQFN-16, Top View)
                    Pin 1 indicator at corner dot
                    EXPOSED PAD ACTIVE - Must connect to GND!

                    ┌──────────────────────────┐
                    │  ●                       │
             Pin 1  │ SD_MODE           OUTN   │ Pin 16  → J3 Pin 2 (Speaker -)
                    │   ↑                      │
             Pin 2  │ GAIN              OUTP   │ Pin 15  → J3 Pin 1 (Speaker +)
                    │                          │
             Pin 3  │ GND               GND    │ Pin 14  → GND
                    │                          │
             Pin 4  │ VDD               VDD    │ Pin 13  → 5V
                    │   ↑                      │
             Pin 5  │ DIN               GND    │ Pin 12  → GND
                    │   ↑                      │
             Pin 6  │ BCLK              GND    │ Pin 11  → GND
                    │   ↑                      │
             Pin 7  │ LRCLK             GND    │ Pin 10  → GND
                    │   ↑                      │
             Pin 8  │ GND               GND    │ Pin 9   → GND
                    │                          │
                    │    ┌──────────────┐      │
                    │    │  EXPOSED PAD │      │
                    │    │     (EP)     │ ─────┼──→ GND (CRITICAL!)
                    │    └──────────────┘      │
                    └──────────────────────────┘

Connections:
- Pin 1 (SD_MODE): R4 (1MΩ to 5V) + Net AMP_SD (to GPIO 7)
- Pin 2 (GAIN): No connection (default 9dB gain)
- Pin 3, 8, 9, 10, 11, 12, 14 (GND): Ground
- Pin 4, 13 (VDD): 5V
- Pin 5 (DIN): Net I2S_DIN (to GPIO 13)
- Pin 6 (BCLK): Net I2S_BCLK (to GPIO 12)
- Pin 7 (LRCLK): Net I2S_LRCLK (to GPIO 11)
- Pin 15 (OUTP): J3 Pin 1
- Pin 16 (OUTN): J3 Pin 2
- Exposed Pad: GND with thermal vias (4-6 vias, 0.3mm)

Capacitors:
- C5: 10µF between Pin 4 (VDD) and GND, place <3mm from pin
- C6: 100nF between Pin 4 (VDD) and GND, place <2mm from pin

SD_MODE behavior:
- R4 (1MΩ) pulls SD_MODE to ~0.5V via internal 100kΩ pulldown
- GPIO 7 LOW = shutdown mode
- GPIO 7 HIGH or floating = active, outputs (L+R)/2 stereo mix

On-board Speaker (SPK1):
- Part: GSPK2307P-8R1W (LCSC C530531)
- Specs: 23mm diameter, 8Ω, 1W
- Mounting: Solder pads for wire leads, speaker mounted in center of LED ring

Backup connector J3:
- 2-pin JST XH connector for testing alternative speakers
- Parallels SPK1 - both can be connected simultaneously
```

---

## 7. Detailed Schematic - U4 (74AHCT125PW)

**Function:** 3.3V to 5V level shifter for LED data
**Datasheet:** https://www.nxp.com/docs/en/data-sheet/74AHC_AHCT125.pdf

```
                    74AHCT125PW (TSSOP-14, Top View)
                    Pin 1 indicator at corner dot

                    ┌──────────────────────────┐
                    │  ●                       │
             Pin 1  │ 1OE               VCC    │ Pin 14  → 5V + C4
                    │   ↓                      │
             Pin 2  │ 1A                4OE    │ Pin 13  → 3.3V (disable)
                    │   ↑                      │
             Pin 3  │ 1Y                4A     │ Pin 12  → NC
                    │   ↓                      │
             Pin 4  │ 2OE               4Y     │ Pin 11  → NC
                    │                          │
             Pin 5  │ 2A                3OE    │ Pin 10  → 3.3V (disable)
                    │                          │
             Pin 6  │ 2Y                3A     │ Pin 9   → NC
                    │                          │
             Pin 7  │ GND               3Y     │ Pin 8   → NC
                    │                          │
                    └──────────────────────────┘

Connections:
- Pin 1 (1OE): GND (active low - enables buffer 1)
- Pin 2 (1A): Net LED_DATA_3V3 (from GPIO 38)
- Pin 3 (1Y): R3 (330Ω) → Net LED_DATA_5V → D1 DIN
- Pin 4 (2OE): 3.3V (disable buffer 2)
- Pin 5-6 (2A, 2Y): No connection
- Pin 7 (GND): Ground
- Pin 8-9 (3Y, 3A): No connection
- Pin 10 (3OE): 3.3V (disable buffer 3)
- Pin 11-12 (4Y, 4A): No connection
- Pin 13 (4OE): 3.3V (disable buffer 4)
- Pin 14 (VCC): 5V

Capacitors:
- C4: 100nF between Pin 14 (VCC) and GND, place <2mm from pin

Why this works for level shifting:
- AHCT family has TTL-compatible inputs (VIH > 2.0V)
- 3.3V input is recognized as logic HIGH
- Output swings to full VCC (5V)
```

---

## 8. Detailed Schematic - LED Ring (D1-D16)

**Component:** SK6812MINI-E addressable RGBW LED
**Datasheet:** https://cdn-shop.adafruit.com/product-files/2686/SK6812MINI_REV.01-1-2.pdf

```
                    SK6812MINI-E (3535 package, Top View)
                    Identify Pin 1 by notched corner or marking

                    ┌──────────────────────────┐
                    │                          │
             Pin 1  │ DOUT              DIN    │ Pin 4
                    │                          │
             Pin 2  │ GND               VDD    │ Pin 3
                    │                          │
                    └──────────────────────────┘

LED Chain:
    U4 Pin 3 → R3 (330Ω) → D1.DIN
    D1.DOUT → D2.DIN
    D2.DOUT → D3.DIN
    ...
    D15.DOUT → D16.DIN
    D16.DOUT → NC

Power Distribution:
    5V Rail + C7 (10µF bulk) ──┬── D1.VDD + C8 (100nF)
                               ├── D2.VDD
                               ├── D3.VDD
                               ├── D4.VDD + C9 (100nF)
                               ├── D5.VDD
                               ├── D6.VDD
                               ├── D7.VDD
                               ├── D8.VDD + C10 (100nF)
                               ├── D9.VDD
                               ├── D10.VDD
                               ├── D11.VDD
                               ├── D12.VDD + C11 (100nF)
                               ├── D13.VDD
                               ├── D14.VDD
                               ├── D15.VDD
                               └── D16.VDD

    All D1-D16.GND → Ground plane

LED Ring Layout:
- Arrange 16 LEDs in a circle, 60mm diameter
- Angular spacing: 22.5° between LEDs
- Orient all LEDs with DIN/DOUT aligned for easy daisy-chain routing
- Data flows clockwise from D1 to D16
```

---

## 9. Detailed Schematic - I2C Bus

```
                          3.3V
                           │
                    ┌──────┴──────┐
                    │             │
                   R1            R2
                  4.7kΩ         4.7kΩ
                    │             │
                    │             │
    Net I2C_SDA ────┼─────────────┼───┬──── U1 Pin 4 (SDA)
    (GPIO 8)        │             │   │
                    │             │   └──── U2 Pin 4 (SDA)
                    │             │
    Net I2C_SCL ────┼─────────────┼───┬──── U1 Pin 3 (SCL)
    (GPIO 9)                      │   │
                                  │   └──── U2 Pin 3 (SCL)
                                  │
                                 GND

I2C Parameters:
- Speed: 400kHz (Fast Mode)
- Pull-ups: 4.7kΩ to 3.3V
- Max trace length: 50mm total
```

---

## 10. Detailed Schematic - Touch Pads

```
    GPIO 1 ─────────────────────────► Touch Pad T1
    GPIO 2 ─────────────────────────► Touch Pad T2
    GPIO 3 ─────────────────────────► Touch Pad T3
    GPIO 4 ─────────────────────────► Touch Pad T4

Touch Pad Design:
- Size: 15mm × 15mm bare copper
- No soldermask over pad
- Guard ring: 1mm ground ring around each pad with 2mm gap
- Route traces on bottom layer
```

---

## 11. Detailed Schematic - Power Distribution

```
    J1 Pin 1 (3.3V from MCU board) ───┬─── U1 VDD (Pin 6)
                                      ├─── U1 VDD_IO (Pin 1, Pin 8)
                                      ├─── U1 SA0 (Pin 5)
                                      ├─── U1 CS (Pin 10)
                                      ├─── U2 VDD (Pin 2, Pin 8)
                                      ├─── U2 EN (Pin 6)
                                      ├─── R1 (I2C pull-up)
                                      ├─── R2 (I2C pull-up)
                                      ├─── U4 Pin 4 (2OE - disable)
                                      ├─── U4 Pin 10 (3OE - disable)
                                      └─── U4 Pin 13 (4OE - disable)


    J1 Pin 2 (5V from MCU board) ─────┬─── U4 VCC (Pin 14) + C4
                                      ├─── U3 VDD (Pin 4, Pin 13) + C5, C6
                                      ├─── D1-D16 VDD + C7, C8-C11
                                      └─── R4 (to SD_MODE)


    J1 Pin 3 (GND) ───────────────────┴─── Ground plane (bottom layer)
                                          All component GND pins
                                          U4 Pin 1 (1OE - enable)
                                          U3 Exposed Pad
```

---

## 12. Connector J1/J2 - MCU Board Socket

```
    J1 (1×20 female header)              J2 (1×20 female header)
    Left side of MCU board               Right side of MCU board

    ┌─────┐                              ┌─────┐
    │  1  │ 3.3V ◄─────────────────────  │  1  │
    │  2  │ 5V ◄───────────────────────  │  2  │
    │  3  │ GND ◄──────────────────────  │  3  │
    │  4  │                              │  4  │
    │  5  │                              │  5  │
    │  6  │ GPIO 1 (Touch T1) ◄────────  │  6  │
    │  7  │ GPIO 2 (Touch T2) ◄────────  │  7  │
    │  8  │ GPIO 3 (Touch T3) ◄────────  │  8  │
    │  9  │ GPIO 4 (Touch T4) ◄────────  │  9  │
    │ 10  │ GPIO 5 (IMU INT) ◄─────────  │ 10  │
    │ 11  │                              │ 11  │
    │ 12  │ GPIO 7 (AMP_SD) ◄──────────  │ 12  │
    │ 13  │ GPIO 8 (I2C SDA) ◄─────────  │ 13  │
    │ 14  │ GPIO 9 (I2C SCL) ◄─────────  │ 14  │
    │ 15  │                              │ 15  │
    │ 16  │ GPIO 11 (I2S LRCLK) ◄──────  │ 16  │
    │ 17  │ GPIO 12 (I2S BCLK) ◄───────  │ 17  │
    │ 18  │ GPIO 13 (I2S DIN) ◄────────  │ 18  │
    │ 19  │                              │ 19  │
    │ 20  │ GPIO 38 (LED DATA) ◄───────  │ 20  │
    └─────┘                              └─────┘

Note: Exact pin positions depend on MCU board pinout.
The designer should verify against the specific MCU board datasheet.
MCU board dimensions: approximately 51.5mm × 20mm
Header spacing: Match MCU board pin spacing (2.54mm pitch, ~46mm between rows)
```

---

## 13. PCB Layout Requirements

### 13.1 Component Placement

```
    ┌────────────────────────────────────────────────────────────────────────┐
    │                                                              (100, 80) │
    │                                                                        │
    │   ┌──────┐                                              ┌──────┐      │
    │   │  T1  │                                              │  T2  │      │
    │   │      │                                              │      │      │
    │   └──────┘                                              └──────┘      │
    │    (10,70)                                              (75,70)       │
    │                                                                        │
    │        ┌────────────────────────────────────────────┐                  │
    │        │   J1 ○○○○○○○○○○        ○○○○○○○○○○ J2       │                  │
    │        │                                            │                  │
    │        │         MCU Board Socket Area              │                  │
    │        │           (51.5mm × 20mm)                  │                  │
    │        │                                            │                  │
    │        │       ○○○○○○○○○○        ○○○○○○○○○○         │                  │
    │        └────────────────────────────────────────────┘                  │
    │         (25,45)                              (75,45)                   │
    │                                                                        │
    │                    LED RING (60mm diameter)                            │
    │                  ┌─────────────────────────┐                           │
    │                  │  D1  D2  D3  D4  D5     │                           │
    │                  │D16   ┌─────────┐    D6  │                           │
    │                  │D15   │  SPK1   │    D7  │ Center: (50, 40)          │
    │                  │D14   │  23mm   │    D8  │ Speaker in ring center    │
    │                  │  D13 └─D12─D11─┘ D10 D9 │                           │
    │                  └─────────────────────────┘                           │
    │                          ┌───┐                                         │
    │                          │MOT│ MOT1: 8×3.2mm LRA (50, 32)              │
    │                          └───┘                                         │
    │   ┌────┐   ┌────┐   ┌────┐   ┌────┐                                   │
    │   │ U1 │   │ U2 │   │ U3 │   │ U4 │                                   │
    │   └────┘   └────┘   └────┘   └────┘                                   │
    │   (15,12)  (30,12)  (50,12)  (70,12)                                  │
    │                                                                        │
    │   ┌──────┐        ┌────┐  ┌────┐           ┌──────┐                   │
    │   │  T3  │        │ J3 │  │ J4 │           │  T4  │                   │
    │   │      │        │bkp │  │bkp │           │      │                   │
    │   └──────┘        └────┘  └────┘           └──────┘                   │
    │    (10,5)         (35,5)  (55,5)            (75,5)                    │
    │                                                                        │
    │ (0,0)                                                                  │
    └────────────────────────────────────────────────────────────────────────┘

    Coordinates in mm from bottom-left origin

    SPEAKER (SPK1): 23mm speaker in center of LED ring, wire leads to pads near U3
    LRA MOTOR (MOT1): 8×3.2mm motor below LED ring, wire leads to pads near U2
    BACKUP CONNECTORS (J3, J4): For testing alternative speakers/motors
```

### 13.2 Layer Assignment

| Layer | Content |
|-------|---------|
| Top | Signal traces, all SMD components, through-hole components |
| Bottom | Ground pour (solid), touch pad traces only |

### 13.3 Trace Widths

| Net Type | Width | Notes |
|----------|-------|-------|
| 5V power | 0.8mm minimum | High current for LEDs |
| 3.3V power | 0.5mm minimum | |
| Signal traces | 0.25mm | I2C, I2S, GPIO |
| LED data | 0.3mm | Include series resistor R3 inline |

### 13.4 Critical Routing Rules

1. **Ground pour:** Solid copper pour on bottom layer, connect to all GND pins with vias
2. **LED data:** Route on top layer, keep away from I2S traces
3. **I2C bus:** Route SDA and SCL together, minimize stubs
4. **I2S bus:** Route BCLK, LRCLK, DIN as a group
5. **Touch traces:** Route on bottom layer only, through ground pour opening
6. **Decoupling caps:** Place within specified distance of IC pins
7. **U3 thermal pad:** Connect to ground with 4-6 thermal vias (0.3mm)

### 13.5 Keep-Out Zones

- No traces under MCU board antenna area (check MCU datasheet)
- 2mm clearance around touch pads (for guard ring)

### 13.6 Via Specifications

| Type | Drill | Pad | Use |
|------|-------|-----|-----|
| Signal | 0.3mm | 0.6mm | General routing |
| Power | 0.4mm | 0.8mm | 5V distribution |
| Thermal | 0.3mm | 0.6mm | U3 exposed pad |

---

## 14. Mechanical Features

### 14.1 Mounting Holes

| Location | Size | Type |
|----------|------|------|
| (3, 3) | 3.2mm | Non-plated through-hole |
| (97, 3) | 3.2mm | Non-plated through-hole |
| (3, 77) | 3.2mm | Non-plated through-hole |
| (97, 77) | 3.2mm | Non-plated through-hole |

### 14.2 Fiducials

| Location | Size |
|----------|------|
| (5, 5) | 1mm copper circle, 2mm soldermask opening |
| (95, 5) | 1mm copper circle, 2mm soldermask opening |
| (5, 75) | 1mm copper circle, 2mm soldermask opening |

### 14.3 Board Outline

- Rectangle 100mm × 80mm
- Corner radius: 3mm on all corners

---

## 15. Test Points

| Designator | Net | Purpose |
|------------|-----|---------|
| TP1 | 3.3V | Power verification |
| TP2 | 5V | Power verification |
| TP3 | GND | Ground reference |
| TP4 | LED_DATA_5V | Post-level-shifter signal |
| TP5 | I2C_SDA | Bus debugging |
| TP6 | I2C_SCL | Bus debugging |
| TP7 | I2S_BCLK | Audio debugging |
| TP8 | AMP_SD | Amp enable signal |

Test point style: 1.5mm circular pad, no soldermask

---

## 16. Silkscreen Requirements

**Top side:**
- Reference designators: U1, U2, U3, U4, D1-D16, J1-J4, R1-R4, C1-C11, T1-T4, TP1-TP8
- Pin 1 indicators for all ICs
- LED data direction arrows (D1 → D16)
- Connector labels: "SPK" at J3, "MOTOR" at J4
- Board name and version
- JLCPCB order number box (8mm × 4mm)

**Bottom side:**
- Board name and version
- Revision date

---

## 17. Design Verification Checklist

Before submitting:
- [ ] All LCSC part numbers verified and in stock
- [ ] All footprints match component datasheets
- [ ] ERC (Electrical Rules Check) passes
- [ ] DRC (Design Rules Check) passes
- [ ] All decoupling capacitors placed near IC pins
- [ ] Ground pour connected to all GND pins
- [ ] U3 exposed pad has thermal vias to ground
- [ ] LED chain direction correct (D1 → D16)
- [ ] Touch pads have guard rings
- [ ] Test points accessible
- [ ] Fiducials present for assembly

---

## 18. Deliverables Checklist

Please provide:
- [ ] Schematic PDF
- [ ] Schematic source file (KiCad/EasyEDA/Altium)
- [ ] PCB Gerber files (RS-274X)
- [ ] PCB source file
- [ ] BOM spreadsheet with LCSC part numbers
- [ ] Pick and place file (CSV with X, Y, rotation)
- [ ] Assembly drawing PDF
- [ ] 3D render (optional)

---

## 19. Contact for Questions

[Your contact information here]

---

**End of Specification**
