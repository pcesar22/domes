# DOMES Development Board Engineering Specification

**Version:** 2.0
**Date:** 2026-01-09
**Status:** Ready for Outsourcing
**Datasheet Review:** Complete

---

## 1. Overview

This document specifies a development PCB that the ESP32-S3-DevKitC-1 v1.1 slots into, providing all peripherals needed for DOMES pod development.

### 1.1 Design Goals

- ESP32-S3-DevKitC-1 v1.1 plugs directly into female headers
- All DOMES peripherals accessible for testing
- JLCPCB PCBA service compatible
- Single-sided SMT (all SMD components on top)
- Test points for debugging
- **Fully outsourceable design** - all specifications complete

### 1.2 System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DOMES Development Board v2.0                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐  │
│   │                    ESP32-S3-DevKitC-1 v1.1                          │  │
│   │                    (plugs into J1/J2 headers)                       │  │
│   │                                                                     │  │
│   │   USB-C ◄─────────────────────────────────────────────────────────┐│  │
│   │                                                                    ││  │
│   │   3.3V ──┬──────────────────────────────────────────────────────┐ ││  │
│   │          │                                                      │ ││  │
│   │   5V ────┼─────────────────────────────────────┐                │ ││  │
│   │          │                                     │                │ ││  │
│   │   GND ───┼─────────────────────────────────────┼────────────────┼─┘│  │
│   │          │                                     │                │  │  │
│   └──────────┼─────────────────────────────────────┼────────────────┼──┘  │
│              │                                     │                │     │
│              │  ┌─────────────────────────────┐   │                │     │
│              │  │    74AHCT125 (U4)           │   │                │     │
│              │  │    Level Shifter            │   │                │     │
│   GPIO 38 ───┼──┤ 1A              1Y ├────────┼───┼─► D1 (LED)     │     │
│              │  │                    │        │   │                │     │
│              │  │ VCC=5V    GND     │        │   │                │     │
│              │  └────────────────────┘        │   │                │     │
│              │                                │   │                │     │
│              │  ┌─────────────────────────────┼───┼────────────────┤     │
│              │  │   LED Ring (D1-D16)         │   │                │     │
│              │  │   SK6812MINI-E x16          │   │                │     │
│              │  │   VDD=5V                    │   │                │     │
│              │  └─────────────────────────────┘   │                │     │
│              │                                     │                │     │
│   GPIO 8 ────┼──┬─────────────────────────────────┴────────────────┤     │
│   GPIO 9 ────┼──┤  I2C Bus (3.3V)                                  │     │
│              │  │  ┌──────────────────┐  ┌──────────────────┐      │     │
│              │  └──┤  LIS2DW12 (U1)   ├──┤  DRV2605L (U2)   │      │     │
│              │     │  IMU 0x19        │  │  Haptic 0x5A     │      │     │
│              │     └──────────────────┘  └────────┬─────────┘      │     │
│              │                                    │                │     │
│   GPIO 5 ────┼────────────────────────────────────┤ INT1           │     │
│              │                                    │                │     │
│              │                            ┌───────┴───────┐        │     │
│              │                            │  LRA Motor    │        │     │
│              │                            │  J4 Connector │        │     │
│              │                            └───────────────┘        │     │
│              │                                                     │     │
│   GPIO 11 ───┼──┬─────────────────────────────────────────────────┤     │
│   GPIO 12 ───┼──┤  I2S Bus (3.3V)                                  │     │
│   GPIO 13 ───┼──┤  ┌──────────────────┐                            │     │
│   GPIO 7  ───┼──┤  │  MAX98357A (U3)  │                            │     │
│              │  │  │  Audio Amp       │                            │     │
│              │  │  └────────┬─────────┘                            │     │
│              │  │           │                                      │     │
│              │  │   ┌───────┴───────┐                              │     │
│              │  │   │    Speaker    │                              │     │
│              │  │   │  J3 Connector │                              │     │
│              │  │   └───────────────┘                              │     │
│              │  │                                                  │     │
│   GPIO 1-4 ──┼──┴── Touch Pads (T1-T4)                             │     │
│              │                                                     │     │
└──────────────┴─────────────────────────────────────────────────────┴─────┘
```

---

## 2. Bill of Materials (BOM)

All parts verified in stock at JLCPCB/LCSC as of 2026-01-09.

### 2.1 Active Components

| Ref | Description | LCSC Part # | Manufacturer | Package | Qty | Stock | Basic |
|-----|-------------|-------------|--------------|---------|-----|-------|-------|
| U1 | LIS2DW12TR 3-axis Accelerometer | C189624 | STMicroelectronics | LGA-12 (2x2mm) | 1 | 10,463 | No |
| U2 | DRV2605LDGSR Haptic Driver | C527464 | Texas Instruments | MSOP-10 | 1 | 476 | No |
| U3 | MAX98357AETE+T I2S Audio Amp | C910544 | Analog Devices | TQFN-16 (3x3mm) | 1 | 15,884 | No |
| U4 | 74AHCT125PW Level Shifter | C7464 | NXP | TSSOP-14 | 1 | 46,283 | No |
| D1-D16 | SK6812MINI-E RGBW LED | C5149201 | OPSCO | 3535 | 16 | 161,209 | No |

### 2.2 Connectors

| Ref | Description | LCSC Part # | Package | Qty | Stock | Basic |
|-----|-------------|-------------|---------|-----|-------|-------|
| J1 | Female Header 1x20 2.54mm | C2905423 | TH | 1 | 9,345 | No |
| J2 | Female Header 1x20 2.54mm | C2905423 | TH | 1 | 9,345 | No |
| J3 | XH-2A 2P Vertical (Speaker) | C158012 | TH | 1 | 219,506 | No |
| J4 | XH-2A 2P Vertical (LRA) | C158012 | TH | 1 | 219,506 | No |

### 2.3 Passive Components

| Ref | Description | LCSC Part # | Package | Qty | Stock | Basic |
|-----|-------------|-------------|---------|-----|-------|-------|
| C1 | 100nF 16V X5R (U1 VDD) | C1525 | 0402 | 1 | 54M | **Yes** |
| C2 | 100nF 16V X5R (U1 VDD_IO) | C1525 | 0402 | 1 | 54M | **Yes** |
| C3 | 1µF 10V X5R (U2 REG pin) | C52923 | 0402 | 1 | 7.8M | **Yes** |
| C4 | 100nF 16V X5R (U4 VCC) | C1525 | 0402 | 1 | 54M | **Yes** |
| C5 | 10uF 10V X5R (U3 VDD) | C15525 | 0402 | 1 | 6.5M | **Yes** |
| C6 | 100nF 16V X5R (U3 VDD) | C1525 | 0402 | 1 | 54M | **Yes** |
| C7 | 10uF 16V X5R (LED 5V bulk) | C15525 | 0402 | 1 | 6.5M | **Yes** |
| C8-C11 | 100nF 16V X5R (LED bypass) | C1525 | 0402 | 4 | 54M | **Yes** |
| R1 | 4.7K 1% (I2C SDA pull-up) | C25900 | 0402 | 1 | 9.3M | **Yes** |
| R2 | 4.7K 1% (I2C SCL pull-up) | C25900 | 0402 | 1 | 9.3M | **Yes** |
| R3 | 330R 1% (LED data series) | C25105 | 0402 | 1 | 4.7M | **Yes** |
| R4 | 1M 1% (SD_MODE pull-up) | C26083 | 0402 | 1 | 4.2M | **Yes** |

### 2.4 Cost Summary

| Category | Cost |
|----------|------|
| Active ICs (5 types) | ~$4.50 |
| LEDs (16x SK6812) | ~$1.26 |
| Connectors (4x) | ~$0.50 |
| Passives (16x) | ~$0.10 |
| **Component Total** | **~$6.36** |
| PCB (5 pcs, 2-layer) | ~$2.00 |
| PCBA Assembly | ~$15.00 |
| **Total per Board** | **~$23.00** |

---

## 3. Detailed Schematic Specification

### 3.1 Power Distribution

```
                            POWER RAILS
