# DOMES Development Roadmap
## Accelerated Bring-Up Strategy

---

## EXECUTIVE SUMMARY

| Aspect | Decision |
|--------|----------|
| **Strategy** | Parallel dev: Claude writes firmware while you design PCB |
| **Timeline** | ~6-8 weeks to functional multi-pod prototype |
| **Dev Boards** | Minimal - only for ESP-NOW/RF validation (can't unit test) |
| **Breadboarding** | Skip - go straight to PCB |
| **Testing** | POSIX unit tests with CMock, not emulators |
| **CI** | Host-based unit tests only (no hardware CI) |

---

## DEVELOPMENT MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     PARALLEL DEVELOPMENT STREAMS                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   YOU (PM/EM + Hardware)              CLAUDE (Firmware)                 │
│   ─────────────────────               ─────────────────                 │
│                                                                          │
│   Week 1:                             Week 1:                           │
│   • Order dev boards (3x)             • Set up ESP-IDF project          │
│   • Order PCB components              • Write driver layer (mocked)     │
│   • Start schematic                   • Write unit test framework       │
│                                                                          │
│   Week 2:                             Week 2:                           │
│   • Validate ESP-NOW (2 hrs)          • Write communication layer       │
│   • Continue schematic                • Write game engine               │
│   • Start PCB layout                  • Write protocol handlers         │
│                                                                          │
│   Week 3:                             Week 3:                           │
│   • Finish PCB layout                 • Write OTA system                │
│   • Order dev PCB                     • Write smoke tests               │
│   • Order enclosure prints            • Expand unit test coverage       │
│                                                                          │
│   Week 4-5:                           Week 4-5:                         │
│   • PCB arrives                       • Flash & debug on real HW        │
│   • Assemble 6 units                  • Fix hardware-specific issues    │
│   • Integration testing               • Multi-pod testing               │
│                                                                          │
│   Week 6-7:                           Week 6-7:                         │
│   • Form-factor PCB design            • Feature completion              │
│   • Enclosure refinement              • BLE app integration             │
│                                                                          │
│   Week 8:                             Week 8:                           │
│   • Form-factor assembly              • Final polish                    │
│   • Demo-ready prototypes             • Documentation                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## WHY NO EMULATOR?

| Option | Verdict | Reason |
|--------|---------|--------|
| **Wokwi** | ❌ Skip | No support for DRV2605L, LIS2DW12, custom audio |
| **QEMU** | ❌ Skip | Limited peripheral emulation, complex setup |
| **Renode** | ❌ Skip | ESP32-S3 support incomplete |
| **POSIX Unit Tests** | ✅ Use | Fast, mock hardware, test business logic |

**Bottom line**: Emulators can't emulate our specific peripherals. Unit tests with mocked drivers give us 80% coverage. Real hardware validation for the remaining 20%.

---

## WHAT NEEDS REAL HARDWARE VALIDATION?

Only things that **cannot** be unit tested:

| Validation | Why Real HW? | Time Required |
|------------|--------------|---------------|
| **ESP-NOW latency** | RF timing is physics | 2 hours |
| **BLE + ESP-NOW coexistence** | RF arbitration | 1 hour |
| **Touch through diffuser** | Material properties | 1 hour |

**Everything else** (game logic, protocol, state machines, OTA) → Unit tests

---

## PHASE 1: SETUP & PARALLEL START (Week 1)

### You: Procurement + Schematic

**Order immediately:**
- 3× ESP32-S3-DevKitC-1-N16R8 ($45)
- PCB components (per BOM in System Architecture)
- 3D print filament for enclosure prototypes

**Start schematic** using System Architecture as reference.

### Claude: Firmware Foundation

**Deliverables:**
1. ESP-IDF project structure (per Software Architecture)
2. CMake build system with host-target support
3. Driver layer with hardware abstraction:
   - `led_driver.hpp` - SK6812 control (mocked for tests)
   - `audio_driver.hpp` - I2S audio (mocked for tests)
   - `haptic_driver.hpp` - DRV2605L (mocked for tests)
   - `touch_driver.hpp` - Capacitive touch (mocked for tests)
   - `imu_driver.hpp` - LIS2DW12 (mocked for tests)
4. Unit test framework (CMock + Unity)
5. First passing unit tests

**Architecture for testability:**
```cpp
// Hardware abstraction allows mocking
class ILedDriver {
public:
    virtual void setColor(uint8_t index, Color c) = 0;
    virtual void show() = 0;
};

class Sk6812Driver : public ILedDriver { /* Real implementation */ };
class MockLedDriver : public ILedDriver { /* For unit tests */ };
```

---

## PHASE 2: CORE DEVELOPMENT (Week 2-3)

### You: Hardware Validation + PCB

**ESP-NOW Validation (Week 2, ~2 hours):**
```
Setup: 3× ESP32-S3 dev boards

Test 1: Latency
- Flash Claude's espnow_latency_test firmware
- Run 10,000 ping-pong packets
- Success: 95th percentile RTT < 2ms

Test 2: BLE Coexistence
- Flash ble_espnow_coex_test firmware
- Connect phone via BLE
- Send commands while ESP-NOW running
- Success: No packet loss, stable connection

Test 3: Touch (optional, can do on PCB)
- Copper tape + acrylic sheet
- Flash touch_test firmware
- Success: Reliable detection through 2mm plastic
```

**PCB Design (Week 2-3):**
- Complete schematic
- Layout per ID requirements (hexagonal)
- Order from JLCPCB with assembly

### Claude: Feature Development

**Week 2 Deliverables:**
1. Communication layer:
   - `espnow_service.cpp` - Pod-to-pod messaging
   - `ble_service.cpp` - Phone connection
   - `protocol.cpp` - Message encoding/decoding
2. Timing service:
   - Clock synchronization
   - Microsecond timestamps
3. Unit tests for all communication logic

**Week 3 Deliverables:**
1. Game engine:
   - `state_machine.cpp` - Pod states
   - `drill_engine.cpp` - Drill logic
   - `feedback_service.cpp` - Coordinated LED/audio/haptic
2. OTA system:
   - Partition management
   - Rollback logic
3. Smoke test suite
4. >70% unit test coverage

---

## PHASE 3: INTEGRATION (Week 4-5)

### You: PCB Assembly

- PCBs arrive (~1 week from order)
- Assemble 6 units
- Basic power-on test

### Claude: Hardware Bring-Up

**On real hardware:**
1. Flash firmware
2. Debug hardware-specific issues:
   - GPIO assignments
   - I2C addresses
   - Timing adjustments
3. Tune parameters:
   - Touch thresholds
   - Haptic calibration
   - Audio levels
4. Multi-pod testing:
   - 6-pod synchronized drills
   - Reaction time measurement
   - Stress testing

**Deliverable:** Working 6-pod demo

---

## PHASE 4: FORM-FACTOR (Week 6-8)

### You: Form-Factor Hardware

- Hexagonal PCB layout
- LED ring integration
- Enclosure 3D prints
- Charging base

### Claude: Polish & Features

- BLE app integration (basic)
- Additional drills
- Power optimization
- Documentation

**Deliverable:** Demo-ready prototypes

---

## TESTING STRATEGY

### Unit Tests (Run on Host - Linux/Mac)

```
firmware/
├── test/
│   ├── test_protocol.cpp      # Message encoding
│   ├── test_state_machine.cpp # Pod states
│   ├── test_drill_engine.cpp  # Game logic
│   ├── test_timing.cpp        # Clock sync
│   └── mocks/
│       ├── mock_led_driver.cpp
│       ├── mock_audio_driver.cpp
│       └── ...
```

**Run tests:**
```bash
cd firmware
idf.py --preview set-target linux
idf.py build
./build/test_app
```

### Hardware Smoke Tests (Run on Device)

```cpp
// Built into firmware, triggered by button hold or BLE command
void runSmokeTests() {
    TEST(led_ring_all_colors);
    TEST(audio_play_tone);
    TEST(haptic_click);
    TEST(touch_detection);
    TEST(espnow_ping);
    TEST(ble_advertise);
    TEST(nvs_read_write);
    TEST(battery_voltage);
}
```

### No Hardware CI

**Why not:**
- Overkill for early development
- Complex setup (physical devices, flashing)
- Unit tests catch 80% of bugs
- Manual hardware testing is fine for 6-8 week timeline

**Later (post-prototype):** Consider hardware-in-loop CI with dedicated test fixtures.

---

## MINIMAL PROCUREMENT

### Dev Boards (Order Week 1)

| Item | Qty | Cost | Source |
|------|-----|------|--------|
| ESP32-S3-DevKitC-1-N16R8 | 3 | $45 | Digikey |

**That's it.** No breakout boards needed - you're going straight to PCB.

### PCB Components (Order Week 1)

Order full BOM for 10 units per System Architecture document.

---

## TIMELINE SUMMARY

```
Week:  1     2     3     4     5     6     7     8
       │     │     │     │     │     │     │     │
YOU:   │─────┼─────┼─────┼─────┼─────┼─────┼─────┤
       │Order│Valid│ PCB │ PCB │Assem│Form │Form │Demo
       │Parts│ESP- │Layou│Order│ble  │Fact │Fact │Ready
       │Schem│NOW  │t    │     │     │PCB  │Print│
       │     │(2hr)│     │     │     │     │     │
       │     │     │     │     │     │     │     │
CLAUDE:│─────┼─────┼─────┼─────┼─────┼─────┼─────┤
       │Proj │Comms│Game │     │HW   │     │     │
       │Setup│Layer│Engin│ OTA │Debug│Feats│Polish│
       │Drivr│Proto│State│Tests│Tune │BLE  │Docs │
       │Tests│     │     │     │     │     │     │
       │     │     │     │     │     │     │     │
       ▼     ▼     ▼     ▼     ▼     ▼     ▼     ▼
    Firmware  ESP-NOW   PCB    PCB    6-Pod  Form   Demo
    Compiles  Valid     Order  Arrive Demo   Factor Ready
```

---

## DECISION GATES

### Gate 1: End of Week 2
**Question:** Does ESP-NOW meet latency requirements?
- ✅ Yes → Continue
- ❌ No → Fallback to BLE-only (higher latency but works)

### Gate 2: End of Week 5
**Question:** Do 6 pods work together?
- ✅ Yes → Proceed to form-factor
- ❌ No → Debug, iterate on dev PCB

### Gate 3: End of Week 8
**Question:** Is product demo-ready?
- ✅ Yes → User testing, investor demos
- ❌ No → Additional iteration

---

## CLAUDE'S DEVELOPMENT PRIORITIES

### Week 1 Focus
1. **Project setup** - ESP-IDF, CMake, directory structure
2. **Driver abstractions** - Interfaces that can be mocked
3. **Unit test framework** - CMock integration
4. **LED driver** - First real driver implementation

### Week 2 Focus
1. **ESP-NOW wrapper** - Clean C++ API over ESP-IDF
2. **BLE service** - Basic connection and commands
3. **Protocol layer** - Message types, encoding

### Week 3 Focus
1. **Game engine** - State machine, drill logic
2. **Feedback service** - Coordinated LED/audio/haptic
3. **OTA system** - Basic update flow

### Week 4-5 Focus
1. **Hardware debugging** - GPIO, timing, calibration
2. **Integration testing** - Multi-pod scenarios
3. **Bug fixes** - From real hardware testing

### Week 6-8 Focus
1. **Feature completion** - All planned drills
2. **BLE app basics** - Minimum viable app
3. **Documentation** - For handoff/continuity

---

## RISKS & MITIGATIONS

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| ESP-NOW latency too high | Low | Validate Week 2; fallback to BLE |
| PCB layout errors | Medium | Careful review; dev PCB is throwaway |
| Touch through diffuser | Medium | Test materials; add MPR121 if needed |
| Timeline slip | Medium | Parallel streams reduce critical path |

---

*Document Created: 2026-01-03*
*Last Updated: 2026-01-03*
*Project: DOMES*
