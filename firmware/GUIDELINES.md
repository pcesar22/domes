# DOMES Firmware Development Guidelines

## Overview

This document defines the coding standards and architectural decisions for the DOMES firmware project. All contributors must adhere to these guidelines.

**Target Platform:** ESP32-S3-WROOM-1-N16R8
**Framework:** ESP-IDF v5.x
**Language:** C++20

---

## 1. Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Files | camelCase | `ledDriver.hpp`, `feedbackService.cpp` |
| Classes | PascalCase | `LedDriver`, `FeedbackService` |
| Interfaces | I + PascalCase | `IHapticDriver`, `IAudioDriver` |
| Methods/Functions | camelCase | `init()`, `playEffect()`, `setColor()` |
| Variables | camelCase | `reactionTime`, `ledCount` |
| Member variables | camelCase + trailing `_` | `ledDriver_`, `intensity_` |
| Constants | k + PascalCase | `kTag`, `kDeviceAddress`, `kMaxRetries` |
| Namespaces | lowercase | `pins`, `config`, `utils` |
| Macros | SCREAMING_SNAKE_CASE | `CONFIG_DOMES_LED_COUNT` |

---

## 2. Memory Management

### 2.1 Heap Allocation Policy

**Allowed during initialization only.** After `app_main()` completes setup, no further heap allocation should occur.

```cpp
// ALLOWED: Factory pattern at startup
auto haptic = createHapticDriver(pins::kI2cSda, pins::kI2cScl);

// ALLOWED: std::make_unique during init
auto audio = std::make_unique<AudioDriver>(pins::kI2sBclk, pins::kI2sLrclk, pins::kI2sDout);

// FORBIDDEN: Allocation during runtime
void onTouchEvent() {
    auto buffer = new uint8_t[256];  // NO! Allocates after init
}
```

### 2.2 Containers

Use **ETL (Embedded Template Library)** for all containers. Standard STL containers that allocate dynamically are forbidden.

```cpp
// FORBIDDEN
std::vector<int> values;
std::string name;
std::map<int, Data> lookup;

// ALLOWED
etl::vector<int, 32> values;          // Fixed capacity 32
etl::string<64> name;                  // Fixed capacity 64 chars
etl::map<int, Data, 16> lookup;        // Fixed capacity 16 entries
```

### 2.3 FreeRTOS Primitives

Prefer static allocation for tasks, queues, and semaphores. Dynamic allocation is acceptable during init.

```cpp
// Preferred: Static allocation
class AudioTask {
    static inline StackType_t stack_[4096];
    static inline StaticTask_t tcb_;
    TaskHandle_t handle_;

public:
    void start() {
        handle_ = xTaskCreateStatic(
            taskFunc, "audio", 4096,
            this, 5, stack_, &tcb_
        );
    }
};

// Acceptable during init only
xTaskCreate(taskFunc, "audio", 4096, this, 5, &handle_);
```

---

## 3. Error Handling

### 3.1 No Exceptions

C++ exceptions are **disabled**. Use `std::expected<T, esp_err_t>` for error handling.

```cpp
#include "utils/expected.hpp"  // tl::expected backport

// Return expected for operations that can fail
tl::expected<SensorReading, esp_err_t> readSensor() {
    uint8_t data[4];
    esp_err_t err = i2cRead(kSensorAddr, data, sizeof(data));
    if (err != ESP_OK) {
        return tl::unexpected(err);
    }
    return SensorReading{data};
}

// Usage
auto result = readSensor();
if (!result) {
    ESP_LOGE(kTag, "Read failed: %s", esp_err_to_name(result.error()));
    return result.error();
}
auto reading = result.value();
```

### 3.2 ESP-IDF Error Macros

For simple propagation, use ESP-IDF macros:

```cpp
esp_err_t init() {
    ESP_RETURN_ON_ERROR(driverA.init(), kTag, "Driver A init failed");
    ESP_RETURN_ON_ERROR(driverB.init(), kTag, "Driver B init failed");
    return ESP_OK;
}
```

---

## 4. Forbidden Features

| Feature | Reason | Alternative |
|---------|--------|-------------|
| `<iostream>` | Adds ~200KB binary size | `ESP_LOGx` macros |
| C++ exceptions | Disabled, adds overhead | `std::expected` |
| RTTI | Disabled, adds overhead | Design without `dynamic_cast` |
| `std::vector`, `std::string`, `std::map` | Dynamic allocation | ETL equivalents |
| `new`/`delete` after init | Heap fragmentation | Pre-allocate at startup |
| `malloc`/`free` after init | Heap fragmentation | Static allocation |

---

## 5. Interrupt Service Routines (ISR)

### 5.1 Minimal ISR Policy

ISRs should do minimal work and defer processing to tasks:

```cpp
void IRAM_ATTR touchIsr(void* arg) {
    auto* self = static_cast<TouchDriver*>(arg);
    BaseType_t woken = pdFALSE;

    // Capture timestamp (IRAM-safe)
    uint32_t timestamp = esp_timer_get_time();

    // Defer to task
    xQueueSendFromISR(self->queue_, &timestamp, &woken);
    portYIELD_FROM_ISR(woken);
}
```

### 5.2 ISR Requirements

When code must execute in ISR context:

- Function **must** have `IRAM_ATTR`
- All accessed data **must** have `DRAM_ATTR`
- No logging (`ESP_LOGx`)
- No heap allocation
- No floating point operations
- Complete in <10μs
- Only call IRAM-safe functions (FreeRTOS `*FromISR` APIs are safe)

```cpp
DRAM_ATTR static volatile uint32_t lastEventTime;

void IRAM_ATTR gpioIsr(void* arg) {
    lastEventTime = esp_timer_get_time();  // OK: DRAM variable
}
```

---

## 6. Logging

Use ESP-IDF logging macros with per-module tags:

```cpp
static constexpr const char* kTag = "haptic";

ESP_LOGE(kTag, "Error: %s", esp_err_to_name(err));   // Errors
ESP_LOGW(kTag, "Warning: threshold adjusted");       // Warnings
ESP_LOGI(kTag, "Initialized successfully");          // Info
ESP_LOGD(kTag, "Register 0x%02X = 0x%02X", reg, val); // Debug
ESP_LOGV(kTag, "Entering function");                 // Verbose
```

Runtime level control:
```cpp
esp_log_level_set("haptic", ESP_LOG_DEBUG);  // Per-module
esp_log_level_set("*", ESP_LOG_INFO);        // Default
```

### 6.1 Performance Tracing

For post-mortem performance analysis, use the tracing framework:

```cpp
#include "trace/traceApi.hpp"

void processGameTick() {
    // RAII scope trace - begin on entry, end on exit
    TRACE_SCOPE(TRACE_ID("Game.Tick"), domes::trace::TraceCategory::kGame);

    // Point-in-time events
    TRACE_INSTANT(TRACE_ID("Game.Hit"), domes::trace::TraceCategory::kGame);

    // Counter values over time
    TRACE_COUNTER(TRACE_ID("Game.Score"), score, domes::trace::TraceCategory::kGame);
}
```

Dump and visualize with Perfetto:
```bash
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json
# Open trace.json at https://ui.perfetto.dev
```

---

## 7. Integer Types

| Context | Type | Example |
|---------|------|---------|
| Hardware registers | Fixed-width | `uint8_t reg = 0x5A;` |
| Protocol fields | Fixed-width | `uint16_t packetLen;` |
| Buffers/arrays | Fixed-width | `uint8_t buffer[256];` |
| Timestamps | Fixed-width | `uint32_t timestampUs;` |
| Loop counters | Natural | `for (int i = 0; i < n; ++i)` |
| General logic | Natural | `unsigned retryCount = 0;` |

---

## 8. Ownership and Pointers

### 8.1 Ownership Model

- `std::unique_ptr` for factory-created objects at initialization
- References for dependency injection after init
- Raw pointers only for C API interop or nullable references

```cpp
// Factory returns unique_ptr (ownership transfer)
std::unique_ptr<HapticDriver> createHapticDriver(gpio_num_t sda, gpio_num_t scl);

// app_main owns objects
auto haptic = createHapticDriver(pins::kI2cSda, pins::kI2cScl);
auto audio = createAudioDriver(pins::kI2sBclk, pins::kI2sLrclk, pins::kI2sDout);

// Services receive references (no ownership)
class FeedbackService {
public:
    FeedbackService(IHapticDriver& haptic, IAudioDriver& audio);
private:
    IHapticDriver& haptic_;
    IAudioDriver& audio_;
};

FeedbackService feedback(*haptic, *audio);
```

---

## 9. Const Correctness

Strict const-correctness is required:

```cpp
// Non-mutating methods are const
bool isPlaying() const;
esp_err_t getStatus(Status& out) const;

// Non-mutating parameters are const
esp_err_t setColors(etl::span<const Color> colors);
void processEvent(const TouchEvent& event);

// Compile-time constants use constexpr
static constexpr uint8_t kMaxEffects = 128;
static constexpr const char* kTag = "driver";

// Lookup tables in flash
constexpr std::array<uint16_t, 256> kGammaTable = {{
    0, 1, 2, 4, 7, 11, /* ... */
}};
```

---

## 10. Thread Safety

Use RAII lock guards for mutex protection:

```cpp
class Mutex {
public:
    Mutex() { handle_ = xSemaphoreCreateMutexStatic(&buffer_); }
    void lock() { xSemaphoreTake(handle_, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(handle_); }
private:
    StaticSemaphore_t buffer_;
    SemaphoreHandle_t handle_;
};

class MutexGuard {
public:
    explicit MutexGuard(Mutex& m) : mutex_(m) { mutex_.lock(); }
    ~MutexGuard() { mutex_.unlock(); }
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
private:
    Mutex& mutex_;
};

// Usage
void SharedResource::update(int value) {
    MutexGuard lock(mutex_);
    value_ = value;
}  // Auto-unlocks
```