═══════════════════════════════════════════════════════════════════════════

    DevKit J1 Pin 1 (3V3) ──────┬────────────────────────────────► 3.3V Rail
                                │
                                ├── U1 VDD (Pin 6)
                                ├── U1 VDD_IO (Pin 7)
                                ├── U2 VDD (Pin 1)
                                ├── U3 (NOT used - runs from 5V)
                                ├── R1 (I2C SDA pull-up)
                                └── R2 (I2C SCL pull-up)


    DevKit J1 Pin 2 (5V) ───────┬────────────────────────────────► 5V Rail
                                │
                                ├── U4 VCC (Pin 14)
                                ├── D1-D16 VDD (LED power)
                                ├── U3 VDD (Pin 4)
                                └── C7 (bulk capacitor)


    DevKit J1 Pin 3 (GND) ──────┴────────────────────────────────► GND Plane
                                │
                                ├── All IC GND pins
                                ├── All capacitor GND
                                └── U4 1OE (Pin 1) - tie to GND
```

### 3.2 U1 - LIS2DW12TR Accelerometer

**Datasheet:** [ST LIS2DW12](https://www.st.com/resource/en/datasheet/lis2dw12.pdf)
**I2C Address:** 0x19 (SA0=VDD)
**Package:** LGA-12 (2x2x0.7mm)

```
                    LIS2DW12TR (Top View - Pin 1 at dot)
                    ┌────────────────────────────────────┐
                    │                                    │
              Vdd_I/O│ 1                            12 │ INT2        ──► NC
                     │                                  │
                  Res│ 2                            11 │ INT1        ──► GPIO 5
                     │                                  │
                  SCL│ 3   ◄── GPIO 9 ──────────────────┤
                     │                                  │
                  SDA│ 4   ◄──► GPIO 8 ─────────────    │
                     │                              │  │
                  SA0│ 5   ◄── 3.3V (Addr=0x19) ────┼───┤
                     │                              │  │
                  Vdd│ 6   ◄── 3.3V + C1 (100nF) ───┼───┤
                     │                              │ 10│ CS   ◄── 3.3V (I2C mode)
                  GND│ 7   ◄── GND ─────────────────┼───┤
                     │                              │  9│ GND  ◄── GND
                Vdd_IO│ 8  ◄── 3.3V + C2 (100nF) ───┘  │
                     │                                  │
                     └──────────────────────────────────┘

