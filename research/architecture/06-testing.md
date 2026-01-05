# 06 - Testing

## AI Agent Instructions

Load this file when:
- Setting up unit testing framework
- Writing mock implementations
- Creating test cases
- Setting up CI pipeline

Prerequisites: `03-driver-development.md`

---

## Testing Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Testing Pyramid                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│                      ┌─────────┐                             │
│                      │  E2E    │  Manual on hardware         │
│                      │  Tests  │  (Smoke tests)              │
│                     ┌┴─────────┴┐                            │
│                     │Integration│  Multiple components       │
│                     │   Tests   │  (Host or target)          │
│                    ┌┴───────────┴┐                           │
│                    │  Unit Tests │  Single component         │
│                    │  (Host)     │  Fast, isolated           │
│                    └─────────────┘                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

| Test Type | Runs On | Speed | Hardware Required |
|-----------|---------|-------|-------------------|
| Unit tests | Linux host | < 1s | No |
| Integration tests | Linux host | < 10s | No |
| Smoke tests | ESP32 target | 1-5 min | Yes |

---

## Test Framework: Unity

ESP-IDF uses Unity test framework by default.

### Test File Structure

```
firmware/
└── test/
    ├── CMakeLists.txt
    ├── mocks/
    │   ├── mock_led_driver.hpp
    │   ├── mock_audio_driver.hpp
    │   ├── mock_haptic_driver.hpp
    │   └── mock_i2c.hpp
    ├── test_led_driver.cpp
    ├── test_feedback_service.cpp
    └── test_protocol.cpp
```

### Test CMakeLists.txt

```cmake
# firmware/test/CMakeLists.txt
idf_component_register(
    SRCS
        "test_led_driver.cpp"
        "test_feedback_service.cpp"
        "test_protocol.cpp"
    INCLUDE_DIRS
        "."
        "mocks"
        "../main"
        "../main/interfaces"
        "../main/drivers"
        "../main/services"
        "../main/game"
        "../main/utils"
    REQUIRES
        unity
)
```

---

## Mock Implementations

### Mock LED Driver

```cpp
// test/mocks/mock_led_driver.hpp
#pragma once

#include "interfaces/i_led_driver.hpp"
#include <array>

namespace domes {

class MockLedDriver : public ILedDriver {
public:
    // Interface implementation
    esp_err_t init() override {
        initCallCount++;
        initialized = true;
        return initReturnValue;
    }

    esp_err_t setAll(Color color) override {
        setAllCallCount++;
        lastSetAllColor = color;
        for (auto& c : colors) c = color;
        return ESP_OK;
    }

    esp_err_t setLed(uint8_t index, Color color) override {
        if (index >= kLedCount) return ESP_ERR_INVALID_ARG;
        setLedCallCount++;
        colors[index] = color;
        return ESP_OK;
    }

    esp_err_t setBrightness(uint8_t brightness) override {
        this->brightness = brightness;
        return ESP_OK;
    }

    esp_err_t refresh() override {
        refreshCallCount++;
        return refreshReturnValue;
    }

    bool isInitialized() const override {
        return initialized;
    }

    // Test inspection
    void reset() {
        initCallCount = 0;
        setAllCallCount = 0;
        setLedCallCount = 0;
        refreshCallCount = 0;
        initialized = false;
        brightness = 255;
        lastSetAllColor = Color::off();
        colors.fill(Color::off());
        initReturnValue = ESP_OK;
        refreshReturnValue = ESP_OK;
    }

    // Test control
    esp_err_t initReturnValue = ESP_OK;
    esp_err_t refreshReturnValue = ESP_OK;

    // Test inspection
    int initCallCount = 0;
    int setAllCallCount = 0;
    int setLedCallCount = 0;
    int refreshCallCount = 0;
    bool initialized = false;
    uint8_t brightness = 255;
    Color lastSetAllColor{};
    std::array<Color, kLedCount> colors{};
};

}  // namespace domes
```

### Mock Audio Driver

```cpp
// test/mocks/mock_audio_driver.hpp
#pragma once

#include "interfaces/i_audio_driver.hpp"
#include <string>

namespace domes {

class MockAudioDriver : public IAudioDriver {
public:
    esp_err_t init() override {
        initCalled = true;
        return initReturnValue;
    }

    esp_err_t playTone(uint16_t frequencyHz, uint16_t durationMs) override {
        playToneCallCount++;
        lastToneFrequency = frequencyHz;
        lastToneDuration = durationMs;
        playing = true;
        return ESP_OK;
    }

    esp_err_t playSound(const char* filename) override {
        playSoundCallCount++;
        lastSoundFile = filename;
        playing = true;
        return ESP_OK;
    }

    esp_err_t stop() override {
        stopCallCount++;
        playing = false;
        return ESP_OK;
    }

    esp_err_t setVolume(uint8_t volume) override {
        this->volume = volume;
        return ESP_OK;
    }

    bool isPlaying() const override { return playing; }
    bool isInitialized() const override { return initCalled; }

    void reset() {
        initCalled = false;
        playToneCallCount = 0;
        playSoundCallCount = 0;
        stopCallCount = 0;
        playing = false;
        volume = 128;
        lastToneFrequency = 0;
        lastToneDuration = 0;
        lastSoundFile.clear();
        initReturnValue = ESP_OK;
    }

    // Control
    esp_err_t initReturnValue = ESP_OK;

    // Inspection
    bool initCalled = false;
    int playToneCallCount = 0;
    int playSoundCallCount = 0;
    int stopCallCount = 0;
    bool playing = false;
    uint8_t volume = 128;
    uint16_t lastToneFrequency = 0;
    uint16_t lastToneDuration = 0;
    std::string lastSoundFile;
};

}  // namespace domes
```

