# DOMES Development Board Engineering Specification

**Version:** 1.0
**Date:** 2026-01-08
**Status:** Draft

## 1. Overview

This document specifies a development PCB that the ESP32-S3-DevKitC-1 v1.1 slots into, providing all peripherals needed for DOMES pod development. The board allows firmware iteration without requiring the full production PCB.

### 1.1 Design Goals

- ESP32-S3-DevKitC-1 v1.1 plugs directly into female headers
- All DOMES peripherals accessible for testing
- JLCPCB assembly compatible (PCBA service)
- Single-sided SMT where possible
- Test points for debugging

### 1.2 Block Diagram

```
                    ┌─────────────────────────────────────────┐
                    │        DOMES Development Board          │
                    │                                         │
    ┌───────────┐   │  ┌──────────────────────────────────┐  │
    │  USB-C    │───┼──│  ESP32-S3-DevKitC-1 v1.1         │  │
    │  (DevKit) │   │  │  (plugs into female headers)     │  │
    └───────────┘   │  │                                  │  │
                    │  │  GPIO 38 ──► Level Shift ──► LEDs│  │
                    │  │  GPIO 8/9 ──► I2C Bus           │  │
                    │  │  GPIO 11/12/13 ──► I2S Audio    │  │
                    │  │  GPIO 1-4 ──► Touch Pads        │  │
                    │  │  3.3V/GND ──► Power Rails       │  │
                    │  └──────────────────────────────────┘  │
                    │                                         │
                    │  ┌─────────┐  ┌─────────┐  ┌─────────┐ │
                    │  │ SK6812  │  │LIS2DW12 │  │DRV2605L │ │
                    │  │ 16x LED │  │  IMU    │  │ Haptic  │ │
                    │  │  Ring   │  │  I2C    │  │  I2C    │ │
                    │  └─────────┘  └─────────┘  └─────────┘ │
                    │                                         │
                    │  ┌─────────┐  ┌─────────┐  ┌─────────┐ │
                    │  │MAX98357 │  │ Speaker │  │   LRA   │ │
                    │  │Audio Amp│  │Connector│  │  Motor  │ │
                    │  │  I2S    │  │   JST   │  │Connector│ │
                    │  └─────────┘  └─────────┘  └─────────┘ │
                    │                                         │
                    │  ┌────────────────────────────────────┐ │
                    │  │     4x Capacitive Touch Pads       │ │
                    │  └────────────────────────────────────┘ │
                    └─────────────────────────────────────────┘
```

---

## 2. Bill of Materials (BOM)

All parts verified in stock at JLCPCB/LCSC as of 2026-01-08.

### 2.1 Active Components

| Ref | Description | LCSC Part # | Manufacturer | Package | Qty | Unit Cost | Stock | Basic |
|-----|-------------|-------------|--------------|---------|-----|-----------|-------|-------|
| U1 | LIS2DW12TR Accelerometer | C189624 | STMicroelectronics | LGA-12 (2x2mm) | 1 | $0.55 | 10,463 | No |
| U2 | DRV2605LDGSR Haptic Driver | C527464 | Texas Instruments | MSOP-10 | 1 | $1.35 | 476 | No |
| U3 | MAX98357AETE+T Audio Amp | C910544 | Maxim Integrated | TQFN-16 (3x3mm) | 1 | $1.19 | 15,884 | No |
| U4 | 74AHCT125S14-13 Level Shifter | C842299 | Diodes Inc | SO-14 | 1 | $0.14 | 2,121 | No |
| D1-D16 | SK6812MINI-E RGBW LED | C5149201 | NORMAND | 3535 | 16 | $0.079 | 161,209 | No |

### 2.2 Connectors

| Ref | Description | LCSC Part # | Package | Qty | Unit Cost | Stock | Basic |
|-----|-------------|-------------|---------|-----|-----------|-------|-------|
| J1, J2 | Female Header 1x20 2.54mm | C2905423 | TH P=2.54mm | 2 | $0.15 | 9,345 | No |
| J3 | XH-2.54 2P Connector (Speaker) | C7429641 | TH P=2.5mm | 1 | $0.03 | 157,481 | No |
| J4 | PH-2.0 2P Connector (LRA) | C18077744 | TH P=2mm | 1 | $0.03 | 150,017 | No |

### 2.3 Passive Components (Basic Parts - No Extended Fee)

