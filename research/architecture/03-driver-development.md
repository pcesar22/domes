# 03 - Driver Development

## AI Agent Instructions

Load this file when implementing or modifying hardware drivers.

Prerequisites: `01-project-structure.md`

---

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              APPLICATION LAYER                               │
│                                                                              │
│   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                    │
│   │ Game Engine  │   │ Drill Manager│   │ State Machine│                    │
│   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                    │
│          │                  │                  │                            │
├──────────┴──────────────────┴──────────────────┴────────────────────────────┤
│                              SERVICE LAYER                                   │
│                                                                              │
│   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                    │
│   │   Feedback   │   │    Comm      │   │    Timing    │                    │
│   │   Service    │   │   Service    │   │   Service    │                    │
│   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                    │
│          │                  │                  │                            │
├──────────┴──────────────────┴──────────────────┴────────────────────────────┤
│                              DRIVER LAYER                                    │
│                                                                              │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐        │
│   │  LED   │ │ Audio  │ │ Haptic │ │ Touch  │ │  IMU   │ │ Power  │        │
│   │ Driver │ │ Driver │ │ Driver │ │ Driver │ │ Driver │ │ Driver │        │
│   └────┬───┘ └────┬───┘ └────┬───┘ └────┬───┘ └────┬───┘ └────┬───┘        │
│        │          │          │          │          │          │             │
├────────┴──────────┴──────────┴──────────┴──────────┴──────────┴─────────────┤
│                              HARDWARE LAYER                                  │
│                                                                              │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐        │
│   │SK6812  │ │MAX98357│ │DRV2605L│ │TouchPad│ │LIS2DW12│ │Battery │        │
│   │ (RMT)  │ │ (I2S)  │ │ (I2C)  │ │(GPIO)  │ │ (I2C)  │ │ (ADC)  │        │
│   └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Peripheral Interconnect

```
                          ┌─────────────────────┐
                          │     ESP32-S3        │
                          │                     │
       RMT ──────────────►│ GPIO48 ─────► LED Data
                          │                     │
       I2S ──────────────►│ GPIO15 ─────► BCLK
                          │ GPIO16 ─────► LRCLK
                          │ GPIO17 ─────► DOUT ────► MAX98357A
                          │                     │
       I2C ──────────────►│ GPIO1  ─────► SDA ─┬──► DRV2605L (0x5A)
       (shared bus)       │ GPIO2  ─────► SCL ─┴──► LIS2DW12 (0x18)
                          │                     │
       Touch ────────────►│ GPIO4  ─────► TOUCH_PAD
                          │                     │
       ADC ──────────────►│ GPIO3  ─────► VBAT_SENSE
                          │                     │
                          └─────────────────────┘
```

---

## Driver Interfaces

All drivers implement a common pattern:

```
┌──────────────────────────────────────────────┐
│               IXxxDriver                      │
├──────────────────────────────────────────────┤
│ + init() → esp_err_t                         │
│ + deinit() → esp_err_t                       │
│ + isInitialized() → bool                     │
├──────────────────────────────────────────────┤
│   [driver-specific methods]                   │
└──────────────────────────────────────────────┘
```

### Interface Summary

| Interface | Primary Methods | Hardware |
|-----------|-----------------|----------|
| `ILedDriver` | `setAll()`, `setLed()`, `refresh()` | SK6812 RGBW ring |
| `IAudioDriver` | `playTone()`, `playSound()`, `stop()` | MAX98357A + speaker |
| `IHapticDriver` | `playEffect()`, `stop()` | DRV2605L + LRA |
| `ITouchDriver` | `enable()`, `onTouch()`, `waitForTouch()` | Capacitive + IMU |
| `IImuDriver` | `readAcceleration()`, `enableTapDetection()` | LIS2DW12 |
| `IPowerDriver` | `getBatteryPercent()`, `enterDeepSleep()` | ADC + GPIO |

---

## Interface Definitions (Virtual Base Classes)

```cpp
// EXAMPLE: Minimal interface pattern

class ILedDriver {
public:
    virtual ~ILedDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t deinit() = 0;
    virtual esp_err_t setAll(Color color) = 0;
    virtual esp_err_t setLed(uint8_t index, Color color) = 0;
    virtual esp_err_t refresh() = 0;
    virtual bool isInitialized() const = 0;
};

class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t playTone(uint16_t freqHz, uint16_t durationMs) = 0;
    virtual esp_err_t playSound(const char* filename) = 0;
    virtual esp_err_t stop() = 0;
    virtual bool isPlaying() const = 0;
};

class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t playEffect(uint8_t effectId) = 0;
    virtual esp_err_t stop() = 0;
    virtual bool isPlaying() const = 0;
};

class ITouchDriver {
public:
    using TouchCallback = std::function<void(const TouchEvent&)>;
    virtual ~ITouchDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t enable() = 0;
    virtual bool isTouched() const = 0;
    virtual void onTouch(TouchCallback cb) = 0;
};

class IImuDriver {
public:
    virtual ~IImuDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t readAcceleration(Acceleration& out) = 0;
    virtual esp_err_t enableTapDetection() = 0;
    virtual bool wasTapDetected() = 0;
};

class IPowerDriver {
public:
    virtual ~IPowerDriver() = default;
    virtual esp_err_t init() = 0;
    virtual uint8_t getBatteryPercent() = 0;
    virtual bool isCharging() = 0;
    virtual void enterDeepSleep() = 0;
};
```

