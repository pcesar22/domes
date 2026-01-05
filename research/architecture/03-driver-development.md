# 03 - Driver Development

## AI Agent Instructions

Load this file when:
- Implementing any hardware driver (LED, audio, haptic, touch, IMU, power)
- Creating service layer components
- Setting up dependency injection
- Writing driver tests

Prerequisites: `00-getting-started.md`, `01-project-structure.md`

---

## Driver Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                     main.cpp                              │
│  Creates drivers, injects into services, starts tasks    │
└─────────────────────────┬────────────────────────────────┘
                          │ owns (unique_ptr)
                          ▼
┌──────────────────────────────────────────────────────────┐
│                   Driver Layer                            │
│  LedDriver, AudioDriver, HapticDriver, TouchDriver, etc │
│  Each implements an interface (ILedDriver, etc.)         │
└─────────────────────────┬────────────────────────────────┘
                          │ references
                          ▼
┌──────────────────────────────────────────────────────────┐
│                   Service Layer                           │
│  FeedbackService, CommService, TimingService             │
│  Receives driver interfaces via constructor              │
└──────────────────────────────────────────────────────────┘
```

---

## Interface Definitions

### ILedDriver

```cpp
// interfaces/i_led_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t w = 0;

    static constexpr Color off() { return {0, 0, 0, 0}; }
    static constexpr Color red() { return {255, 0, 0, 0}; }
    static constexpr Color green() { return {0, 255, 0, 0}; }
    static constexpr Color blue() { return {0, 0, 255, 0}; }
    static constexpr Color white() { return {0, 0, 0, 255}; }
};

/**
 * @brief Abstract interface for LED ring driver
 *
 * Controls 16x SK6812 RGBW LEDs via RMT peripheral.
 */
class ILedDriver {
public:
    virtual ~ILedDriver() = default;

    /**
     * @brief Initialize the LED driver
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Set all LEDs to the same color
     * @param color RGBW color
     * @return ESP_OK on success
     */
    virtual esp_err_t setAll(Color color) = 0;

    /**
     * @brief Set a single LED
     * @param index LED index (0-15)
     * @param color RGBW color
     * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index out of range
     */
    virtual esp_err_t setLed(uint8_t index, Color color) = 0;

    /**
     * @brief Set LED brightness (global)
     * @param brightness 0-255 (0=off, 255=max)
     * @return ESP_OK on success
     */
    virtual esp_err_t setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Refresh LED display (send data to LEDs)
     * @return ESP_OK on success
     */
    virtual esp_err_t refresh() = 0;

    /**
     * @brief Check if driver is initialized
     */
    virtual bool isInitialized() const = 0;

    static constexpr uint8_t kLedCount = 16;
};

}  // namespace domes
```

### IAudioDriver

```cpp
// interfaces/i_audio_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for audio driver
 *
 * Controls MAX98357A I2S amplifier for sound playback.
 */
class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;

    virtual esp_err_t init() = 0;

    /**
     * @brief Play a tone
     * @param frequencyHz Tone frequency in Hz
     * @param durationMs Duration in milliseconds
     * @return ESP_OK on success
     */
    virtual esp_err_t playTone(uint16_t frequencyHz, uint16_t durationMs) = 0;

    /**
     * @brief Play a sound from SPIFFS
     * @param filename Path to .raw file (e.g., "/spiffs/hit.raw")
     * @return ESP_OK on success
     */
    virtual esp_err_t playSound(const char* filename) = 0;

    /**
     * @brief Stop any playing audio
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Set volume level
     * @param volume 0-255 (0=mute, 255=max)
     * @return ESP_OK on success
     */
    virtual esp_err_t setVolume(uint8_t volume) = 0;

    /**
     * @brief Check if audio is currently playing
     */
    virtual bool isPlaying() const = 0;

    virtual bool isInitialized() const = 0;
};

}  // namespace domes
```

### IHapticDriver

```cpp
// interfaces/i_haptic_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for haptic driver
 *
 * Controls DRV2605L haptic driver with LRA motor.
 */
