# 01 - Project Structure

## AI Agent Instructions

Load this file when:
- Creating new source files
- Unsure about naming conventions
- Adding new modules or components

Prerequisites: `00-getting-started.md` completed

---

## Directory Layout

```
firmware/
├── CMakeLists.txt              # Root CMake (project definition)
├── sdkconfig.defaults          # Default menuconfig options
├── sdkconfig.defaults.devkit   # DevKit-specific overrides
├── sdkconfig.defaults.pcb      # PCB-specific overrides
├── partitions.csv              # Flash partition table
│
├── main/
│   ├── CMakeLists.txt          # Main component CMake
│   ├── main.cpp                # Entry point (app_main)
│   ├── config.hpp              # Pin definitions, constants
│   │
│   ├── interfaces/             # Abstract base classes
│   │   ├── i_led_driver.hpp
│   │   ├── i_audio_driver.hpp
│   │   ├── i_haptic_driver.hpp
│   │   ├── i_touch_driver.hpp
│   │   ├── i_imu_driver.hpp
│   │   └── i_power_driver.hpp
│   │
│   ├── drivers/                # Hardware driver implementations
│   │   ├── led_driver.hpp
│   │   ├── led_driver.cpp
│   │   ├── audio_driver.hpp
│   │   ├── audio_driver.cpp
│   │   ├── haptic_driver.hpp
│   │   ├── haptic_driver.cpp
│   │   ├── touch_driver.hpp
│   │   ├── touch_driver.cpp
│   │   ├── imu_driver.hpp
│   │   ├── imu_driver.cpp
│   │   ├── power_driver.hpp
│   │   └── power_driver.cpp
│   │
│   ├── services/               # Business logic services
│   │   ├── feedback_service.hpp
│   │   ├── feedback_service.cpp
│   │   ├── comm_service.hpp
│   │   ├── comm_service.cpp
│   │   ├── timing_service.hpp
│   │   ├── timing_service.cpp
│   │   ├── config_service.hpp
│   │   └── config_service.cpp
│   │
│   ├── game/                   # Game logic
│   │   ├── game_engine.hpp
│   │   ├── game_engine.cpp
│   │   ├── drill_manager.hpp
│   │   ├── drill_manager.cpp
│   │   ├── state_machine.hpp
│   │   ├── state_machine.cpp
│   │   ├── protocol.hpp
│   │   └── protocol.cpp
│   │
│   ├── platform/               # Platform-specific code
│   │   ├── pins.hpp            # Pin abstraction
│   │   ├── pins_devkit.hpp     # DevKit pin assignments
│   │   └── pins_pcb_v1.hpp     # PCB v1 pin assignments
│   │
│   └── utils/                  # Utilities
│       ├── error_codes.hpp
│       ├── ring_buffer.hpp
│       └── expected.hpp        # tl::expected backport
│
├── components/                 # Reusable ESP-IDF components
│   ├── etl/                    # Embedded Template Library
│   └── esp_now_wrapper/        # ESP-NOW C++ wrapper
│
├── test/
│   ├── CMakeLists.txt
│   ├── mocks/                  # Mock implementations
│   │   ├── mock_led_driver.hpp
│   │   ├── mock_audio_driver.hpp
│   │   └── ...
│   ├── test_led_driver.cpp
│   ├── test_audio_driver.cpp
│   └── ...
│
└── tools/
    ├── flash_factory.sh
    └── generate_samples.py
```

---

## Naming Conventions

### Files

| Type | Convention | Example |
|------|------------|---------|
| Source files | snake_case | `led_driver.cpp` |
| Header files | snake_case | `led_driver.hpp` |
| Interfaces | `i_` prefix | `i_led_driver.hpp` |
| Mocks | `mock_` prefix | `mock_led_driver.hpp` |
| Tests | `test_` prefix | `test_led_driver.cpp` |
| Platform configs | `_platform` suffix | `pins_devkit.hpp` |

