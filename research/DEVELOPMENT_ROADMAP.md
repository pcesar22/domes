# DOMES Development Roadmap
## Phased Bring-Up Strategy

---

## EXECUTIVE SUMMARY

**Strategy**: Dev kit validation → Integration breadboard → Dev PCB → Form-factor prototype

**Timeline**: ~10-14 weeks to functional multi-pod prototype

**Philosophy**: De-risk critical unknowns cheaply before committing to PCB fabrication

---

## PHASE 0: PROCUREMENT (Week 0-1)

### 0.1 Development Boards

| Item | Qty | Purpose | Est. Cost | Source |
|------|-----|---------|-----------|--------|
| ESP32-S3-DevKitC-1-N16R8 | 4 | Main dev, multi-pod testing | $15 × 4 = $60 | Digikey/Mouser |
| ESP32-S3-DevKitC-1-N8R2 | 2 | Backup, cost comparison | $10 × 2 = $20 | Digikey/Mouser |

### 0.2 Breakout Boards

| Item | Qty | Purpose | Est. Cost | Source |
|------|-----|---------|-----------|--------|
| Adafruit DRV2605L Haptic Driver | 2 | Haptic testing | $8 × 2 = $16 | Adafruit |
| Adafruit MAX98357A I2S Amp | 2 | Audio testing | $6 × 2 = $12 | Adafruit |
| LIS2DW12 breakout (or LIS3DH) | 2 | IMU tap detection | $8 × 2 = $16 | Adafruit/SparkFun |
| SK6812 RGBW LED Ring (16 LED) | 2 | LED testing | $10 × 2 = $20 | AliExpress/Amazon |

### 0.3 Components

| Item | Qty | Purpose | Est. Cost | Source |
|------|-----|---------|-----------|--------|
| LRA Motor (10mm) | 4 | Haptic feedback | $3 × 4 = $12 | Digikey/AliExpress |
| Speaker 23mm 8Ω 1W | 4 | Audio output | $2 × 4 = $8 | AliExpress |
| LiPo Battery 1200mAh 103040 | 4 | Power testing | $5 × 4 = $20 | AliExpress |
| TP4056 module | 4 | Charging | $1 × 4 = $4 | AliExpress |
| Copper tape (for touch) | 1 roll | Touch sensor prototyping | $8 | Amazon |
| Pogo pins (various) | 1 kit | Charging connector tests | $15 | Amazon |
| Neodymium magnets 6×3mm | 20 | Alignment testing | $10 | Amazon |

### 0.4 Tools & Supplies

| Item | Qty | Purpose | Est. Cost |
|------|-----|---------|-----------|
| Breadboards | 4 | Prototyping | $15 |
| Jumper wires | 1 kit | Connections | $10 |
| Logic analyzer (optional) | 1 | Debugging | $15-50 |
| Multimeter | 1 | Power measurements | Have or $30 |
| USB-C cables | 4 | Programming/power | $15 |

### 0.5 Budget Summary - Phase 0

| Category | Cost |
|----------|------|
| Dev boards | $80 |
| Breakouts | $64 |
| Components | $77 |
| Tools/supplies | $55-90 |
| **Total** | **~$280-320** |

**Lead time**: Most items 2-5 days (Adafruit/SparkFun/Digikey), AliExpress items 1-2 weeks

---

## PHASE 1: SUBSYSTEM VALIDATION (Week 1-2)

### 1.1 Goals
- Validate each subsystem independently
- Confirm component choices work as expected
- Get baseline firmware running
- **NO INTEGRATION YET** - isolated testing

### 1.2 Test Plan

#### Test 1.1: ESP-NOW Latency (CRITICAL)
```
Setup: 3× ESP32-S3 dev boards

Test procedure:
1. Flash ESP-NOW ping-pong firmware
2. Board A sends packet, timestamps TX
3. Board B receives, sends ACK
4. Board A receives ACK, timestamps RX
5. Measure round-trip latency
6. Log 10,000 packets, analyze distribution

Success criteria:
- 95th percentile RTT < 2ms
- 99th percentile RTT < 5ms
- No packets lost at 10Hz rate
```

**Firmware deliverable**: `test_espnow_latency.c`

#### Test 1.2: BLE Connection + ESP-NOW Relay
```
Setup:
- Phone with nRF Connect app (or simple test app)
- ESP32-S3 "master" with BLE + ESP-NOW
- ESP32-S3 "slave" with ESP-NOW only

Test procedure:
1. Phone connects to master via BLE
2. Phone sends command (e.g., "LED ON")
3. Master relays to slave via ESP-NOW
4. Measure total latency phone→slave

Success criteria:
- Total latency < 50ms
- Stable connection over 10 minutes
```

