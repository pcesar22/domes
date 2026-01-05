# 06 - Testing

## AI Agent Instructions

Load this file when setting up unit testing or writing mocks.

Prerequisites: `03-driver-development.md`

---

## Testing Strategy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            TESTING PYRAMID                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                           ┌─────────┐                                        │
│                           │   E2E   │  Manual on hardware                    │
│                           │  Tests  │  (Smoke tests)                         │
│                          ┌┴─────────┴┐                                       │
│                          │Integration│  Multiple components                  │
│                          │   Tests   │  (Host or target)                     │
│                         ┌┴───────────┴┐                                      │
│                         │ Unit Tests  │  Single component                    │
│                         │   (Host)    │  Fast, isolated                      │
│                         └─────────────┘                                      │
│                                                                              │
│   Priority: Unit tests first, integration second, E2E for validation        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Test Type | Runs On | Speed | Hardware |
|-----------|---------|-------|----------|
| Unit tests | Linux host | < 1s | No |
| Integration | Linux host | < 10s | No |
| Smoke tests | ESP32 target | 1-5 min | Yes |

---

## Test File Structure

```
firmware/
└── test/
    ├── CMakeLists.txt
    ├── mocks/
    │   ├── mock_led_driver.hpp
    │   ├── mock_audio_driver.hpp
    │   └── ...
    ├── test_led_driver.cpp
    ├── test_feedback_service.cpp
    └── test_protocol.cpp
```

---

## Mock Pattern

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                             MOCK STRUCTURE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Mock Driver                                                          │  │
│   ├──────────────────────────────────────────────────────────────────────┤  │
│   │                                                                       │  │
│   │  Interface Methods (from IDriver):                                   │  │
│   │  • init()              → Record call, return configured value        │  │
│   │  • doAction(params)    → Record params, return configured value      │  │
│   │  • getState()          → Return configured state                     │  │
│   │                                                                       │  │
│   │  Test Control:                                                        │  │
│   │  • esp_err_t nextReturnValue   ← Configure return values             │  │
│   │  • bool simulateFailure        ← Trigger error conditions            │  │
│   │                                                                       │  │
│   │  Test Inspection:                                                     │  │
│   │  • int callCount               ← Verify method was called            │  │
│   │  • ParamType lastParam         ← Verify correct parameters           │  │
│   │  • void reset()                ← Clear state between tests           │  │
│   │                                                                       │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Mock Interface

```cpp
// EXAMPLE: Mock pattern - implement for each driver interface

class MockLedDriver : public ILedDriver {
public:
    // Test control - configure behavior
    esp_err_t initReturnValue = ESP_OK;
    esp_err_t refreshReturnValue = ESP_OK;

    // Test inspection - verify calls
    int initCallCount = 0;
    int refreshCallCount = 0;
    Color lastSetAllColor{};

    // Reset between tests
    void reset();

    // Interface implementation records calls and returns configured values
};
```

---

## Test Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           UNIT TEST FLOW                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   setUp()                     Test Case                     tearDown()       │
│      │                            │                             │            │
│      ▼                            ▼                             ▼            │
│   ┌──────────┐            ┌──────────────┐              ┌──────────┐        │
│   │  Reset   │            │   Execute    │              │  Cleanup │        │
│   │  mocks   │───────────►│   action     │─────────────►│  state   │        │
│   └──────────┘            └──────┬───────┘              └──────────┘        │
│                                  │                                          │
│                                  ▼                                          │
│                           ┌──────────────┐                                  │
│                           │   Assert     │                                  │
│                           │   results    │                                  │
│                           └──────────────┘                                  │
│                                                                              │
│   Example assertions:                                                        │
│   • TEST_ASSERT_EQUAL(1, mock.callCount)                                    │
│   • TEST_ASSERT_EQUAL(ESP_OK, result)                                       │
│   • TEST_ASSERT_EQUAL(255, mock.lastColor.g)                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Unity Test Macros

| Macro | Purpose |
|-------|---------|
| `TEST_ASSERT(cond)` | Boolean condition |
| `TEST_ASSERT_EQUAL(exp, act)` | Integer equality |
| `TEST_ASSERT_EQUAL_STRING(exp, act)` | String equality |
| `TEST_ASSERT_EQUAL_MEMORY(exp, act, len)` | Memory comparison |
| `TEST_ASSERT_FLOAT_WITHIN(delta, exp, act)` | Float with tolerance |
| `TEST_FAIL_MESSAGE("reason")` | Force failure |

---

## Coverage Targets

| Component | Target | Rationale |
|-----------|--------|-----------|
| Protocol encoding | 100% | Critical for interop |
| State machine | 90% | Core logic |
| Services | 80% | Business logic |
| Drivers (mocked) | 70% | Hardware abstraction |
| **Overall** | **> 70%** | MVP threshold |

---

## CI Pipeline

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CI PIPELINE STAGES                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐             │
│   │  Build   │───►│  Unit    │───►│  Build   │───►│  Size    │             │
│   │  Linux   │    │  Tests   │    │  ESP32   │    │  Check   │             │
│   └──────────┘    └──────────┘    └──────────┘    └──────────┘             │
│                                                                              │
│   On push/PR:     Run tests      Verify target    Binary < 4MB             │
│   set-target      ./build/       compiles         for OTA                  │
│   linux           domes.elf                                                │
│                                                                              │
│   Nightly (hardware):                                                       │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐                             │
│   │  Flash   │───►│  Smoke   │───►│  Latency │                             │
│   │  DevKit  │    │  Tests   │    │  Tests   │                             │
│   └──────────┘    └──────────┘    └──────────┘                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Smoke Test Suite

| Test | Method | Pass Criteria |
|------|--------|---------------|
| LED Ring | Color cycle | All 16 LEDs visible |
| Audio | Tone sequence | Audible 440/880/1320 Hz |
| Haptic | Effect sequence | Vibration felt |
| Touch | Wait for input | Detects within 5s |
| IMU | Read accel | ~1g magnitude |
| Battery | Read voltage | 3.0-4.25V |

Smoke tests require manual observation. Run via `test_all` serial command.

---

*Prerequisites: 03-driver-development.md*
*Related: 02-build-system.md (CI config), 13-validation-test-hooks.md*