| Ref | Description | LCSC Part # | Package | Qty | Unit Cost | Stock | Basic |
|-----|-------------|-------------|---------|-----|-----------|-------|-------|
| C1-C4 | 100nF MLCC 16V | C1525 | 0402 | 4 | $0.002 | 54M | Yes |
| C5-C6 | 10uF MLCC 10V | C15525 | 0402 | 2 | $0.005 | 6.5M | Yes |
| C7 | 4.7uF MLCC 10V | C23733 | 0402 | 1 | $0.004 | 4.7M | Yes |
| R1-R2 | 10K Ohm 1% | C25744 | 0402 | 2 | $0.002 | 20M | Yes |
| R3-R4 | 4.7K Ohm 1% | C25900 | 0402 | 2 | $0.002 | 9.3M | Yes |
| R5 | 330 Ohm 1% (LED data) | C25105 | 0402 | 1 | $0.002 | 4.7M | Yes |

### 2.4 Estimated BOM Cost

| Category | Cost |
|----------|------|
| Active ICs | ~$5.00 |
| LEDs (16x) | ~$1.26 |
| Connectors | ~$0.40 |
| Passives | ~$0.05 |
| **Total** | **~$6.71** |

*Note: PCB fabrication and assembly fees additional (~$2-5 for prototypes at 5 qty)*

---

## 3. Schematic Design

### 3.1 Power Distribution

```
DevKit 3.3V ────┬──── VCC (all ICs)
                │
                └──── 100nF decoupling at each IC

DevKit 5V ──────┬──── VLED (SK6812 power rail)
                │
                └──── Level shifter VCC-B (high side)

DevKit GND ─────┴──── Common ground plane
```

**Notes:**
- SK6812 LEDs operate at 5V for full brightness
- Level shifter required: 3.3V GPIO → 5V LED data
- No additional LDO needed (DevKit provides regulated power)

### 3.2 LED Ring (SK6812MINI-E x16)

```
                        74AHCT125 Level Shifter
                        ┌─────────────────────┐
GPIO 38 (DevKit) ──────►│ 1A          1Y      │──── 330Ω ──► D1 DIN
                        │                      │
3.3V ──────────────────►│ VCC-A               │
5V ────────────────────►│ VCC-B               │
GND ───────────────────►│ GND                 │
                        │ 1OE (tie to GND)    │
                        └─────────────────────┘

D1 DOUT → D2 DIN → D3 DIN → ... → D16 DIN (daisy chain)

Power: 5V rail with 100nF + 10uF bulk capacitor
```

**LED Arrangement:** Circular ring, 16 LEDs evenly spaced

### 3.3 I2C Bus (Shared)

```
                    ┌───────────────┐
GPIO 8 (SDA) ──┬───►│ LIS2DW12      │ (Addr: 0x19 with SA0=HIGH)
               │    │ Accelerometer │
               │    └───────────────┘
               │    ┌───────────────┐
               └───►│ DRV2605L      │ (Addr: 0x5A fixed)
                    │ Haptic Driver │
                    └───────────────┘

                    ┌───────────────┐
GPIO 9 (SCL) ──┬───►│ LIS2DW12      │
               │    └───────────────┘
               │    ┌───────────────┐
               └───►│ DRV2605L      │
                    └───────────────┘

Pull-ups: 4.7K to 3.3V on SDA and SCL
```

### 3.4 IMU - LIS2DW12

```
┌─────────────────────────────────────────┐
│  LIS2DW12TR (LGA-12)                    │
│                                         │
│  VDD ◄── 3.3V + 100nF                   │
│  VDD_IO ◄── 3.3V + 100nF                │
│  GND ◄── GND                            │
│                                         │
│  SDA ◄──► GPIO 8 (I2C Data)             │
│  SCL ◄── GPIO 9 (I2C Clock)             │
│  SA0 ◄── 3.3V (Address = 0x19)          │
│                                         │
│  INT1 ──► GPIO 5 (optional tap detect)  │
│  INT2 ──► NC                            │
│  CS ◄── 3.3V (I2C mode enable)          │
└─────────────────────────────────────────┘
```

### 3.5 Haptic Driver - DRV2605L

```
┌─────────────────────────────────────────┐
│  DRV2605L (MSOP-10)                     │
│                                         │
│  VDD ◄── 3.3V + 100nF                   │
│  GND ◄── GND                            │
│                                         │
│  SDA ◄──► GPIO 8 (I2C Data)             │
│  SCL ◄── GPIO 9 (I2C Clock)             │
│                                         │
│  IN/TRIG ◄── NC (I2C mode)              │
│  EN ◄── 3.3V (always enabled)           │
│                                         │
│  OUT+ ──► J4 Pin 1 (LRA +)              │
│  OUT- ──► J4 Pin 2 (LRA -)              │
└─────────────────────────────────────────┘

LRA Motor: 10mm diameter, ~3V rated (e.g., G1040003D)
```

### 3.6 Audio Amplifier - MAX98357A