CRITICAL CONNECTIONS:
- Pin 5 (SA0): Connect to VDD for address 0x19. DO NOT float.
- Pin 10 (CS): Connect to VDD for I2C mode. DO NOT float.
- Pin 2 (Res): Leave unconnected (reserved).
- Pin 12 (INT2): Leave unconnected if not used.

DECOUPLING:
- C1: 100nF between Pin 6 (VDD) and GND, place within 2mm of pin
- C2: 100nF between Pin 8 (VDD_IO) and GND, place within 2mm of pin
```

### 3.3 U2 - DRV2605LDGSR Haptic Driver

**Datasheet:** [TI DRV2605L](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)
**I2C Address:** 0x5A (fixed)
**Package:** MSOP-10 (3x3mm)

```
                    DRV2605LDGSR (Top View - Pin 1 at dot)
                    ┌────────────────────────────────────┐
                    │                                    │
                 REG│ 1  ──► C3 (1µF to GND)        10 │ OUT-       ──► J4 Pin 2 (LRA-)
                    │                                  │
                 VDD│ 2  ◄── 3.3V                     9 │ OUT+       ──► J4 Pin 1 (LRA+)
                    │                                  │
                 SCL│ 3  ◄── GPIO 9 ──────────────    8 │ VDD        ◄── 3.3V
                    │                             │   │
                 SDA│ 4  ◄──► GPIO 8 ─────────────┤   7 │ GND        ◄── GND
                    │                             │   │
               IN/TRIG│ 5 ◄── NC (or GPIO for PWM)│   6 │ EN         ◄── 3.3V (always on)
                    │                             │   │              or GPIO for power control
                    └─────────────────────────────────┘

CRITICAL CONNECTIONS:
- Pin 1 (REG): 1.8V regulator output - REQUIRES 1µF capacitor (C3) to GND
- Pin 5 (IN/TRIG): Leave NC for I2C-only control, or connect to GPIO for PWM input.
- Pin 6 (EN): Connect to VDD for always-on. Connect to GPIO for power management.
- Pins 2 and 8: Both are VDD - connect both to 3.3V.

MOTOR CONNECTION (J4):
- Pin 9 (OUT+) → J4 Pin 1
- Pin 10 (OUT-) → J4 Pin 2
- LRA Motor: 10mm diameter, ~3V rated, e.g., G1040003D