**Firmware deliverable**: `test_ble_espnow_bridge.c`

#### Test 1.3: Touch Detection (CRITICAL)
```
Setup:
- ESP32-S3 dev board
- Copper tape pad (~50mm diameter)
- Connected to touch-capable GPIO

Test procedure:
1. Configure ESP32-S3 touch peripheral
2. Test detection threshold with bare hand
3. Test detection through 2mm acrylic (simulating diffuser)
4. Test false trigger rate (no touch)
5. Test response time (touch → interrupt)

Success criteria:
- 100% detection rate for firm touch
- >95% detection rate for light tap
- Works through 2mm plastic
- <10ms response time
- <1% false trigger rate over 1 hour
```

**Firmware deliverable**: `test_touch_sensing.c`

#### Test 1.4: IMU Tap Detection
```
Setup:
- ESP32-S3 dev board
- LIS2DW12 breakout on I2C

Test procedure:
1. Configure tap detection interrupt
2. Test single tap detection
3. Test double tap detection
4. Tune threshold parameters
5. Test on different surfaces (table, rubber mat)

Success criteria:
- >90% single tap detection
- >80% double tap detection
- Interrupt wakes ESP32 from light sleep
```

**Firmware deliverable**: `test_imu_tap.c`

#### Test 1.5: LED Ring Control
```
Setup:
- ESP32-S3 dev board
- SK6812 RGBW 16-LED ring
- 5V power supply (or USB power)

Test procedure:
1. Use FastLED or ESP-IDF RMT driver
2. Test all colors (R, G, B, W)
3. Test color mixing
4. Test animations (chase, pulse, fade)
5. Measure current at various brightness levels

Success criteria:
- All 16 LEDs individually addressable
- Smooth animations at 60 FPS
- White channel works correctly
- Current matches datasheet expectations
```

**Firmware deliverable**: `test_led_ring.c`

#### Test 1.6: Audio Output (I2S)
```
Setup:
- ESP32-S3 dev board
- MAX98357A breakout
- 23mm speaker

Test procedure:
1. Play pre-generated WAV samples (16kHz, 16-bit)
2. Test various volumes
3. Test different sound types (beeps, jingles, voice)
4. Measure power consumption during playback

Success criteria:
- Clean audio output (no distortion at 80% volume)
- Adequate loudness for gym environment
- Startup pop acceptable or mitigated
```

**Firmware deliverable**: `test_audio_i2s.c`

#### Test 1.7: Haptic Feedback
```
Setup:
- ESP32-S3 dev board
- DRV2605L breakout
- LRA motor

Test procedure:
1. Initialize DRV2605L over I2C
2. Test built-in effects (clicks, buzzes, pulses)
3. Tune auto-resonance calibration
4. Test response time (trigger → vibration)
5. Test various intensity levels

Success criteria:
- Crisp, satisfying haptic feel
- Response time < 20ms
- At least 10 distinguishable effect variations
```

**Firmware deliverable**: `test_haptic.c`

### 1.3 Phase 1 Deliverables

| Deliverable | Description |
|-------------|-------------|
| Test firmware modules | 7 standalone test programs |
| Test results document | Pass/fail for each test, measurements |
| Component validation | Confirm or flag component choices |
| Issue log | Any problems discovered |

### 1.4 Decision Gate

**Before Phase 2**:
- All critical tests (ESP-NOW, Touch) must pass
- Document any component substitutions needed
- Update system architecture if changes required

---

## PHASE 2: INTEGRATION BREADBOARD (Week 2-4)

### 2.1 Goals
- Integrate all subsystems on one ESP32-S3
- Validate power budget in practice
- Build minimal "pod" firmware
- Test concurrent operation (touch + audio + LED + haptic)

### 2.2 Integration Breadboard

```
┌─────────────────────────────────────────────────────────────┐
│                    INTEGRATION BREADBOARD                    │
│                                                              │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│   │   ESP32-S3  │    │  MAX98357A  │    │  DRV2605L   │    │
│   │   DevKit    │    │   + Speaker │    │   + LRA     │    │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘    │
│          │                  │                   │           │
│          │    I2S (3 wire)  │     I2C (2 wire) │           │
│          ├──────────────────┘                   │           │
│          ├──────────────────────────────────────┘           │
│          │                                                  │
│   ┌──────┴──────┐    ┌─────────────┐    ┌─────────────┐    │
│   │   LIS2DW12  │    │  SK6812 x16 │    │ Touch Pad   │    │
│   │    (I2C)    │    │  LED Ring   │    │ (copper)    │    │
│   └─────────────┘    └─────────────┘    └─────────────┘    │
│                                                              │
│   Power: USB (dev) or LiPo + TP4056 (battery test)          │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Integration Tests

#### Test 2.1: Concurrent Operation
```
Test: All subsystems running simultaneously
- LEDs animating (chase pattern)
- Audio playing (background tone)
- Haptic on standby
- Touch armed
- IMU monitoring
- ESP-NOW listening