### Code Elements

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `LedDriver` |
| Interfaces | `I` prefix + PascalCase | `ILedDriver` |
| Methods | camelCase | `playEffect()` |
| Variables | camelCase | `reactionTime` |
| Member variables | camelCase + `_` suffix | `ledDriver_` |
| Constants | `k` prefix + PascalCase | `kMaxBrightness` |
| Namespaces | lowercase | `domes`, `pins` |
| Macros | SCREAMING_SNAKE | `CONFIG_DOMES_LED_COUNT` |

---

## File Templates

### Interface Header Template

```cpp
// interfaces/i_example_driver.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for Example driver
 *
 * Implementations:
 * - ExampleDriver: Real hardware implementation
 * - MockExampleDriver: Test mock
 */
class IExampleDriver {
public:
    virtual ~IExampleDriver() = default;

    /**
     * @brief Initialize the driver
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Check if driver is ready
     * @return true if initialized
     */
    virtual bool isInitialized() const = 0;
};

}  // namespace domes
```

### Driver Header Template

```cpp
// drivers/example_driver.hpp
#pragma once

#include "interfaces/i_example_driver.hpp"
#include <cstdint>

namespace domes {

/**
 * @brief Hardware implementation of Example driver
 */
class ExampleDriver final : public IExampleDriver {
public:
    /**
     * @brief Construct driver with pin configuration
     * @param pin GPIO pin number
     */
    explicit ExampleDriver(gpio_num_t pin);

    // IExampleDriver interface
    esp_err_t init() override;
    bool isInitialized() const override;

private:
    static constexpr const char* kTag = "example";

    gpio_num_t pin_;
    bool initialized_ = false;
};

}  // namespace domes
```

### Driver Implementation Template

```cpp
// drivers/example_driver.cpp
#include "example_driver.hpp"
#include "esp_log.h"

namespace domes {

ExampleDriver::ExampleDriver(gpio_num_t pin)
    : pin_(pin) {
}

esp_err_t ExampleDriver::init() {
    if (initialized_) {
        ESP_LOGW(kTag, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Initialize hardware

    initialized_ = true;
    ESP_LOGI(kTag, "Initialized on GPIO %d", static_cast<int>(pin_));
    return ESP_OK;
}

bool ExampleDriver::isInitialized() const {
    return initialized_;
}

}  // namespace domes
```

### Mock Template

```cpp
// test/mocks/mock_example_driver.hpp
#pragma once

#include "interfaces/i_example_driver.hpp"

namespace domes {

/**
 * @brief Mock implementation for testing
 */
class MockExampleDriver : public IExampleDriver {
public:
    esp_err_t init() override {
        initCalled = true;
        return initReturnValue;
    }

    bool isInitialized() const override {
        return initialized;
    }

    // Test control
    bool initCalled = false;
    esp_err_t initReturnValue = ESP_OK;
    bool initialized = false;
};

}  // namespace domes
```

---

## Include Order

Follow this order in all source files:

```cpp
// 1. Corresponding header (for .cpp files)
#include "example_driver.hpp"

// 2. ESP-IDF headers
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 3. Standard library headers
#include <cstdint>
#include <cstring>

// 4. ETL headers
#include "etl/vector.h"
#include "etl/string.h"

// 5. Project headers (interfaces first)
#include "interfaces/i_other_driver.hpp"
#include "utils/error_codes.hpp"
```

---

## Component Boundaries

### When to Create a New Component (in `components/`)

Create a separate component when:
- Code is reusable across projects
- Has its own dependencies (Kconfig, etc.)
- Third-party library (ETL, etc.)

### What Stays in `main/`

Keep in main when:
- Project-specific implementation
- Single-use utilities
- Platform-specific code

---

## Namespace Usage

```cpp
// All project code in 'domes' namespace
namespace domes {

class LedDriver { ... };

}  // namespace domes

// Platform-specific in nested namespace
namespace domes::pins {

constexpr gpio_num_t kLedData = GPIO_NUM_48;

}  // namespace domes::pins

// Using in main.cpp
using namespace domes;

LedDriver led(pins::kLedData);
```

---

*Related: `00-getting-started.md`, `02-build-system.md`*