DECOUPLING:
- C3: 1µF between Pin 1 (REG) and GND, place within 3mm of pin
```

### 3.4 U3 - MAX98357AETE+T Audio Amplifier

**Datasheet:** [Analog Devices MAX98357A](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf)
**Package:** TQFN-16 (3x3x0.75mm, 0.5mm pitch)

```
                    MAX98357AETE+T (Top View - Pin 1 at dot)
                    ┌────────────────────────────────────┐
                    │                                    │
             SD_MODE│ 1  ◄── R4 (1M to 5V)          16 │ OUTN       ──► J3 Pin 2 (SPK-)
                    │      + GPIO 7 (for shutdown)     │
                GAIN│ 2  ◄── NC (9dB default)       15 │ OUTP       ──► J3 Pin 1 (SPK+)
                    │                                  │
                 GND│ 3  ◄── GND                    14 │ GND        ◄── GND
                    │                                  │
                 VDD│ 4  ◄── 5V + C5 (10uF)         13 │ VDD        ◄── 5V
                    │      + C6 (100nF)                │
                 DIN│ 5  ◄── GPIO 13 ──────────     12 │ GND        ◄── GND
                    │                          │      │
               BCLK│ 6  ◄── GPIO 12 ───────────┤   11 │ GND        ◄── GND
                    │                          │      │
               LRCLK│ 7  ◄── GPIO 11 ──────────┤   10 │ GND        ◄── GND
                    │                          │      │
                 GND│ 8  ◄── GND               │    9 │ GND        ◄── GND
                    │                          │      │
                    │      ┌────────────────┐  │      │
                    │      │   Exposed Pad  │◄─┴──────┤◄── GND (CRITICAL!)
                    │      │   (EP) - GND   │         │
                    │      └────────────────┘         │
                    └─────────────────────────────────┘

CRITICAL CONNECTIONS:
- Exposed Pad (EP): MUST connect to GND for thermal dissipation!
- Pin 1 (SD_MODE): 1M resistor to 5V + optional GPIO 7 for shutdown control
  * With 1M to 5V: Outputs (L+R)/2 stereo mix
  * GPIO 7 LOW: Shutdown mode
  * GPIO 7 HIGH/float with 1M: Active
- Pin 2 (GAIN): NC = 9dB (default). GND = 12dB. 100K to GND = 15dB.
- Pins 4 and 13: Both are VDD - connect both to 5V.
- Pins 3, 8, 9, 10, 11, 12, 14: All GND - connect all to ground plane.

SPEAKER CONNECTION (J3):
- Pin 15 (OUTP) → J3 Pin 1
- Pin 16 (OUTN) → J3 Pin 2
- Speaker: 4Ω minimum, 8Ω recommended, 0.5-1W, 23mm diameter

DECOUPLING:
- C5: 10uF between Pin 4 (VDD) and GND, place within 3mm of pin
- C6: 100nF between Pin 4 (VDD) and GND, place within 2mm of pin
```

### 3.5 U4 - 74AHCT125PW Level Shifter

**Datasheet:** [NXP 74AHCT125](https://www.nxp.com/docs/en/data-sheet/74AHC_AHCT125.pdf)
**Package:** TSSOP-14 (5x4.4mm)

```
                    74AHCT125PW (Top View - Pin 1 at dot)
                    ┌────────────────────────────────────┐
                    │                                    │
                 1OE│ 1  ◄── GND (always enabled)   14 │ VCC        ◄── 5V + C4 (100nF)
                    │                                  │
                  1A│ 2  ◄── GPIO 38 ───────────── 13 │ 4OE        ◄── 3.3V (disabled)
                    │                                  │
                  1Y│ 3  ──► R3 (330R) ──► D1 DIN  12 │ 4A         ◄── NC
                    │                                  │
                 2OE│ 4  ◄── 3.3V (disabled)       11 │ 4Y         ──► NC
                    │                                  │
                  2A│ 5  ◄── NC                    10 │ 3OE        ◄── 3.3V (disabled)
                    │                                  │
                  2Y│ 6  ──► NC                     9 │ 3A         ◄── NC
                    │                                  │
                 GND│ 7  ◄── GND                    8 │ 3Y         ──► NC
                    │                                  │
                    └──────────────────────────────────┘

CRITICAL CONNECTIONS:
- Pin 14 (VCC): Connect to 5V - this sets output high level to 5V
- Pin 1 (1OE): Connect to GND to enable buffer 1 (active low)
- Pins 4, 10, 13 (2OE, 3OE, 4OE): Connect to 3.3V to disable unused buffers
- Pin 2 (1A): 3.3V logic input from ESP32 GPIO 38
- Pin 3 (1Y): 5V logic output to LED data line

