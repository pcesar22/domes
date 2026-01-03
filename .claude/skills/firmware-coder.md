# Firmware Coder Agent

Expert firmware developer for the DOMES ESP32-S3 project. Writes production-quality embedded C++20 code following strict project conventions.

---

## Activation

Use this skill when:
- Writing new firmware drivers, services, or components
- Implementing features in the firmware codebase
- Fixing firmware bugs or refactoring code
- Creating interfaces, mocks, or tests for firmware

---

## Project Context

**DOMES** (Distributed Open-source Motion & Exercise System) - Reaction training pods with LEDs, audio, haptics, touch sensing, ESP-NOW + BLE communication.

| Spec | Value |
|------|-------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB PSRAM) |
| Framework | ESP-IDF v5.x |
| Language | C++20 |
| RTOS | FreeRTOS |

---

## Mandatory Conventions

### Naming Standards

```
Files:           camelCase          ledDriver.hpp, feedbackService.cpp
Classes:         PascalCase         LedDriver, FeedbackService
Interfaces:      I + PascalCase     IHapticDriver, IAudioDriver
Methods:         camelCase          init(), playEffect(), setColor()
Variables:       camelCase          reactionTime, ledCount
Members:         camelCase + _      ledDriver_, intensity_
Constants:       k + PascalCase     kTag, kDeviceAddress, kMaxRetries
Namespaces:      lowercase          pins, config, utils
Macros:          SCREAMING_SNAKE    CONFIG_DOMES_LED_COUNT
```

### Memory Rules (CRITICAL)

**Heap allocation allowed ONLY during initialization.**

```cpp
// ALLOWED: Factory at startup
auto haptic = createHapticDriver(pins::kI2cSda, pins::kI2cScl);

// FORBIDDEN: Runtime allocation
void onEvent() {
    auto buf = new uint8_t[256];  // NEVER
}
```

**Use ETL containers, not STL:**

```cpp
// FORBIDDEN
std::vector<int> values;
std::string name;

// REQUIRED
etl::vector<int, 32> values;
etl::string<64> name;
```

### Error Handling

No exceptions. Use `tl::expected<T, esp_err_t>`:

```cpp
tl::expected<Reading, esp_err_t> readSensor() {
    uint8_t data[4];
    esp_err_t err = i2cRead(kAddr, data, sizeof(data));
    if (err != ESP_OK) {
        return tl::unexpected(err);
    }
    return Reading{data};
}
```

Or ESP-IDF macros for simple propagation:

```cpp
esp_err_t init() {
    ESP_RETURN_ON_ERROR(driverA.init(), kTag, "A failed");
    ESP_RETURN_ON_ERROR(driverB.init(), kTag, "B failed");
    return ESP_OK;
}
```

### ISR Requirements

```cpp
void IRAM_ATTR touchIsr(void* arg) {
    auto* self = static_cast<TouchDriver*>(arg);
    BaseType_t woken = pdFALSE;
    uint32_t ts = esp_timer_get_time();
    xQueueSendFromISR(self->queue_, &ts, &woken);
    portYIELD_FROM_ISR(woken);
}
```

- `IRAM_ATTR` on function
- `DRAM_ATTR` on accessed data
- No logging, no heap, no floats
- Complete in <10μs
- Only `*FromISR` FreeRTOS APIs

### Logging

```cpp
static constexpr const char* kTag = "module";

ESP_LOGE(kTag, "Error: %s", esp_err_to_name(err));
ESP_LOGW(kTag, "Warning message");
ESP_LOGI(kTag, "Info: initialized");
ESP_LOGD(kTag, "Debug: reg=0x%02X", reg);
```

### Thread Safety

```cpp
class MutexGuard {
public:
    explicit MutexGuard(Mutex& m) : mutex_(m) { mutex_.lock(); }
    ~MutexGuard() { mutex_.unlock(); }
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
private:
    Mutex& mutex_;
};

void Resource::update(int val) {
    MutexGuard lock(mutex_);
    value_ = val;
}  // Auto-unlocks
```

---

## Forbidden Features

| Feature | Why | Alternative |
|---------|-----|-------------|
| `<iostream>` | +200KB | `ESP_LOGx` |
| Exceptions | Disabled | `tl::expected` |
| RTTI | Disabled | Design without `dynamic_cast` |
| `std::vector/string/map` | Dynamic alloc | ETL equivalents |
| `new/delete` after init | Fragmentation | Pre-allocate |

---

## File Structure

```cpp
// myDriver.hpp
#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#include <cstdint>

#include "etl/vector.h"

#include "interfaces/iMyDriver.hpp"

class MyDriver : public IMyDriver {
public:
    esp_err_t init() override;

private:
    static constexpr const char* kTag = "mydriver";
    static constexpr uint8_t kMaxItems = 32;

    etl::vector<uint8_t, kMaxItems> buffer_;
    Mutex mutex_;
};
```

Include order:
1. Corresponding header
2. ESP-IDF headers
3. Standard library
4. ETL headers
5. Project headers

---

## Architecture Pattern

Every driver needs an interface:

```cpp
// interfaces/iHapticDriver.hpp
class IHapticDriver {
public:
    virtual ~IHapticDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t playEffect(uint8_t effectId) = 0;
};
```

Services use dependency injection:

```cpp
class FeedbackService {
public:
    FeedbackService(IHapticDriver& haptic, IAudioDriver& audio)
        : haptic_(haptic), audio_(audio) {}
private:
    IHapticDriver& haptic_;
    IAudioDriver& audio_;
};
```

---

## Task Pinning

- **Core 0**: WiFi, BLE, ESP-NOW (protocol stack)
- **Core 1**: Audio, game logic (user-responsive)
- **Either**: LED updates, touch polling

---

## Formatting

| Rule | Value |
|------|-------|
| Line length | 100 chars max |
| Indentation | 4 spaces |
| Braces | K&R style |
| Pointers | `Type* ptr` (attached to type) |

---

## Pre-Commit Checklist

Before completing ANY firmware task, verify:

- [ ] No `new`/`malloc` after init phase
- [ ] No `std::vector`, `std::string`, `std::map`
- [ ] No `<iostream>` includes
- [ ] All public APIs have Doxygen comments
- [ ] All non-mutating methods are `const`
- [ ] ISR code has `IRAM_ATTR`, data has `DRAM_ATTR`
- [ ] Errors use `tl::expected` or `esp_err_t`
- [ ] Mutex access uses `MutexGuard` RAII
- [ ] Compiles: `idf.py build` with no warnings
- [ ] Interface exists for every driver
- [ ] Key operations have logging

---

## Reference Documents

When you need more detail:
- `firmware/GUIDELINES.md` - Complete coding standards
- `research/SOFTWARE_ARCHITECTURE.md` - Design decisions
- `research/SYSTEM_ARCHITECTURE.md` - Hardware specs, pins
- `research/DEVELOPMENT_ROADMAP.md` - Task dependencies

---

## Workflow

1. **Read first** - Understand existing code before modifying
2. **Interface first** - Define interface before implementation
3. **Minimal ISRs** - Defer work to tasks via queues
4. **Test with mocks** - Every interface gets a mock
5. **Check before commit** - Run the checklist above
