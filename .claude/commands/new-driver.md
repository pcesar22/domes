---
description: Scaffold a new ESP-IDF driver with interface, implementation, and mock
argument-hint: <driver-name>
allowed-tools: Read, Write, Glob, Grep
---

# Create New Driver

Generate a complete driver scaffold following DOMES firmware architecture:
- Abstract interface in `interfaces/`
- Implementation in `drivers/`
- Mock for testing in `mocks/`

## Arguments

- `$1` (required) - Driver name in snake_case (e.g., `haptic`, `audio`, `touch_sensor`)

## Instructions

1. **Validate input:**
   - Ensure `$1` is provided
   - Convert to appropriate naming: `haptic` → `IHapticDriver`, `HapticDriver`, `MockHapticDriver`

2. **Read project guidelines:**
   - Read `firmware/GUIDELINES.md` for coding standards
   - Read existing interfaces in `firmware/domes/main/interfaces/` for patterns
   - Read existing drivers in `firmware/domes/main/drivers/` for implementation patterns

3. **Generate interface** (`firmware/domes/main/interfaces/i_<name>_driver.hpp`):

```cpp
#pragma once

#include <cstdint>
#include <expected>
#include <esp_err.h>

namespace domes {

/**
 * @brief Abstract interface for <Name> driver
 *
 * Implementations must be thread-safe and support static allocation.
 */
class I<Name>Driver {
public:
    virtual ~I<Name>Driver() = default;

    // Prevent copying
    I<Name>Driver(const I<Name>Driver&) = delete;
    I<Name>Driver& operator=(const I<Name>Driver&) = delete;

    /**
     * @brief Initialize the driver hardware
     * @return ESP_OK on success, error code on failure
     */
    virtual std::expected<void, esp_err_t> init() = 0;

    /**
     * @brief Deinitialize and release hardware resources
     */
    virtual void deinit() = 0;

    // TODO: Add driver-specific methods based on hardware requirements

protected:
    I<Name>Driver() = default;
};

} // namespace domes
```

4. **Generate implementation** (`firmware/domes/main/drivers/<name>_driver.hpp`):

```cpp
#pragma once

#include "interfaces/i_<name>_driver.hpp"
#include "config.hpp"

#include <esp_log.h>

namespace domes {

/**
 * @brief Hardware implementation of <Name> driver
 *
 * Uses ESP-IDF peripheral APIs. Thread-safe, statically allocated.
 */
template </* hardware config params if needed */>
class <Name>Driver final : public I<Name>Driver {
public:
    static constexpr const char* kTag = "<NAME>";

    <Name>Driver() = default;
    ~<Name>Driver() override { deinit(); }

    // Non-copyable, non-movable (hardware resource)
    <Name>Driver(const <Name>Driver&) = delete;
    <Name>Driver& operator=(const <Name>Driver&) = delete;
    <Name>Driver(<Name>Driver&&) = delete;
    <Name>Driver& operator=(<Name>Driver&&) = delete;

    std::expected<void, esp_err_t> init() override {
        if (initialized_) {
            return std::unexpected(ESP_ERR_INVALID_STATE);
        }

        // TODO: Initialize hardware
        // Example: Configure GPIO, I2C, SPI, etc.

        ESP_LOGI(kTag, "Initialized");
        initialized_ = true;
        return {};
    }

    void deinit() override {
        if (!initialized_) {
            return;
        }

        // TODO: Release hardware resources

        ESP_LOGI(kTag, "Deinitialized");
        initialized_ = false;
    }

    // TODO: Implement driver-specific methods

private:
    bool initialized_ = false;
    // TODO: Add hardware handles, buffers (statically allocated)
};

} // namespace domes
```

5. **Generate mock** (`firmware/domes/main/mocks/mock_<name>_driver.hpp`):

```cpp
#pragma once

#include "interfaces/i_<name>_driver.hpp"

namespace domes {

/**
 * @brief Mock implementation of <Name> driver for testing
 *
 * Records calls and allows setting expected behavior.
 */
class Mock<Name>Driver final : public I<Name>Driver {
public:
    Mock<Name>Driver() = default;
    ~Mock<Name>Driver() override = default;

    // Test instrumentation
    struct CallCounts {
        uint32_t init = 0;
        uint32_t deinit = 0;
        // TODO: Add counters for driver-specific methods
    };

    CallCounts& calls() { return calls_; }
    void reset() { calls_ = CallCounts{}; }

    // Set what init() should return
    void setInitResult(esp_err_t result) { initResult_ = result; }

    // I<Name>Driver implementation
    std::expected<void, esp_err_t> init() override {
        calls_.init++;
        if (initResult_ != ESP_OK) {
            return std::unexpected(initResult_);
        }
        return {};
    }

    void deinit() override {
        calls_.deinit++;
    }

    // TODO: Implement driver-specific methods with call tracking

private:
    CallCounts calls_{};
    esp_err_t initResult_ = ESP_OK;
};

} // namespace domes
```

6. **Create mocks directory if needed:**
   - Ensure `firmware/domes/main/mocks/` exists

7. **Update CMakeLists.txt:**
   - Add new source files to `firmware/domes/main/CMakeLists.txt` if using .cpp files

8. **Report what was created:**
   - List all generated files
   - Note any TODO items the developer should address
   - Suggest next steps (implement hardware-specific methods, add to component)