WHY 74AHCT WORKS FOR LEVEL SHIFTING:
- AHCT has TTL-compatible inputs: VIL < 0.8V, VIH > 2.0V
- 3.3V input is reliably recognized as HIGH
- Output swings to full VCC (5V) rail
- Fast propagation delay (~7ns) handles 800kHz LED data

DECOUPLING:
- C4: 100nF between Pin 14 (VCC) and GND, place within 2mm of pin
```

### 3.6 LED Ring - D1-D16 SK6812MINI-E

**Datasheet:** [SK6812MINI](https://cdn-shop.adafruit.com/product-files/2686/SK6812MINI_REV.01-1-2.pdf)
**Package:** 3535 (3.5x3.5x1.5mm)

```
                    SK6812MINI-E (Top View)
                    ┌────────────────────────────────────┐
                    │                                    │
                DOUT│ 1   ───► Next LED DIN         4 │ DIN        ◄── Prev LED DOUT
                    │        (or NC for D16)           │              (or R3 for D1)
                 GND│ 2   ◄── GND                    3 │ VDD        ◄── 5V
                    │                                  │
                    └──────────────────────────────────┘

LED CHAIN WIRING:
    R3 (330R) ──► D1.DIN → D1.DOUT ──► D2.DIN → D2.DOUT ──► ... ──► D16.DIN → D16.DOUT (NC)

POWER DISTRIBUTION:
    5V Rail ──┬── D1.VDD + C8 (100nF)
              ├── D2.VDD
              ├── D3.VDD
              ├── D4.VDD + C9 (100nF)    ← Bypass every 4 LEDs
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

CRITICAL NOTES:
- 330Ω series resistor (R3) on data line prevents reflections
- VDD range: 4.5V to 5.5V - must use 5V, not 3.3V
- Logic HIGH threshold: 0.7 × VDD = 3.5V - requires level shifter from 3.3V MCU
- Bypass capacitor every 4 LEDs or when power traces are long
- C7 (10uF bulk cap) at start of LED power rail

LED RING LAYOUT (16 LEDs, 60mm ring diameter):
- Angular spacing: 360° / 16 = 22.5° per LED
- LED orientation: All DIN/DOUT aligned for daisy-chain routing
- Data flow: Clockwise from D1 (nearest to U4) to D16
```

### 3.7 I2C Bus

```
                    I2C BUS TOPOLOGY
═══════════════════════════════════════════════════════════════════════════

                          3.3V
                           │
                    ┌──────┴──────┐
                    │             │
                   R1            R2
                  4.7K          4.7K
                    │             │
    GPIO 8 (SDA) ───┼─────────────┼───┬───────────────┬─────────────────────
                    │             │   │               │
                    │             │   │ SDA           │ SDA
                    │             │   ▼               ▼
                    │             │  ┌───────┐      ┌───────┐
                    │             │  │  U1   │      │  U2   │
    GPIO 9 (SCL) ───┼─────────────┼───┤LIS2DW12│     │DRV2605L│
                                  │   │ 0x19  │      │ 0x5A  │
                                  │   └───────┘      └───────┘
                                  │   │ SCL           │ SCL
                                  └───┴───────────────┴─────────────────────

SPECIFICATIONS:
- Bus speed: 400kHz (Fast Mode) - both devices support this
- Pull-up resistors: 4.7kΩ to 3.3V (R1, R2)
- Maximum bus capacitance: 400pF
- Trace length: Keep < 50mm total

I2C ADDRESSES:
- U1 (LIS2DW12): 0x19 (7-bit) when SA0 = VDD
- U2 (DRV2605L): 0x5A (7-bit) fixed
```

### 3.8 I2S Audio Bus

```
                    I2S BUS TOPOLOGY
═══════════════════════════════════════════════════════════════════════════

    GPIO 12 (BCLK)  ────────────────────────────────► U3 Pin 6 (BCLK)
    GPIO 11 (LRCLK) ────────────────────────────────► U3 Pin 7 (LRCLK)
    GPIO 13 (DIN)   ────────────────────────────────► U3 Pin 5 (DIN)
    GPIO 7  (SD)    ────┬───────────────────────────► U3 Pin 1 (SD_MODE)
                        │
                        R4 (1M to 5V)