```
┌─────────────────────────────────────────┐
│  MAX98357AETE+T (TQFN-16)               │
│                                         │
│  VDD ◄── 3.3V + 4.7uF + 100nF           │
│  GND ◄── GND (EP connected)             │
│                                         │
│  BCLK ◄── GPIO 12 (I2S Bit Clock)       │
│  LRCLK ◄── GPIO 11 (I2S Word Select)    │
│  DIN ◄── GPIO 13 (I2S Data)             │
│                                         │
│  SD_MODE ◄── GPIO 7 (shutdown control)  │
│              HIGH = enabled             │
│              LOW/float = shutdown       │
│                                         │
│  GAIN ◄── GND (15dB default gain)       │
│                                         │
│  OUT+ ──► J3 Pin 1 (Speaker +)          │
│  OUT- ──► J3 Pin 2 (Speaker -)          │
└─────────────────────────────────────────┘

Speaker: 23mm, 8 ohm, 0.5-1W
```

### 3.7 Touch Pads

```
GPIO 1 ──► Touch Pad 1 (exposed copper area)
GPIO 2 ──► Touch Pad 2
GPIO 3 ──► Touch Pad 3
GPIO 4 ──► Touch Pad 4

Notes:
- Pads are bare copper areas on PCB top layer
- Size: ~15x15mm each recommended
- No additional components (ESP32-S3 has built-in touch sensing)
- Add guard ring (ground pour) around each pad
- May add series 1K resistor for ESD protection (optional)
```

---

## 4. PCB Layout Guidelines

### 4.1 Board Dimensions

- **Target Size:** 100mm x 80mm (fits development/testing)
- **Layers:** 2-layer (4-layer optional for better EMI)
- **Minimum Trace/Space:** 0.15mm (JLCPCB standard)
- **Via Size:** 0.3mm drill / 0.6mm pad

### 4.2 Component Placement

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  ┌──────┐                                      ┌──────┐      │
│  │Touch1│                                      │Touch2│      │
│  └──────┘                                      └──────┘      │
│                                                              │
│         ┌────────────────────────────────┐                   │
│         │                                │                   │
│         │   ESP32-S3-DevKitC-1 v1.1      │                   │
│         │   (Female Headers J1, J2)      │                   │
│         │                                │                   │
│         └────────────────────────────────┘                   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              SK6812 LED Ring (D1-D16)                │   │
│  │                     ◯ ◯ ◯ ◯                          │   │
│  │                   ◯         ◯                        │   │
│  │                  ◯           ◯                       │   │
│  │                  ◯           ◯                       │   │
│  │                   ◯         ◯                        │   │
│  │                     ◯ ◯ ◯ ◯                          │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                     │
│  │ U1   │  │ U2   │  │ U3   │  │ U4   │                     │
│  │IMU   │  │Haptic│  │Audio │  │Level │                     │
│  └──────┘  └──────┘  └──────┘  └──────┘                     │
│                                                              │
│  ┌──────┐                                      ┌──────┐      │
│  │Touch3│     [J3 Speaker]  [J4 LRA Motor]     │Touch4│      │
│  └──────┘                                      └──────┘      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 4.3 Routing Guidelines

1. **Ground Plane:** Solid ground pour on bottom layer
2. **Power Routing:**
   - 3.3V: 0.5mm trace minimum
   - 5V (LED): 0.8mm trace minimum (handles 1A)
3. **LED Data:**
   - Keep short, direct routing
   - Series 330Ω resistor close to first LED
4. **I2C:**
   - Route together, <50mm total length
   - Place pull-ups near DevKit
5. **I2S Audio:**
   - Route BCLK, LRCLK, DIN together
   - Keep away from LED data line
6. **Touch Pads:**
   - Guard ring around each pad
   - Hatched ground pour (reduce capacitance)

### 4.4 JLCPCB Assembly Notes

- All components on **top side** only
- Mark Pin 1 orientation clearly in silkscreen
- Use JLCPCB extended parts library for non-basic parts
- Provide clear fiducial marks

---

## 5. GPIO Mapping

### 5.1 DevKitC-1 v1.1 Pinout Used

| DevKit Pin | GPIO | Function | Direction | Notes |
|------------|------|----------|-----------|-------|
| 3V3 | - | Power | PWR | 3.3V regulated |
| 5V | - | Power | PWR | USB 5V |
| GND | - | Ground | PWR | Multiple pins |
| GPIO 38 | 38 | LED Data | OUT | DevKit onboard LED |
| GPIO 8 | 8 | I2C SDA | I/O | Shared bus |
| GPIO 9 | 9 | I2C SCL | OUT | Shared bus |
| GPIO 11 | 11 | I2S LRCLK | OUT | Word select |
| GPIO 12 | 12 | I2S BCLK | OUT | Bit clock |
| GPIO 13 | 13 | I2S DOUT | OUT | Audio data |
| GPIO 7 | 7 | AMP_SD | OUT | Amp shutdown |
| GPIO 5 | 5 | IMU_INT1 | IN | Tap detect (optional) |
| GPIO 1 | 1 | TOUCH_1 | IN | Capacitive touch |
| GPIO 2 | 2 | TOUCH_2 | IN | Capacitive touch |
| GPIO 3 | 3 | TOUCH_3 | IN | Capacitive touch |
| GPIO 4 | 4 | TOUCH_4 | IN | Capacitive touch |