### Mock Haptic Driver

```cpp
// test/mocks/mock_haptic_driver.hpp
#pragma once

#include "interfaces/i_haptic_driver.hpp"

namespace domes {

class MockHapticDriver : public IHapticDriver {
public:
    esp_err_t init() override {
        initCalled = true;
        return initReturnValue;
    }

    esp_err_t playEffect(uint8_t effectId) override {
        playEffectCallCount++;
        lastEffectId = effectId;
        return ESP_OK;
    }

    esp_err_t setIntensity(uint8_t intensity) override {
        this->intensity = intensity;
        return ESP_OK;
    }

    esp_err_t stop() override {
        stopCalled = true;
        return ESP_OK;
    }

    bool isInitialized() const override { return initCalled; }

    void reset() {
        initCalled = false;
        playEffectCallCount = 0;
        lastEffectId = 0;
        intensity = 128;
        stopCalled = false;
        initReturnValue = ESP_OK;
    }

    // Control
    esp_err_t initReturnValue = ESP_OK;

    // Inspection
    bool initCalled = false;
    int playEffectCallCount = 0;
    uint8_t lastEffectId = 0;
    uint8_t intensity = 128;
    bool stopCalled = false;
};

}  // namespace domes
```

---

## Unit Test Examples

### Test FeedbackService

```cpp
// test/test_feedback_service.cpp
#include "unity.h"
#include "services/feedback_service.hpp"
#include "mocks/mock_led_driver.hpp"
#include "mocks/mock_audio_driver.hpp"
#include "mocks/mock_haptic_driver.hpp"

namespace {
    domes::MockLedDriver mockLed;
    domes::MockAudioDriver mockAudio;
    domes::MockHapticDriver mockHaptic;
}

void setUp() {
    mockLed.reset();
    mockAudio.reset();
    mockHaptic.reset();
}

void tearDown() {
    // Cleanup if needed
}

TEST_CASE("FeedbackService playSuccess sets green color", "[feedback]") {
    domes::FeedbackService service(mockLed, mockAudio, mockHaptic);
    service.init();

    esp_err_t err = service.playSuccess();

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(1, mockLed.setAllCallCount);
    TEST_ASSERT_EQUAL(0, mockLed.lastSetAllColor.r);
    TEST_ASSERT_EQUAL(255, mockLed.lastSetAllColor.g);
    TEST_ASSERT_EQUAL(0, mockLed.lastSetAllColor.b);
}

TEST_CASE("FeedbackService playSuccess plays tone", "[feedback]") {
    domes::FeedbackService service(mockLed, mockAudio, mockHaptic);
    service.init();

    service.playSuccess();

    TEST_ASSERT_EQUAL(1, mockAudio.playToneCallCount);
    TEST_ASSERT_EQUAL(1000, mockAudio.lastToneFrequency);
}

TEST_CASE("FeedbackService playSuccess triggers haptic", "[feedback]") {
    domes::FeedbackService service(mockLed, mockAudio, mockHaptic);
    service.init();

    service.playSuccess();

    TEST_ASSERT_EQUAL(1, mockHaptic.playEffectCallCount);
    TEST_ASSERT_EQUAL(domes::IHapticDriver::kEffectStrongClick, mockHaptic.lastEffectId);
}

TEST_CASE("FeedbackService playError sets red color", "[feedback]") {
    domes::FeedbackService service(mockLed, mockAudio, mockHaptic);
    service.init();

    service.playError();

    TEST_ASSERT_EQUAL(255, mockLed.lastSetAllColor.r);
    TEST_ASSERT_EQUAL(0, mockLed.lastSetAllColor.g);
}
```

### Test Protocol Encoding