SPECIFICATIONS:
- Sample rate: 16kHz (sufficient for feedback sounds)
- Bit depth: 16-bit
- Format: Standard I2S (Philips format)
- MCLK: Not required (MAX98357A has internal PLL)

SD_MODE BEHAVIOR:
- R4 creates ~0.5V when GPIO 7 is floating → Active, (L+R)/2 output
- GPIO 7 = LOW → Shutdown mode (< 2.7μA standby)
- GPIO 7 = HIGH → Active, (L+R)/2 output
```

### 3.9 Capacitive Touch Pads

```
                    TOUCH PAD LAYOUT
═══════════════════════════════════════════════════════════════════════════

    GPIO 1 ────────────────────────────────► Touch Pad T1 (15x15mm copper)
    GPIO 2 ────────────────────────────────► Touch Pad T2 (15x15mm copper)
    GPIO 3 ────────────────────────────────► Touch Pad T3 (15x15mm copper)
    GPIO 4 ────────────────────────────────► Touch Pad T4 (15x15mm copper)

PAD DESIGN REQUIREMENTS:
- Material: Bare copper (ENIG or HASL finish)
- Size: 15mm x 15mm minimum
- Shape: Square or circular
- Spacing: 5mm minimum between pads
- Guard ring: 1mm GND ring around each pad, 2mm gap from pad
- No soldermask over pad area
- Traces: Route on bottom layer, 0.3mm width, avoid running under other pads

GROUND POUR:
- Use hatched ground pour (25% fill) under touch area
- Solid ground pour elsewhere
- Hatching reduces parasitic capacitance for better sensitivity
```

---

## 4. PCB Design Specification

### 4.1 Board Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Dimensions** | 100mm x 80mm | Rectangular with rounded corners (r=3mm) |
| **Layers** | 2 | Top: Signal + Components, Bottom: GND pour |
| **Thickness** | 1.6mm | Standard |
| **Copper Weight** | 1 oz (35μm) | Both layers |
| **Surface Finish** | HASL Lead-Free | Or ENIG for touch pads |
| **Soldermask** | Green | Any color acceptable |
| **Silkscreen** | White | Top side only |
| **Min Trace/Space** | 0.2mm / 0.2mm | 8 mil / 8 mil |
| **Min Via** | 0.3mm drill, 0.6mm pad | Standard via |

### 4.2 Layer Stackup

```
    ┌──────────────────────────────────────┐
    │  TOP LAYER (Signal + Components)     │  35μm copper
    ├──────────────────────────────────────┤
    │  FR4 Core (1.5mm)                    │  Dielectric
    ├──────────────────────────────────────┤
    │  BOTTOM LAYER (GND Pour)             │  35μm copper
    └──────────────────────────────────────┘
```

### 4.3 Component Placement

```
    ┌────────────────────────────────────────────────────────────────────────┐
    │ (0,0)                                                         (100,0) │
    │                                                                        │
    │   ┌──────┐                                              ┌──────┐      │
    │   │  T1  │                                              │  T2  │      │
    │   │15x15 │                                              │15x15 │      │
    │   └──────┘ (10,10)                              (75,10) └──────┘      │
    │                                                                        │
    │        ┌────────────────────────────────────────────┐                  │
    │        │            J1                J2            │                  │
    │        │   ○ ○ ○ ○ ○ ○ ○ ○ ○ ○    ○ ○ ○ ○ ○ ○ ○ ○ ○ ○   │                  │
    │        │                                            │                  │
    │        │        ESP32-S3-DevKitC-1 v1.1             │                  │
    │        │            (51.5mm x 20mm)                 │                  │
    │        │                                            │                  │
    │        │   ○ ○ ○ ○ ○ ○ ○ ○ ○ ○    ○ ○ ○ ○ ○ ○ ○ ○ ○ ○   │                  │
    │        │                                            │ (75,25)          │
    │ (25,25)└────────────────────────────────────────────┘                  │
    │                                                                        │
    │                    LED RING (60mm diameter)                            │
    │              ┌───────────────────────────────┐                         │
    │              │    D1   D2   D3   D4   D5     │                         │
    │              │  D16                      D6  │                         │
    │              │  D15         ●           D7  │ Center: (50, 50)         │
    │              │  D14                      D8  │                         │
    │              │    D13  D12  D11  D10  D9    │                         │
    │              └───────────────────────────────┘                         │
    │                                                                        │
    │   ┌────┐   ┌────┐   ┌────┐   ┌────┐                                   │
    │   │ U1 │   │ U2 │   │ U3 │   │ U4 │        (ICs near bottom edge)    │
    │   │IMU │   │HAP │   │AMP │   │LVL │                                   │
    │   └────┘   └────┘   └────┘   └────┘                                   │
    │  (15,65)  (30,65)  (45,65)  (60,65)                                   │
    │                                                                        │
    │   ┌──────┐        ┌────┐  ┌────┐           ┌──────┐                   │
    │   │  T3  │        │ J3 │  │ J4 │           │  T4  │                   │
    │   │15x15 │        │SPK │  │LRA │           │15x15 │                   │
    │   └──────┘        └────┘  └────┘           └──────┘                   │
    │  (10,70)         (35,72) (50,72)          (75,70)                     │
    │                                                                        │
    │ (0,80)                                                       (100,80) │
    └────────────────────────────────────────────────────────────────────────┘