Measure:
- CPU utilization (both cores)
- Memory usage
- Any conflicts or glitches
```

#### Test 2.2: Touch → Full Response
```
Test: Complete feedback loop
1. Touch pad
2. LED changes color (< 10ms)
3. Sound plays (< 20ms)
4. Haptic pulses (< 20ms)
5. ESP-NOW broadcasts event (< 1ms)

Success: All feedback within 50ms of touch
```

#### Test 2.3: Power Budget Validation
```
Test: Measure actual current draw
- Idle (armed, waiting for touch)
- Active (LED on, touch detected)
- Audio playing
- All systems max

Compare to estimates in System Architecture doc
```

#### Test 2.4: Multi-Pod Communication
```
Setup: 3 breadboard assemblies

Test: Synchronized behavior
1. Touch Pod A
2. Pod A sends ESP-NOW to B and C
3. All pods light up simultaneously

Measure: Visual sync (should look instant to human eye)
```

### 2.4 Firmware Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     FIRMWARE STRUCTURE                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  main/                                                       │
│  ├── main.c                 # App entry point               │
│  ├── config.h               # Pin definitions, settings     │
│  │                                                          │
│  ├── drivers/                                                │
│  │   ├── led_ring.c         # SK6812 control               │
│  │   ├── audio.c            # I2S + sample playback        │
│  │   ├── haptic.c           # DRV2605L control             │
│  │   ├── touch.c            # Capacitive touch             │
│  │   ├── imu.c              # LIS2DW12 tap detection       │
│  │   └── power.c            # Battery monitoring           │
│  │                                                          │
│  ├── comms/                                                  │
│  │   ├── espnow_handler.c   # Pod-to-pod messaging         │
│  │   ├── ble_handler.c      # Phone communication          │
│  │   └── protocol.c         # Message encoding/decoding    │
│  │                                                          │
│  ├── game/                                                   │
│  │   ├── drill_engine.c     # Drill state machine          │
│  │   ├── feedback.c         # Audio/haptic/LED coordination│
│  │   └── timing.c           # Reaction time measurement    │
│  │                                                          │
│  └── assets/                                                 │
│      ├── sounds/            # WAV samples (SPIFFS)         │
│      └── effects/           # Haptic effect definitions    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.5 Phase 2 Deliverables

| Deliverable | Description |
|-------------|-------------|
| Integrated firmware | Single firmware running all subsystems |
| 3 working breadboards | Multi-pod testing capability |
| Power measurements | Validated power budget |
| Demo: "Touch-and-react" | Touch any pod, all respond |

---

## PHASE 3: DEVELOPMENT PCB (Week 4-8)

### 3.1 Goals
- Consolidate breadboard into single PCB
- NOT form-factor optimized (rectangular OK)
- Easy to assemble and debug
- Make 6-10 units for real multi-pod testing

### 3.2 Dev PCB Philosophy

**DO**:
- Route for electrical correctness
- Include all test points
- Use 0603 passives (easy hand soldering)
- Keep components accessible
- Include silk screen labels

**DON'T**:
- Optimize for size (yet)
- Match final form factor
- Remove debug features
- Use 0402 or smaller

### 3.3 Dev PCB Specifications

| Parameter | Specification |
|-----------|---------------|
| Shape | Rectangular (~60mm × 80mm) |
| Layers | 4-layer (signal, GND, power, signal) |
| Components | Single-sided preferred |
| Assembly | Hand-solderable (TQFP, 0603) |
| Quantity | 10 boards |
| Fab | JLCPCB or PCBWay |
| Assembly | Partial PCBA or hand-assemble |

### 3.4 Dev PCB Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        DEV PCB v0.1                              │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                          │    │
│  │   ┌───────────┐              ┌───────────┐              │    │
│  │   │ ESP32-S3  │              │ LIS2DW12  │              │    │
│  │   │ WROOM-1   │──── I2C ─────│    IMU    │              │    │
│  │   │  N16R8    │              └───────────┘              │    │
│  │   │           │              ┌───────────┐              │    │
│  │   │           │──── I2C ─────│ DRV2605L  │── LRA Motor  │    │
│  │   │           │              └───────────┘              │    │
│  │   │           │              ┌───────────┐              │    │
│  │   │           │──── I2S ─────│ MAX98357A │── Speaker    │    │
│  │   │           │              └───────────┘              │    │
│  │   │           │                                         │    │
│  │   │           │──── Data ────── SK6812 Ring (off-board) │    │
│  │   │           │                                         │    │
│  │   │           │──── Touch ───── Copper Pad (off-board)  │    │
│  │   │           │                                         │    │
│  │   │           │──── USB-C ───── Programming/Debug       │    │
│  │   └───────────┘                                         │    │
│  │                                                          │    │
│  │   ┌─────────┐   ┌─────────┐   ┌─────────────┐          │    │
│  │   │ TP4056  │───│  LDO    │───│ Battery Conn │          │    │
│  │   │ Charger │   │ 3.3V    │   │  + Pogo Pads │          │    │
│  │   └─────────┘   └─────────┘   └─────────────────┘       │    │
│  │                                                          │    │
│  │   ┌──────────────────────────────────────────┐          │    │
│  │   │  Test Points: 3V3, GND, VBAT, TX, RX,    │          │    │
│  │   │  I2C, I2S, Touch, EN, GPIO0              │          │    │
│  │   └──────────────────────────────────────────┘          │    │
│  │                                                          │    │
│  │   ┌───────────┐                                         │    │
│  │   │ TagConnect│   JTAG Debug Pads                       │    │
│  │   │  TC2030   │                                         │    │
│  │   └───────────┘                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 3.5 Off-Board Connections (Dev PCB)

For dev PCB, keep these off-board for flexibility:

| Component | Connection | Reason |
|-----------|------------|--------|
| LED Ring | JST connector | Test different LED configs |
| Touch Pad | Wire/connector | Test different pad shapes |
| Speaker | JST connector | Test different speakers |
| LRA Motor | JST connector | Test different motors |
| Battery | JST PH 2.0 | Standard LiPo connector |

### 3.6 Charging Test Jig

Separate from dev PCB, build a charging jig:

```
┌───────────────────────────────────────┐
│         CHARGING TEST JIG             │
│                                       │
│   ┌─────────────────────────────┐    │
│   │     Pogo Pin Cradle         │    │
│   │   ┌───┐ ┌───┐ ┌───┐        │    │
│   │   │ + │ │ - │ │DAT│        │    │
│   │   └───┘ └───┘ └───┘        │    │
│   └─────────────────────────────┘    │
│              │                        │
│   ┌──────────┴──────────┐            │
│   │   USB-C Power In    │            │
│   └─────────────────────┘            │
│                                       │
│   Magnets for alignment testing       │
└───────────────────────────────────────┘
```

### 3.7 Phase 3 Timeline

| Week | Activity |
|------|----------|
| Week 4 | Schematic design, component selection finalization |
| Week 5 | PCB layout, design review |
| Week 6 | Order PCBs (JLCPCB: 5-7 days), order components |
| Week 7 | PCBs arrive, assembly, initial bring-up |
| Week 8 | Debug, firmware refinement, multi-pod testing |

### 3.8 Phase 3 Deliverables

| Deliverable | Description |
|-------------|-------------|
| Schematic | Full schematic in KiCad/Altium |
| PCB layout | 4-layer dev board |
| BOM | Complete bill of materials |
| 6-10 assembled boards | For team testing |
| Firmware v0.2 | Ported from breadboard |
| Multi-pod demo | 6 pods doing coordinated drills |

---

## PHASE 4: FORM-FACTOR PROTOTYPE (Week 8-12)

### 4.1 Goals
- PCB matches hexagonal form factor
- 3D printed enclosure
- Full integration with ID design
- Looks and feels like real product

### 4.2 Form-Factor PCB

| Parameter | Specification |
|-----------|---------------|
| Shape | Hexagonal, ~100mm corner-to-corner |
| Layers | 4-layer |
| Components | Both sides if needed |
| LED integration | Ring of SK6812 on PCB perimeter |
| Touch | Copper pour under diffuser area |
| Assembly | PCBA (professional assembly) |
| Quantity | 10-20 boards |

### 4.3 Enclosure Prototype

| Component | Method |
|-----------|--------|
| Diffuser top | Clear resin SLA print, sanded |
| Base shell | FDM print (PETG or ABS) |
| Rubber grip | Silicone casting or TPU print |
| Magnets | Press-fit into base |
| Pogo pins | Soldered to PCB |

### 4.4 Charging Base Prototype

| Component | Method |
|-----------|--------|
| Base PCB | Simple PCB with pogo pin sockets |
| Housing | 3D printed |
| USB-C | Panel mount or PCB-mount |
| Magnets | Aligned with pod magnets |

### 4.5 Phase 4 Deliverables

| Deliverable | Description |
|-------------|-------------|
| Form-factor PCB | Production-representative design |
| 3D printed enclosure | Matches ID requirements |
| Charging base | Functional stacking/charging |
| 6+ complete prototypes | Full system testing |
| Demo video | Showcase for stakeholders/investors |

---

## PHASE 5: DESIGN FOR MANUFACTURING (Week 12-16)

### 5.1 Goals
- Refine design for production
- Cost optimization
- Reliability testing
- Regulatory prep

### 5.2 Activities

| Activity | Description |
|----------|-------------|
| DFM review | Work with CM on PCB/assembly optimization |
| DVT testing | Drop test, water resistance, thermal |
| Regulatory pre-scan | FCC/CE pre-compliance testing |
| Injection mold quotes | For enclosure production |
| Firmware hardening | OTA, error handling, edge cases |
| App development | iOS/Android companion app |

---

## RISK REGISTER

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| ESP-NOW latency too high | Low | Critical | Test in Phase 1, fallback to BLE |
| Touch unreliable through diffuser | Medium | High | Test materials in Phase 1, add MPR121 if needed |
| Audio quality insufficient | Low | Medium | Test speakers early, upgrade amp if needed |
| Power budget exceeded | Medium | Medium | Measure in Phase 2, optimize or larger battery |
| PCB layout issues | Medium | Medium | Thorough review, prototype before volume |
| Form factor assembly difficult | Medium | Low | Iterate enclosure in Phase 4 |

---

## TIMELINE SUMMARY

```
Week:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
       │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
       ├───┴───┤
       │Phase 0│ Procurement
       │       │
       │       ├───────┤
       │       │Phase 1│ Subsystem Validation
       │       │       │
       │       │       ├───────────┤
       │       │       │  Phase 2  │ Integration Breadboard
       │       │       │           │
       │       │       │           ├───────────────────┤
       │       │       │           │      Phase 3      │ Dev PCB
       │       │       │           │                   │
       │       │       │           │                   ├─────────┤
       │       │       │           │                   │ Phase 4 │
       │       │       │           │                   │Form-Factor
       │       │       │           │                   │         │
       ▼       ▼       ▼           ▼                   ▼         ▼
    Order   Tests   Integrated   PCB Design        Form-Factor
    Parts   Pass    Firmware     Complete          Prototype