class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;

    virtual esp_err_t init() = 0;

    /**
     * @brief Play a haptic effect
     * @param effectId DRV2605L effect ID (1-123)
     * @return ESP_OK on success
     */
    virtual esp_err_t playEffect(uint8_t effectId) = 0;

    /**
     * @brief Set vibration intensity
     * @param intensity 0-255 (0=off, 255=max)
     * @return ESP_OK on success
     */
    virtual esp_err_t setIntensity(uint8_t intensity) = 0;

    /**
     * @brief Stop any ongoing vibration
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    virtual bool isInitialized() const = 0;

    // Common effect IDs
    static constexpr uint8_t kEffectStrongClick = 1;
    static constexpr uint8_t kEffectSharpClick = 4;
    static constexpr uint8_t kEffectSoftBump = 7;
    static constexpr uint8_t kEffectDoubleClick = 10;
    static constexpr uint8_t kEffectTripleClick = 12;
    static constexpr uint8_t kEffectBuzz = 47;
};

}  // namespace domes
```

### ITouchDriver

```cpp
// interfaces/i_touch_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>
#include <optional>

namespace domes {

struct TouchEvent {
    uint32_t timestampUs;   // Microsecond timestamp
    uint8_t strength;       // Touch strength (0-255)
    bool isTap;             // True if detected as tap (vs hold)
};

/**
 * @brief Abstract interface for touch detection
 *
 * Combines ESP32 capacitive touch and LIS2DW12 tap detection.
 */
class ITouchDriver {
public:
    virtual ~ITouchDriver() = default;

    virtual esp_err_t init() = 0;

    /**
     * @brief Enable touch detection
     * @return ESP_OK on success
     */
    virtual esp_err_t enable() = 0;

    /**
     * @brief Disable touch detection
     * @return ESP_OK on success
     */
    virtual esp_err_t disable() = 0;

    /**
     * @brief Check if touch is currently detected
     * @return true if touched
     */
    virtual bool isTouched() const = 0;

    /**
     * @brief Get pending touch event (non-blocking)
     * @return TouchEvent if available, std::nullopt otherwise
     */
    virtual std::optional<TouchEvent> getEvent() = 0;

    /**
     * @brief Wait for touch event (blocking)
     * @param timeoutMs Timeout in milliseconds
     * @return TouchEvent if detected, std::nullopt on timeout
     */
    virtual std::optional<TouchEvent> waitForTouch(uint32_t timeoutMs) = 0;

    /**
     * @brief Set touch sensitivity threshold
     * @param threshold Lower = more sensitive (typical: 400-600)
     * @return ESP_OK on success
     */
    virtual esp_err_t setThreshold(uint16_t threshold) = 0;

    virtual bool isInitialized() const = 0;
};

}  // namespace domes
```

### IImuDriver

```cpp
// interfaces/i_imu_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

struct Acceleration {
    int16_t x;  // mg (milli-g)
    int16_t y;
    int16_t z;
};

/**
 * @brief Abstract interface for IMU driver
 *
 * Controls LIS2DW12 accelerometer for tap detection.
 */
class IImuDriver {
public:
    virtual ~IImuDriver() = default;

    virtual esp_err_t init() = 0;

    /**
     * @brief Read current acceleration
     * @param accel Output acceleration in mg
     * @return ESP_OK on success
     */
    virtual esp_err_t readAcceleration(Acceleration& accel) = 0;

    /**
     * @brief Enable tap detection interrupt
     * @return ESP_OK on success
     */
    virtual esp_err_t enableTapDetection() = 0;

    /**
     * @brief Check if tap was detected (clears flag)
     * @return true if tap detected since last check
     */
    virtual bool wasTapDetected() = 0;

    /**
     * @brief Set tap detection threshold
     * @param threshold Threshold value (device-specific)
     * @return ESP_OK on success
     */
    virtual esp_err_t setTapThreshold(uint8_t threshold) = 0;

    virtual bool isInitialized() const = 0;
};

}  // namespace domes
```

### IPowerDriver

```cpp
// interfaces/i_power_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for power management
 *
 * Handles battery monitoring and sleep modes.
 */
class IPowerDriver {
public:
    virtual ~IPowerDriver() = default;

    virtual esp_err_t init() = 0;

    /**
     * @brief Get battery voltage
     * @return Voltage in millivolts (e.g., 3700 = 3.7V)
     */
    virtual uint16_t getBatteryVoltageMv() = 0;

    /**
     * @brief Get battery percentage
     * @return Percentage 0-100
     */
    virtual uint8_t getBatteryPercent() = 0;

    /**
     * @brief Check if charging
     * @return true if connected to charger and charging
     */
    virtual bool isCharging() = 0;

    /**
     * @brief Enter light sleep
     * @param durationMs Sleep duration (0 = until interrupt)
     * @return ESP_OK on wake
     */
    virtual esp_err_t enterLightSleep(uint32_t durationMs) = 0;

    /**
     * @brief Enter deep sleep
     * @note Does not return - device reboots on wake
     */
    virtual void enterDeepSleep() = 0;

    virtual bool isInitialized() const = 0;
};

}  // namespace domes
```

---

## LED Driver Implementation

```cpp
// drivers/led_driver.hpp
#pragma once

#include "interfaces/i_led_driver.hpp"
#include "driver/rmt_tx.h"
#include <array>

namespace domes {

class LedDriver final : public ILedDriver {
public:
    explicit LedDriver(gpio_num_t dataPin);
    ~LedDriver();