```cpp
// test/test_protocol.cpp
#include "unity.h"
#include "game/protocol.hpp"
#include <cstring>

TEST_CASE("SetColorMsg has correct size", "[protocol]") {
    TEST_ASSERT_EQUAL(12, sizeof(domes::SetColorMsg));
}

TEST_CASE("TouchEventMsg serializes correctly", "[protocol]") {
    domes::TouchEventMsg msg;
    msg.header = domes::makeHeader(domes::MessageType::kTouchEvent, 42);
    msg.podId = 3;
    msg.reactionTimeUs = 150000;  // 150ms
    msg.touchStrength = 200;

    TEST_ASSERT_EQUAL(1, msg.header.version);
    TEST_ASSERT_EQUAL(0x10, msg.header.type);
    TEST_ASSERT_EQUAL(42, msg.header.sequence);
    TEST_ASSERT_EQUAL(3, msg.podId);
    TEST_ASSERT_EQUAL(150000, msg.reactionTimeUs);
}

TEST_CASE("MessageHeader packs to 8 bytes", "[protocol]") {
    TEST_ASSERT_EQUAL(8, sizeof(domes::MessageHeader));
}
```

---

## Running Tests

### Host Tests (Linux)

```bash
cd /home/user/domes/firmware

# Set target to Linux
idf.py --preview set-target linux

# Build
idf.py build

# Run tests
./build/domes.elf

# Or run specific tests
./build/domes.elf -t feedback
./build/domes.elf -t protocol
```

### Target Tests (ESP32)

```bash
# Build for ESP32-S3
idf.py set-target esp32s3
idf.py build

# Flash and run
idf.py -p /dev/ttyUSB0 flash monitor

# Tests run automatically on boot
```

---

## Test Macros Reference

```cpp
// Assertions
TEST_ASSERT(condition)
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)

// Equality
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_UINT8(expected, actual)
TEST_ASSERT_EQUAL_STRING(expected, actual)
TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)

// Floating point
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)

// Arrays
TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)

// Failure
TEST_FAIL()
TEST_FAIL_MESSAGE("reason")
TEST_IGNORE()
```

---

## CI Pipeline

### GitHub Actions Workflow

```yaml
# .github/workflows/test.yml
name: Test

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.2

      - name: Build for Linux
        run: |
          cd firmware
          idf.py --preview set-target linux
          idf.py build

      - name: Run Unit Tests
        run: |
          cd firmware
          ./build/domes.elf

  build-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.2

      - name: Build for ESP32-S3
        run: |
          cd firmware
          idf.py set-target esp32s3
          idf.py build

      - name: Check Binary Size
        run: |
          SIZE=$(stat -c%s firmware/build/domes.bin)
          echo "Binary size: $SIZE bytes"
          if [ $SIZE -gt 4194304 ]; then
            echo "::error::Binary exceeds 4MB limit"
            exit 1
          fi
```

---

## Smoke Tests (On Hardware)

```cpp
// test/smoke_test.cpp
// Built into firmware, triggered by button hold at boot

namespace smoke_test {

bool testLedRing() {
    ESP_LOGI(kTag, "Testing LED ring...");
    ledDriver.setAll(Color::red());
    ledDriver.refresh();
    vTaskDelay(pdMS_TO_TICKS(200));

    ledDriver.setAll(Color::green());
    ledDriver.refresh();
    vTaskDelay(pdMS_TO_TICKS(200));

    ledDriver.setAll(Color::blue());
    ledDriver.refresh();
    vTaskDelay(pdMS_TO_TICKS(200));

    ledDriver.setAll(Color::off());
    ledDriver.refresh();

    return true;  // Visual verification
}

bool testAudio() {
    ESP_LOGI(kTag, "Testing audio...");
    audioDriver.playTone(1000, 200);
    vTaskDelay(pdMS_TO_TICKS(300));
    return true;  // Audible verification
}

bool testHaptic() {
    ESP_LOGI(kTag, "Testing haptic...");
    hapticDriver.playEffect(IHapticDriver::kEffectStrongClick);
    vTaskDelay(pdMS_TO_TICKS(200));
    return true;  // Tactile verification
}

bool testTouch() {
    ESP_LOGI(kTag, "Touch the pod within 5 seconds...");
    auto event = touchDriver.waitForTouch(5000);
    if (!event) {
        ESP_LOGE(kTag, "No touch detected!");
        return false;
    }
    ESP_LOGI(kTag, "Touch detected! Strength: %d", event->strength);
    return true;
}

void runAll() {
    struct Test { const char* name; bool (*func)(); };
    Test tests[] = {
        {"LED Ring", testLedRing},
        {"Audio", testAudio},
        {"Haptic", testHaptic},
        {"Touch", testTouch},
    };

    int passed = 0, failed = 0;
    for (const auto& test : tests) {
        ESP_LOGI(kTag, "=== %s ===", test.name);
        if (test.func()) {
            ESP_LOGI(kTag, "PASS");
            passed++;
        } else {
            ESP_LOGE(kTag, "FAIL");
            failed++;
        }
    }
    ESP_LOGI(kTag, "Results: %d passed, %d failed", passed, failed);
}

}  // namespace smoke_test
```

---

## Coverage Target

| Component | Target Coverage |
|-----------|-----------------|
| Protocol encoding | 100% |
| State machine | 90% |
| Services (with mocks) | 80% |
| Drivers (with mocks) | 70% |
| **Overall** | **> 70%** |

---

*Prerequisites: 03-driver-development.md*
*Related: 02-build-system.md (CI config)*