```

---

## DECISION GATES

### Gate 1: After Phase 1 (Week 2)
**Question**: Do component choices work?
- ESP-NOW latency acceptable? → Proceed
- Touch sensing works? → Proceed
- Major issues? → Revisit architecture

### Gate 2: After Phase 2 (Week 4)
**Question**: Does integrated system work?
- All subsystems play nicely? → Proceed to PCB
- Power budget OK? → Proceed
- Critical issues? → More breadboard iteration

### Gate 3: After Phase 3 (Week 8)
**Question**: Is dev PCB functional?
- 6+ pods working together? → Proceed to form-factor
- Any schematic bugs? → Fix in form-factor PCB
- Go/no-go on form-factor investment

### Gate 4: After Phase 4 (Week 12)
**Question**: Is product viable?
- Prototypes look/feel right? → Proceed to DFM
- User testing positive? → Proceed
- Pivot needed? → Reassess

---

## APPENDIX: SHOPPING LIST

### Immediate Orders (Week 0)

**Digikey/Mouser (2-3 day shipping):**
- 4× ESP32-S3-DevKitC-1-N16R8
- 2× LIS2DW12 breakout (SparkFun SEN-18030 or similar)

**Adafruit (2-3 day shipping):**
- 2× DRV2605L Haptic Driver (Product ID 2305)
- 2× MAX98357A I2S Amp (Product ID 3006)
- 1× LIS3DH breakout if LIS2DW12 unavailable

**Amazon (1-2 day shipping):**
- SK6812 RGBW LED rings (search "SK6812 RGBW 16 LED ring")
- Copper tape (1 inch wide)
- Breadboards
- Jumper wire kit
- Neodymium magnets 6×3mm

**AliExpress (1-2 week shipping) - order immediately:**
- 10× LRA motors (10mm coin type)
- 10× Speakers 23mm 8Ω
- 10× LiPo 1200mAh 103040
- 10× TP4056 modules
- Pogo pin assortment

---

*Document Created: 2026-01-03*
*Project: DOMES*
*Status: Ready for Execution*