    esp_err_t init() override;
    esp_err_t setAll(Color color) override;
    esp_err_t setLed(uint8_t index, Color color) override;
    esp_err_t setBrightness(uint8_t brightness) override;
    esp_err_t refresh() override;
    bool isInitialized() const override;

private:
    static constexpr const char* kTag = "led";

    gpio_num_t dataPin_;
    rmt_channel_handle_t rmtChannel_ = nullptr;
    rmt_encoder_handle_t encoder_ = nullptr;

    std::array<Color, kLedCount> colors_{};
    uint8_t brightness_ = 255;
    bool initialized_ = false;

    esp_err_t initRmt();
};

}  // namespace domes
```

```cpp
// drivers/led_driver.cpp
#include "led_driver.hpp"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"  // ESP-IDF example encoder

namespace domes {

LedDriver::LedDriver(gpio_num_t dataPin)
    : dataPin_(dataPin) {
}

LedDriver::~LedDriver() {
    if (rmtChannel_) {
        rmt_del_channel(rmtChannel_);
    }
    if (encoder_) {
        rmt_del_encoder(encoder_);
    }
}

esp_err_t LedDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = initRmt();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "RMT init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Start with all LEDs off
    setAll(Color::off());
    refresh();

    initialized_ = true;
    ESP_LOGI(kTag, "Initialized %d LEDs on GPIO %d", kLedCount, dataPin_);
    return ESP_OK;
}

esp_err_t LedDriver::initRmt() {
    rmt_tx_channel_config_t txConfig = {
        .gpio_num = dataPin_,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,  // 10MHz = 100ns resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    ESP_RETURN_ON_ERROR(
        rmt_new_tx_channel(&txConfig, &rmtChannel_),
        kTag, "Failed to create RMT channel"
    );

    ESP_RETURN_ON_ERROR(
        rmt_enable(rmtChannel_),
        kTag, "Failed to enable RMT channel"
    );

    // Create LED strip encoder (SK6812 timing)
    led_strip_encoder_config_t encoderConfig = {
        .resolution = 10000000,
    };

    ESP_RETURN_ON_ERROR(
        rmt_new_led_strip_encoder(&encoderConfig, &encoder_),
        kTag, "Failed to create encoder"
    );

    return ESP_OK;
}

esp_err_t LedDriver::setAll(Color color) {
    for (auto& c : colors_) {
        c = color;
    }
    return ESP_OK;
}

esp_err_t LedDriver::setLed(uint8_t index, Color color) {
    if (index >= kLedCount) {
        return ESP_ERR_INVALID_ARG;
    }
    colors_[index] = color;
    return ESP_OK;
}

esp_err_t LedDriver::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    return ESP_OK;
}

esp_err_t LedDriver::refresh() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    // Build pixel buffer (GRBW order for SK6812)
    uint8_t buffer[kLedCount * 4];
    for (size_t i = 0; i < kLedCount; i++) {
        const auto& c = colors_[i];
        uint8_t scale = brightness_;
        buffer[i * 4 + 0] = (c.g * scale) >> 8;
        buffer[i * 4 + 1] = (c.r * scale) >> 8;
        buffer[i * 4 + 2] = (c.b * scale) >> 8;
        buffer[i * 4 + 3] = (c.w * scale) >> 8;
    }

    rmt_transmit_config_t txConfig = {
        .loop_count = 0,
    };

    return rmt_transmit(rmtChannel_, encoder_, buffer, sizeof(buffer), &txConfig);
}

bool LedDriver::isInitialized() const {
    return initialized_;
}

}  // namespace domes
```

---

## Dependency Injection Pattern

### In main.cpp

```cpp
// main/main.cpp
#include "drivers/led_driver.hpp"
#include "drivers/audio_driver.hpp"
#include "drivers/haptic_driver.hpp"
#include "drivers/touch_driver.hpp"
#include "services/feedback_service.hpp"
#include "platform/pins.hpp"
#include <memory>

namespace {
    // Driver instances (owned by main)
    std::unique_ptr<domes::LedDriver> ledDriver;
    std::unique_ptr<domes::AudioDriver> audioDriver;
    std::unique_ptr<domes::HapticDriver> hapticDriver;
    std::unique_ptr<domes::TouchDriver> touchDriver;

    // Service instances
    std::unique_ptr<domes::FeedbackService> feedbackService;
}