ALL COORDINATES IN MILLIMETERS FROM BOTTOM-LEFT ORIGIN
```

### 4.4 Critical Routing Requirements

#### Power Traces
| Net | Width | Notes |
|-----|-------|-------|
| 5V | 0.8mm min | Supplies LEDs (up to 1A peak) |
| 3.3V | 0.5mm min | Low current |
| GND | Pour | Solid pour on bottom layer |

#### Signal Traces
| Net | Width | Length | Notes |
|-----|-------|--------|-------|
| LED_DATA (U4 → D1) | 0.3mm | < 30mm | Route directly, include R3 inline |
| I2C_SDA | 0.25mm | < 50mm total | Route with SCL, matched length |
| I2C_SCL | 0.25mm | < 50mm total | Route with SDA |
| I2S_BCLK | 0.25mm | < 30mm | Keep parallel with LRCLK, DIN |
| I2S_LRCLK | 0.25mm | < 30mm | Keep parallel with BCLK, DIN |
| I2S_DIN | 0.25mm | < 30mm | Keep parallel with BCLK, LRCLK |
| TOUCH_1-4 | 0.3mm | < 50mm | Route on bottom layer |

#### Routing Rules
1. **LED data line:** Route on top layer, keep away from I2S traces
2. **I2C bus:** Route together, minimize stubs
3. **I2S bus:** Route as a group, avoid crossing LED data
4. **Touch traces:** Route on bottom layer under ground pour opening
5. **No traces under DevKit antenna area** (USB end of DevKit)

### 4.5 Via Requirements

| Type | Drill | Pad | Usage |
|------|-------|-----|-------|
| Standard | 0.3mm | 0.6mm | Signal vias |
| Power | 0.4mm | 0.8mm | 5V distribution to LEDs |
| Thermal | 0.3mm | 0.6mm | U3 exposed pad (4-6 vias) |

### 4.6 Fiducials and Tooling

| Feature | Location | Size |
|---------|----------|------|
| Fiducial 1 | (5, 5) | 1mm copper, 2mm mask opening |
| Fiducial 2 | (95, 5) | 1mm copper, 2mm mask opening |
| Fiducial 3 | (5, 75) | 1mm copper, 2mm mask opening |
| Mounting Hole 1 | (3, 3) | 3.2mm, non-plated |
| Mounting Hole 2 | (97, 3) | 3.2mm, non-plated |
| Mounting Hole 3 | (3, 77) | 3.2mm, non-plated |
| Mounting Hole 4 | (97, 77) | 3.2mm, non-plated |

---

## 5. Test Points

| TP# | Net | Location | Purpose |
|-----|-----|----------|---------|
| TP1 | 3.3V | Near J1 | Power verification |
| TP2 | 5V | Near J1 | Power verification |
| TP3 | GND | Center | Ground reference |
| TP4 | LED_DATA | After R3 | LED signal debug |
| TP5 | I2C_SDA | Near R1 | Bus monitoring |
| TP6 | I2C_SCL | Near R2 | Bus monitoring |
| TP7 | I2S_BCLK | Near U3 | Audio debug |
| TP8 | SD_MODE | At U3 Pin 1 | Amp shutdown debug |

Test point style: 1.5mm circular pad, no soldermask

---

## 6. Silkscreen Requirements

### 6.1 Required Markings

```
Top Silkscreen:
- Board name: "DOMES DevBoard v2.0"
- Reference designators for all components (U1, U2, U3, U4, J1-J4, D1-D16)
- Pin 1 indicators for ICs (dot or line)
- Polarity marks for LEDs (arrow showing data direction)
- Connector labels: "SPK" for J3, "LRA" for J4
- Touch pad labels: "T1", "T2", "T3", "T4"
- DevKit orientation indicator (arrow pointing to USB end)
- JLCPCB order number location box (8x4mm)