---

## 11. Documentation

Use Doxygen-style comments for all public APIs:

```cpp
/**
 * @brief Initialize the haptic driver
 *
 * Configures I2C communication and sets the driver to LRA mode.
 * Must be called before any other haptic methods.
 *
 * @param intensity Initial vibration intensity (0-255)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if already initialized
 * @return ESP_ERR_TIMEOUT if I2C communication fails
 *
 * @note Thread-safe after initialization
 * @warning Must be called from a task context, not ISR
 */
esp_err_t init(uint8_t intensity);
```

---

## 12. Constants Organization

### 12.1 Central Configuration

Cross-cutting values in `config/constants.hpp`:

```cpp
namespace config {
    constexpr uint32_t kDefaultTimeoutMs = 1000;
    constexpr size_t kMaxPods = 24;
    constexpr size_t kEventQueueSize = 16;
}
```

### 12.2 Module-Specific

Driver/service-specific values in their headers:

```cpp
class LedDriver {
    static constexpr uint8_t kDefaultBrightness = 128;
    static constexpr uint32_t kRefreshRateHz = 60;
};
```

### 12.3 Platform-Specific

Pin mappings and hardware config in platform headers:

```cpp
// platform/pinsDevkit.hpp
namespace pins {
    constexpr gpio_num_t kLedData = GPIO_NUM_48;
    constexpr gpio_num_t kI2cSda = GPIO_NUM_1;
}
```

---

## 13. File Organization

### 13.1 One Class Per File

Each class gets its own `.hpp`/`.cpp` pair with matching names:

```
drivers/
├── ledDriver.hpp
├── ledDriver.cpp
├── hapticDriver.hpp
└── hapticDriver.cpp
```

### 13.2 Include Guards

Use `#pragma once`:

```cpp
#pragma once

#include "esp_err.h"
// ...
```

### 13.3 Include Order

1. Corresponding header (for `.cpp` files)
2. ESP-IDF headers
3. Standard library headers
4. ETL headers
5. Project headers

```cpp
// hapticDriver.cpp
#include "hapticDriver.hpp"

#include "driver/i2c.h"
#include "esp_log.h"

#include <cstdint>

#include "etl/vector.h"

#include "platform/pins.hpp"
#include "interfaces/iHapticDriver.hpp"
```

---

## 14. Formatting

| Rule | Value |
|------|-------|
| Line length | 100 characters max |
| Indentation | 4 spaces (no tabs) |
| Braces | K&R style (opening brace same line) |
| Pointer/reference | `Type* ptr`, `Type& ref` (attached to type) |

```cpp
esp_err_t HapticDriver::playEffect(uint8_t effectId) {
    if (effectId >= kMaxEffects) {
        ESP_LOGE(kTag, "Invalid effect ID: %d", effectId);
        return ESP_ERR_INVALID_ARG;
    }

    MutexGuard lock(mutex_);
    return writeRegister(kRegWaveform, effectId);
}
```

---

## 15. Platform Abstraction

### 15.1 Platform Selection

Platforms are selected via Kconfig at build time:

```bash
# Build for DevKit (default)
idf.py build

# Build for PCB v1
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.pcbV1" build
```

### 15.2 Pin Configuration

Platform-specific pins in separate headers, unified via `pins.hpp`:

```cpp
// platform/pins.hpp
#pragma once

#if defined(CONFIG_DOMES_PLATFORM_DEVKIT)
    #include "pinsDevkit.hpp"
#elif defined(CONFIG_DOMES_PLATFORM_PCB_V1)
    #include "pinsPcbV1.hpp"
#else
    #error "No platform selected"
#endif
```

### 15.3 Feature Flags

Hardware features gated by Kconfig:

```cpp
#if CONFIG_DOMES_HAS_HAPTIC
    auto haptic = std::make_unique<HapticDriver>(pins::kI2cSda, pins::kI2cScl);
#else
    auto haptic = std::make_unique<HapticStub>();
#endif
```

---

## 16. Dependencies

| Library | Purpose | Integration |
|---------|---------|-------------|
| **ETL** | Fixed-capacity containers | ESP-IDF component |
| **tl::expected** | Error handling | Header in `utils/` |

---

## Checklist Before Commit

- [ ] No `new`/`malloc` after initialization phase
- [ ] No `std::vector`, `std::string`, `std::map` (use ETL)
- [ ] No `<iostream>` includes
- [ ] All public APIs have Doxygen comments
- [ ] All non-mutating methods are `const`
- [ ] All ISR code has `IRAM_ATTR`, data has `DRAM_ATTR`
- [ ] Error returns use `std::expected` or `esp_err_t`
- [ ] Mutex access uses `MutexGuard` RAII wrapper
- [ ] Compiles with no warnings: `idf.py build`

---

*Document Created: 2026-01-03*
*Project: DOMES Firmware*