esp_err_t initDrivers() {
    using namespace domes;
    using namespace domes::pins;

    // Create drivers
    ledDriver = std::make_unique<LedDriver>(kLedData);
    audioDriver = std::make_unique<AudioDriver>(kI2sBclk, kI2sLrclk, kI2sDout);
    hapticDriver = std::make_unique<HapticDriver>(kI2cSda, kI2cScl);
    touchDriver = std::make_unique<TouchDriver>(kTouchPad);

    // Initialize in order
    ESP_RETURN_ON_ERROR(ledDriver->init(), "main", "LED init failed");
    ESP_RETURN_ON_ERROR(audioDriver->init(), "main", "Audio init failed");
    ESP_RETURN_ON_ERROR(hapticDriver->init(), "main", "Haptic init failed");
    ESP_RETURN_ON_ERROR(touchDriver->init(), "main", "Touch init failed");

    return ESP_OK;
}

esp_err_t initServices() {
    using namespace domes;

    // Inject driver references into services
    feedbackService = std::make_unique<FeedbackService>(
        *ledDriver,
        *audioDriver,
        *hapticDriver
    );

    ESP_RETURN_ON_ERROR(feedbackService->init(), "main", "Feedback service init failed");

    return ESP_OK;
}

extern "C" void app_main() {
    ESP_ERROR_CHECK(initDrivers());
    ESP_ERROR_CHECK(initServices());

    // Start application...
}
```

### Service Example

```cpp
// services/feedback_service.hpp
#pragma once

#include "interfaces/i_led_driver.hpp"
#include "interfaces/i_audio_driver.hpp"
#include "interfaces/i_haptic_driver.hpp"
#include "esp_err.h"

namespace domes {

/**
 * @brief Coordinates multi-modal feedback (LED + audio + haptic)
 */
class FeedbackService {
public:
    FeedbackService(ILedDriver& led, IAudioDriver& audio, IHapticDriver& haptic);

    esp_err_t init();

    /**
     * @brief Play success feedback (green flash + beep + vibrate)
     */
    esp_err_t playSuccess();

    /**
     * @brief Play error feedback (red flash + buzz)
     */
    esp_err_t playError();

    /**
     * @brief Play touch feedback (brief pulse)
     */
    esp_err_t playTouchFeedback();

private:
    ILedDriver& led_;
    IAudioDriver& audio_;
    IHapticDriver& haptic_;
};

}  // namespace domes
```

```cpp
// services/feedback_service.cpp
#include "feedback_service.hpp"
#include "esp_log.h"

namespace domes {

namespace {
    constexpr const char* kTag = "feedback";
}

FeedbackService::FeedbackService(ILedDriver& led, IAudioDriver& audio, IHapticDriver& haptic)
    : led_(led)
    , audio_(audio)
    , haptic_(haptic) {
}

esp_err_t FeedbackService::init() {
    ESP_LOGI(kTag, "Initialized");
    return ESP_OK;
}

esp_err_t FeedbackService::playSuccess() {
    led_.setAll(Color::green());
    led_.refresh();
    audio_.playTone(1000, 100);
    haptic_.playEffect(IHapticDriver::kEffectStrongClick);
    return ESP_OK;
}

esp_err_t FeedbackService::playError() {
    led_.setAll(Color::red());
    led_.refresh();
    audio_.playTone(200, 300);
    haptic_.playEffect(IHapticDriver::kEffectBuzz);
    return ESP_OK;
}

esp_err_t FeedbackService::playTouchFeedback() {
    haptic_.playEffect(IHapticDriver::kEffectSharpClick);
    return ESP_OK;
}

}  // namespace domes
```

---

## Driver Development Workflow

### Step 1: Create Interface
1. Define in `interfaces/i_xxx_driver.hpp`
2. All methods virtual, return `esp_err_t`
3. Add `isInitialized()` method
4. Document with Doxygen

### Step 2: Create Implementation
1. Header in `drivers/xxx_driver.hpp`
2. Source in `drivers/xxx_driver.cpp`
3. Implement all interface methods
4. Add private `kTag` for logging

### Step 3: Create Mock
1. Create `test/mocks/mock_xxx_driver.hpp`
2. Add test control variables
3. Default to success returns

### Step 4: Add to Build
1. Add `.cpp` to `main/CMakeLists.txt` SRCS
2. Add any new REQUIRES components

### Step 5: Verify
```bash
idf.py build  # Compiles without errors

# If DevKit available:
idf.py flash monitor  # Driver logs "Initialized"
```

---

## Initialization Order

```
1. NVS (required for WiFi/BLE config)
2. I2C bus (shared by haptic + IMU)
3. Power driver (ADC for battery)
4. LED driver (visual feedback for boot)
5. IMU driver (I2C)
6. Haptic driver (I2C)
7. Audio driver (I2S)
8. Touch driver (depends on IMU)
9. Services (depend on drivers)
10. RF stacks (WiFi, BLE, ESP-NOW)
```

---

*Prerequisites: 00-getting-started.md, 01-project-structure.md*
*Related: 06-testing.md (mocks), 09-reference.md (pins)*