Bottom Silkscreen:
- Board name and version
- Date code location
- Designer/company info (optional)
```

### 6.2 Assembly Orientation Marks

- All IC Pin 1 locations clearly marked
- LED D1 marked as "START"
- LED chain direction shown with arrows
- DevKit USB orientation shown

---

## 7. Assembly Notes for Manufacturer

### 7.1 Component Placement

1. **SMT Components:** All on top side
2. **Through-Hole Components:** J1, J2, J3, J4 on top side
3. **LED Orientation:** DIN/DOUT must follow chain direction (critical!)
4. **IC Orientation:** Check Pin 1 marks carefully
5. **U3 (MAX98357A):** Exposed pad MUST be soldered to GND

### 7.2 Soldering Profile

- Standard lead-free reflow: Peak 250°C
- LEDs: Limit to 260°C peak, 10 seconds max
- Through-hole: Wave solder or hand solder after SMT

### 7.3 Quality Checks

- [ ] Verify all ICs correctly oriented (Pin 1)
- [ ] Verify LED chain direction (D1→D16)
- [ ] Check for solder bridges on fine-pitch ICs (U1, U3)
- [ ] Confirm U3 exposed pad is soldered
- [ ] Inspect through-hole joints (J1-J4)

---

## 8. Design Files Deliverables

For outsourcing, provide the following:

| File | Format | Description |
|------|--------|-------------|
| Schematic | PDF | Full schematic with net names |
| Schematic | KiCad/EasyEDA | Native format for editing |
| PCB Layout | Gerber RS-274X | Manufacturing files |
| PCB Layout | KiCad/EasyEDA | Native format for editing |
| BOM | CSV/Excel | With LCSC part numbers |
| Pick & Place | CSV | Component positions and rotations |
| Assembly Drawing | PDF | Component placement reference |
| This Spec | PDF/Markdown | Complete specification |

---

## 9. Validation Checklist

### 9.1 Pre-Fabrication Review

- [ ] All component footprints verified against datasheets
- [ ] All LCSC part numbers verified in stock
- [ ] Net connectivity verified (ERC check pass)
- [ ] Design rules check (DRC) pass
- [ ] Silkscreen legible and complete
- [ ] Fiducials and tooling holes present

### 9.2 Pre-Assembly Review

- [ ] BOM matches schematic
- [ ] All parts orientation documented
- [ ] Critical component notes included
- [ ] Test points accessible

### 9.3 Post-Assembly Test

- [ ] Visual inspection pass
- [ ] Power rail voltages correct (3.3V ±5%, 5V ±10%)
- [ ] No shorts between rails
- [ ] I2C devices respond (0x19, 0x5A)
- [ ] LEDs light up with test pattern
- [ ] Audio output functional
- [ ] Haptic motor vibrates
- [ ] Touch pads detect touch

---

## 10. Revision History

| Rev | Date | Author | Changes |
|-----|------|--------|---------|
| 1.0 | 2026-01-08 | Claude | Initial specification |
| 2.0 | 2026-01-09 | Claude | Datasheet review complete. Added pin-level connections, corrected level shifter topology, added assembly notes. Ready for outsourcing. |

---

## 11. References

- [ESP32-S3-DevKitC-1 Schematic](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/)
- [LIS2DW12 Datasheet](https://www.st.com/resource/en/datasheet/lis2dw12.pdf)
- [LIS2DW12 Application Note AN5038](https://www.st.com/resource/en/application_note/an5038-lis2dw12-alwayson-3axis-accelerometer-stmicroelectronics.pdf)
- [DRV2605L Datasheet](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)
- [MAX98357A Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf)
- [SK6812MINI-E Datasheet](https://cdn-shop.adafruit.com/product-files/2686/SK6812MINI_REV.01-1-2.pdf)
- [74AHCT125 Datasheet](https://www.nxp.com/docs/en/data-sheet/74AHC_AHCT125.pdf)
- [Adafruit DRV2605L Guide](https://learn.adafruit.com/adafruit-drv2605-haptic-controller-breakout/)
- [Adafruit MAX98357A Guide](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/)