---

## Dependency Injection

```
┌─────────────────┐
│    main.cpp     │
│                 │
│  Creates:       │──────┐
│  - LedDriver    │      │  owns
│  - AudioDriver  │      │
│  - HapticDriver │      ▼
│  - TouchDriver  │  ┌─────────────────┐
│                 │  │  FeedbackService │
│  Injects refs   │──│                  │
│  into services  │  │  Uses:           │
└─────────────────┘  │  - ILedDriver&   │
                     │  - IAudioDriver& │
                     │  - IHapticDriver&│
                     └─────────────────┘
```

```cpp
// EXAMPLE: Dependency injection in main.cpp

// Create concrete drivers
auto led = std::make_unique<LedDriver>(GPIO_NUM_48);
auto audio = std::make_unique<AudioDriver>(/* pins */);
auto haptic = std::make_unique<HapticDriver>(/* i2c */);

// Inject interfaces into services
FeedbackService feedback(*led, *audio, *haptic);
```

---

## I2C Bus Sharing

The I2C bus is shared between IMU and Haptic drivers. Use mutex protection.

```
┌─────────────────────────────────────────────────┐
│                  I2C Bus (400kHz)               │
├─────────────────────────────────────────────────┤
│                                                 │
│    ┌───────────┐          ┌───────────┐        │
│    │  LIS2DW12 │          │  DRV2605L │        │
│    │   (IMU)   │          │  (Haptic) │        │
│    │  Addr:0x18│          │  Addr:0x5A│        │
│    └───────────┘          └───────────┘        │
│          │                      │              │
│          └──────────┬───────────┘              │
│                     │                          │
│              ┌──────┴──────┐                   │
│              │  I2cManager │                   │
│              │   (mutex)   │                   │
│              └─────────────┘                   │
│                                                │
└─────────────────────────────────────────────────┘
```

---

## Initialization Order

```
┌─────────────────────────────────────────────────────────────┐
│                    INITIALIZATION FLOW                       │
│                                                              │
│   ① NVS ───────► Required for WiFi/BLE config               │
│        │                                                     │
│        ▼                                                     │
│   ② I2C Bus ───► Shared by haptic + IMU                     │
│        │                                                     │
│        ▼                                                     │
│   ③ Power ─────► ADC for battery (early for status)         │
│        │                                                     │
│        ▼                                                     │
│   ④ LED ───────► Visual boot feedback                       │
│        │                                                     │
│        ▼                                                     │
│   ⑤ IMU ───────► I2C device                                 │
│        │                                                     │
│        ▼                                                     │
│   ⑥ Haptic ────► I2C device                                 │
│        │                                                     │
│        ▼                                                     │
│   ⑦ Audio ─────► I2S peripheral                             │
│        │                                                     │
│        ▼                                                     │
│   ⑧ Touch ─────► Depends on IMU for fusion                  │
│        │                                                     │
│        ▼                                                     │
│   ⑨ Services ──► Depend on drivers                          │
│        │                                                     │
│        ▼                                                     │
│   ⑩ RF Stacks ─► WiFi, BLE, ESP-NOW                         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Types

```cpp
// EXAMPLE: Common data structures

struct Color {
    uint8_t r, g, b, w;
    static constexpr Color red()   { return {255, 0, 0, 0}; }
    static constexpr Color green() { return {0, 255, 0, 0}; }
    static constexpr Color off()   { return {0, 0, 0, 0}; }
};

struct TouchEvent {
    uint64_t timestampUs;
    uint8_t strength;
    bool isTap;
};

struct Acceleration {
    int16_t x, y, z;  // milli-g
};
```

---

## Testing with Mocks

```
┌────────────────────────────────────────────────────────────┐
│                    TEST CONFIGURATION                       │
│                                                             │
│   Production:              Test:                            │
│   ┌──────────────┐        ┌──────────────┐                 │
│   │  LedDriver   │        │ MockLedDriver│                 │
│   │  (hardware)  │        │ (no-op/stub) │                 │
│   └──────┬───────┘        └──────┬───────┘                 │
│          │                       │                          │
│          └───────────┬───────────┘                          │
│                      │                                      │
│                      ▼                                      │
│               ┌──────────────┐                              │
│               │  ILedDriver  │  ◄── Same interface          │
│               └──────────────┘                              │
│                      │                                      │
│                      ▼                                      │
│               ┌──────────────┐                              │
│               │ GameEngine   │  ◄── Doesn't know            │
│               └──────────────┘      which impl              │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

---

*Related: `06-testing.md` (mocks), `09-reference.md` (pin map)*