### 5.2 Production PCB Differences

| Function | DevKit GPIO | Production GPIO | Notes |
|----------|-------------|-----------------|-------|
| LED Data | GPIO 38 | GPIO 14 | Different RMT channel |

---

## 6. Test Points

| TP# | Signal | Purpose |
|-----|--------|---------|
| TP1 | 3.3V | Power rail verification |
| TP2 | 5V | LED power verification |
| TP3 | GND | Ground reference |
| TP4 | LED_DATA | Post-level-shifter |
| TP5 | I2C_SDA | Bus monitoring |
| TP6 | I2C_SCL | Bus monitoring |
| TP7 | I2S_BCLK | Audio debug |

---

## 7. Firmware Configuration

Update `firmware/domes/main/config.hpp` for dev board:

```cpp
// Board selection
#define CONFIG_DOMES_BOARD_DEVKIT_V1_1 1  // Development board

// GPIO Definitions (DevKit v1.1 + Dev Board)
static constexpr gpio_num_t GPIO_LED_DATA    = GPIO_NUM_38;  // Through level shifter
static constexpr gpio_num_t GPIO_I2C_SDA     = GPIO_NUM_8;
static constexpr gpio_num_t GPIO_I2C_SCL     = GPIO_NUM_9;
static constexpr gpio_num_t GPIO_I2S_BCLK    = GPIO_NUM_12;
static constexpr gpio_num_t GPIO_I2S_LRCLK   = GPIO_NUM_11;
static constexpr gpio_num_t GPIO_I2S_DOUT    = GPIO_NUM_13;
static constexpr gpio_num_t GPIO_AMP_SD      = GPIO_NUM_7;
static constexpr gpio_num_t GPIO_IMU_INT1    = GPIO_NUM_5;
static constexpr gpio_num_t GPIO_TOUCH_1     = GPIO_NUM_1;
static constexpr gpio_num_t GPIO_TOUCH_2     = GPIO_NUM_2;
static constexpr gpio_num_t GPIO_TOUCH_3     = GPIO_NUM_3;
static constexpr gpio_num_t GPIO_TOUCH_4     = GPIO_NUM_4;

// LED Configuration
static constexpr size_t LED_COUNT = 16;
```

---

## 8. Assembly Checklist

### 8.1 Before Power On

- [ ] Visual inspection for solder bridges
- [ ] Check orientation of ICs (Pin 1)
- [ ] Verify LED polarity (DIN/DOUT direction)
- [ ] Measure resistance between 3.3V and GND (should be >1K)
- [ ] Measure resistance between 5V and GND (should be >100Ω)

### 8.2 Initial Power On

- [ ] Connect DevKit via USB
- [ ] Measure 3.3V rail (expect 3.28-3.35V)
- [ ] Measure 5V rail (expect 4.8-5.2V)
- [ ] Check for hot components (short circuit indicator)

### 8.3 Functional Testing

- [ ] Flash LED test firmware → verify all 16 LEDs light up
- [ ] Run I2C scan → should find 0x19 (IMU) and 0x5A (Haptic)
- [ ] Test touch pads → verify touch detection
- [ ] Test audio playback → verify speaker output
- [ ] Test haptic feedback → verify motor vibration

---

## 9. Known Issues & Mitigations

| Issue | Mitigation |
|-------|------------|
| DRV2605L low stock (476 units) | Order extra; consider DRV2605YZFR (C81079) as alternative |
| SK6812 timing sensitive | Use 74AHCT125 (fast edge) not LVC for level shift |
| Touch sensitivity varies | Adjust ESP32 touch threshold in firmware |
| Audio noise from LEDs | Route I2S away from LED data; add 10nF filter cap |

---

## 10. Revision History

| Rev | Date | Author | Changes |
|-----|------|--------|---------|
| 1.0 | 2026-01-08 | Claude | Initial specification |

---

## 11. References

- [ESP32-S3-DevKitC-1 Schematic](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/)
- [ESP32 PCB Layout Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html)
- [LIS2DW12 Datasheet](https://www.st.com/resource/en/datasheet/lis2dw12.pdf)
- [DRV2605L Datasheet](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)
- [MAX98357A Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX98357A-MAX98357B.pdf)
- [SK6812MINI-E Datasheet](https://cdn-shop.adafruit.com/product-files/2686/SK6812MINI_REV.01-1-2.pdf)
