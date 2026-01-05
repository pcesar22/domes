# 12 - System Policies and Resource Management

## AI Agent Instructions

Load this file when:
- Implementing shared resource management (I2C, SPI)
- Configuring memory allocation (SRAM vs PSRAM)
- Setting up pod identity and network joining
- Implementing error handling and recovery
- Writing driver mocks for testing

Prerequisites: `03-driver-development.md`, `09-reference.md`

---

## I2C Bus Manager

### Problem Statement

The I2C bus (I2C0) is shared between:
- **LIS2DW12 IMU** (address 0x18/0x19) - polled for tap detection
- **DRV2605L Haptic** (address 0x5A) - triggered during feedback

Without coordination, concurrent access can corrupt transactions.

### I2C Manager Implementation

```cpp
// platform/i2c_manager.hpp
#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstdint>

namespace domes {

/**
 * @brief Thread-safe I2C bus manager
 *
 * Provides mutex-protected access to the shared I2C bus.
 * All I2C device drivers should use this instead of direct i2c_master calls.
 */
class I2cManager {
public:
    /**
     * @brief Get singleton instance
     */
    static I2cManager& instance();

    /**
     * @brief Initialize I2C bus
     * @param sdaPin SDA GPIO
     * @param sclPin SCL GPIO
     * @param freqHz Bus frequency (default 400kHz)
     * @return ESP_OK on success
     */
    esp_err_t init(gpio_num_t sdaPin, gpio_num_t sclPin, uint32_t freqHz = 400000);

    /**
     * @brief Deinitialize I2C bus
     */
    esp_err_t deinit();

    /**
     * @brief Write to I2C device (thread-safe)
     * @param addr Device address (7-bit)
     * @param data Data to write
     * @param len Data length
     * @param timeoutMs Timeout in ms (default 100)
     * @return ESP_OK on success
     */
    esp_err_t write(uint8_t addr, const uint8_t* data, size_t len,
                    uint32_t timeoutMs = kDefaultTimeoutMs);

    /**
     * @brief Read from I2C device (thread-safe)
     * @param addr Device address (7-bit)
     * @param data Buffer for received data
     * @param len Number of bytes to read
     * @param timeoutMs Timeout in ms
     * @return ESP_OK on success
     */
    esp_err_t read(uint8_t addr, uint8_t* data, size_t len,
                   uint32_t timeoutMs = kDefaultTimeoutMs);

    /**
     * @brief Write then read (thread-safe)
     * @param addr Device address
     * @param writeData Data to write (e.g., register address)
     * @param writeLen Write data length
     * @param readData Buffer for received data
     * @param readLen Number of bytes to read
     * @param timeoutMs Timeout in ms
     * @return ESP_OK on success
     */
    esp_err_t writeRead(uint8_t addr,
                        const uint8_t* writeData, size_t writeLen,
                        uint8_t* readData, size_t readLen,
                        uint32_t timeoutMs = kDefaultTimeoutMs);

    /**
     * @brief Write register value
     * @param addr Device address
     * @param reg Register address
     * @param value Register value
     * @return ESP_OK on success
     */
    esp_err_t writeReg(uint8_t addr, uint8_t reg, uint8_t value);

    /**
     * @brief Read register value
     * @param addr Device address
     * @param reg Register address
     * @param value Output for register value
     * @return ESP_OK on success
     */
    esp_err_t readReg(uint8_t addr, uint8_t reg, uint8_t* value);

    /**
     * @brief Check if device responds
     * @param addr Device address
     * @return true if device ACKs
     */
    bool probe(uint8_t addr);

    /**
     * @brief Recover bus from stuck state
     * @return ESP_OK if recovered
     */
    esp_err_t recover();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    static constexpr uint32_t kDefaultTimeoutMs = 100;
    static constexpr uint32_t kDefaultFreqHz = 400000;  // 400kHz

private:
    I2cManager() = default;
    ~I2cManager();

    // Non-copyable
    I2cManager(const I2cManager&) = delete;
    I2cManager& operator=(const I2cManager&) = delete;

    i2c_port_t port_ = I2C_NUM_0;
    SemaphoreHandle_t mutex_ = nullptr;
    StaticSemaphore_t mutexBuffer_;
    bool initialized_ = false;
    gpio_num_t sdaPin_ = GPIO_NUM_NC;
    gpio_num_t sclPin_ = GPIO_NUM_NC;

    static constexpr const char* kTag = "i2c";
};

/**
 * @brief RAII lock guard for I2C bus
 */
class I2cLock {
public:
    explicit I2cLock(I2cManager& mgr, uint32_t timeoutMs = portMAX_DELAY);
    ~I2cLock();

    bool acquired() const { return acquired_; }

private:
    I2cManager& mgr_;
    bool acquired_;
};

}  // namespace domes
```

```cpp
// platform/i2c_manager.cpp
#include "i2c_manager.hpp"
#include "esp_log.h"

namespace domes {

I2cManager& I2cManager::instance() {
    static I2cManager instance;
    return instance;
}

I2cManager::~I2cManager() {
    deinit();
}

esp_err_t I2cManager::init(gpio_num_t sdaPin, gpio_num_t sclPin, uint32_t freqHz) {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    sdaPin_ = sdaPin;
    sclPin_ = sclPin;

    // Create mutex (static allocation)
    mutex_ = xSemaphoreCreateMutexStatic(&mutexBuffer_);
    if (!mutex_) {
        return ESP_ERR_NO_MEM;
    }

    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sdaPin,
        .scl_io_num = sclPin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = freqHz,
        },
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(port_, &conf), kTag, "Config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(port_, I2C_MODE_MASTER, 0, 0, 0),
                        kTag, "Install failed");

    initialized_ = true;
    ESP_LOGI(kTag, "I2C%d initialized: SDA=%d, SCL=%d, %lukHz",
             port_, sdaPin, sclPin, freqHz / 1000);
    return ESP_OK;
}

esp_err_t I2cManager::deinit() {
    if (!initialized_) return ESP_OK;

    i2c_driver_delete(port_);
    initialized_ = false;
    return ESP_OK;
}

esp_err_t I2cManager::write(uint8_t addr, const uint8_t* data, size_t len,
                            uint32_t timeoutMs) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_write_to_device(
        port_, addr, data, len, pdMS_TO_TICKS(timeoutMs));

    xSemaphoreGive(mutex_);
    return err;
}

esp_err_t I2cManager::read(uint8_t addr, uint8_t* data, size_t len,
                           uint32_t timeoutMs) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_read_from_device(
        port_, addr, data, len, pdMS_TO_TICKS(timeoutMs));

    xSemaphoreGive(mutex_);
    return err;
}

esp_err_t I2cManager::writeRead(uint8_t addr,
                                const uint8_t* writeData, size_t writeLen,
                                uint8_t* readData, size_t readLen,
                                uint32_t timeoutMs) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_write_read_device(
        port_, addr, writeData, writeLen, readData, readLen,
        pdMS_TO_TICKS(timeoutMs));

    xSemaphoreGive(mutex_);
    return err;
}

esp_err_t I2cManager::writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return write(addr, data, 2);
}

esp_err_t I2cManager::readReg(uint8_t addr, uint8_t reg, uint8_t* value) {
    return writeRead(addr, &reg, 1, value, 1);
}

bool I2cManager::probe(uint8_t addr) {
    if (!initialized_) return false;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(port_, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    xSemaphoreGive(mutex_);
    return err == ESP_OK;
}

esp_err_t I2cManager::recover() {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    ESP_LOGW(kTag, "Attempting I2C bus recovery");

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Generate clock pulses to unstick SDA
    i2c_driver_delete(port_);

    gpio_set_direction(sclPin_, GPIO_MODE_OUTPUT);
    gpio_set_direction(sdaPin_, GPIO_MODE_INPUT);

    for (int i = 0; i < 9; i++) {
        gpio_set_level(sclPin_, 0);
        esp_rom_delay_us(5);
        gpio_set_level(sclPin_, 1);
        esp_rom_delay_us(5);
    }

    // Reinitialize
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sdaPin_,
        .scl_io_num = sclPin_,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = kDefaultFreqHz},
    };

    esp_err_t err = i2c_param_config(port_, &conf);
    if (err == ESP_OK) {
        err = i2c_driver_install(port_, I2C_MODE_MASTER, 0, 0, 0);
    }

    xSemaphoreGive(mutex_);

    if (err == ESP_OK) {
        ESP_LOGI(kTag, "I2C bus recovered");
    }
    return err;
}

// RAII Lock
I2cLock::I2cLock(I2cManager& mgr, uint32_t timeoutMs)
    : mgr_(mgr)
    , acquired_(xSemaphoreTake(mgr.mutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
}

I2cLock::~I2cLock() {
    if (acquired_) {
        xSemaphoreGive(mgr_.mutex_);
    }
}

}  // namespace domes
```

### Usage in Drivers

```cpp
// Example: IMU driver using I2C manager
esp_err_t ImuDriver::readAcceleration(Acceleration& accel) {
    auto& i2c = I2cManager::instance();

    uint8_t reg = 0x28 | 0x80;  // OUT_X_L with auto-increment
    uint8_t data[6];

    ESP_RETURN_ON_ERROR(
        i2c.writeRead(kI2cAddress, &reg, 1, data, 6),
        kTag, "Failed to read acceleration"
    );

    accel.x = static_cast<int16_t>(data[0] | (data[1] << 8));
    accel.y = static_cast<int16_t>(data[2] | (data[3] << 8));
    accel.z = static_cast<int16_t>(data[4] | (data[5] << 8));

    return ESP_OK;
}
```

---

## PSRAM Allocation Policy

### Memory Regions

| Region | Size | Use For |
|--------|------|---------|
| SRAM | 512KB | FreeRTOS, task stacks, frequently accessed data |
| PSRAM | 8MB | Large buffers, audio samples, OTA staging |

### Allocation Guidelines

```cpp
// config/memory_policy.hpp
#pragma once

#include "esp_heap_caps.h"
#include <cstddef>

namespace domes::memory {

/**
 * @brief Allocate from internal SRAM (fast, limited)
 * Use for: Task stacks, FreeRTOS objects, ISR-accessed data
 */
inline void* allocSram(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

/**
 * @brief Allocate from PSRAM (slow, abundant)
 * Use for: Audio buffers, OTA staging, large static data
 */
inline void* allocPsram(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/**
 * @brief Allocate from DMA-capable memory (for I2S, SPI)
 */
inline void* allocDma(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
}

/**
 * @brief Free memory allocated with heap_caps
 */
inline void free(void* ptr) {
    heap_caps_free(ptr);
}

/**
 * @brief Print memory stats to log
 */
void logMemoryStats();

}  // namespace domes::memory
```

### What Goes Where

| Component | Memory Region | Rationale |
|-----------|---------------|-----------|
| Task stacks | SRAM | Must be in internal RAM for FreeRTOS |
| FreeRTOS queues | SRAM | Accessed from ISR |
| LED color buffer (64B) | SRAM | Small, frequently accessed |
| Audio DMA buffer (4KB) | DMA-capable SRAM | I2S DMA requirement |
| Audio file cache (64KB) | PSRAM | Large, sequential access |
| OTA staging (512KB) | PSRAM | Large, temporary |
| NVS cache | SRAM | Fast config access |
| Protocol buffers | SRAM | Small, frequent use |

### Kconfig Settings

```
# Memory configuration
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096

# Force small allocations to internal RAM
# Allocations <= 4KB go to SRAM, larger to PSRAM if available
```

### Memory Monitoring

```cpp
void memory::logMemoryStats() {
    ESP_LOGI("memory", "=== Heap Status ===");

    // Internal SRAM
    size_t sramFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t sramMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI("memory", "SRAM: %d free (%d min)", sramFree, sramMin);

    // PSRAM
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramMin = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI("memory", "PSRAM: %d free (%d min)", psramFree, psramMin);

    // Total
    size_t totalFree = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI("memory", "Total: %d free", totalFree);
}
```

---

## Pod Identity and Network Joining

### Pod Identity

Each pod has a unique identity stored in NVS:

```cpp
// config/pod_identity.hpp
#pragma once

#include <cstdint>
#include "esp_err.h"

namespace domes {

struct PodIdentity {
    uint8_t mac[6];         // MAC address (from ESP32)
    uint8_t podId;          // Assigned pod ID (1-24)
    char name[16];          // User-assigned name (e.g., "Pod-1")
    uint32_t serialNumber;  // Factory serial
    uint8_t hardwareRev;    // PCB revision
    uint8_t firmwareSlot;   // Current OTA slot (0/1)
};

class PodIdentityManager {
public:
    /**
     * @brief Load identity from NVS
     */
    esp_err_t load();

    /**
     * @brief Save identity to NVS
     */
    esp_err_t save();

    /**
     * @brief Get current identity
     */
    const PodIdentity& identity() const { return identity_; }

    /**
     * @brief Assign pod ID (1-24)
     * @param id Pod ID (0 = auto-assign based on MAC)
     */
    esp_err_t setPodId(uint8_t id);

    /**
     * @brief Set pod name
     */
    esp_err_t setName(const char* name);

    /**
     * @brief Generate unique pod ID from MAC
     * Uses last 2 bytes of MAC mod 24 + 1
     */
    uint8_t generateIdFromMac() const;

    /**
     * @brief Check if identity is configured
     */
    bool isConfigured() const;

private:
    PodIdentity identity_{};
    bool loaded_ = false;

    static constexpr const char* kNvsNamespace = "identity";
};

}  // namespace domes
```

### Network Joining Protocol

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         NETWORK JOINING PROTOCOL                            │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  New Pod                              Existing Network                      │
│     │                                        │                              │
│     │  1. Broadcast DISCOVER                 │                              │
│     │────────────────────────────────────────►                              │
│     │                                        │                              │
│     │  2. DISCOVER_ACK (master info)         │                              │
│     │◄────────────────────────────────────────                              │
│     │                                        │                              │
│     │  3. JOIN_REQUEST (my identity)         │                              │
│     │────────────────────────────────────────► Master                       │
│     │                                        │                              │
│     │  4. JOIN_ACCEPT (assigned ID, config)  │                              │
│     │◄──────────────────────────────────────── Master                       │
│     │                                        │                              │
│     │  5. Start clock sync                   │                              │
│     │◄───────────────────────────────────────►                              │
│     │                                        │                              │
│     │  6. Transition to Connected            │                              │
│     │                                        │                              │
└────────────────────────────────────────────────────────────────────────────┘
```

### Join Protocol Messages

```cpp
#pragma pack(push, 1)

struct JoinRequestMsg {
    MessageHeader header;
    uint8_t mac[6];
    uint8_t preferredPodId;    // 0 = no preference
    uint8_t hardwareRev;
    uint16_t firmwareVersion;
    char name[16];
};

struct JoinAcceptMsg {
    MessageHeader header;
    uint8_t assignedPodId;     // Assigned by master
    uint8_t networkKey[16];    // For encrypted comms (future)
    uint32_t masterTimestamp;  // For initial sync
};

struct JoinRejectMsg {
    MessageHeader header;
    uint8_t reason;  // 0=network full, 1=incompatible, 2=duplicate
};

#pragma pack(pop)
```

### Pod ID Assignment Rules

1. **User-assigned**: If pod has ID in NVS, use it
2. **Master-assigned**: Master assigns first available ID (1-24)
3. **Conflict resolution**: If two pods claim same ID, lower MAC wins
4. **Duplicate detection**: Master rejects join if MAC already in network

---

## Error Handling and Recovery

### Error Categories

```cpp
// utils/error_codes.hpp
#pragma once

namespace domes {

/**
 * @brief Error categories for recovery decisions
 */
enum class ErrorCategory : uint8_t {
    kHardware,      // Sensor/driver failure
    kCommunication, // Network/protocol failure
    kResource,      // Memory/queue exhausted
    kProtocol,      // Invalid message/state
    kUser,          // Invalid user input
};

/**
 * @brief Detailed error codes
 */
enum class ErrorCode : uint16_t {
    // Hardware (0x01xx)
    kI2cTimeout = 0x0101,
    kI2cNack = 0x0102,
    kI2cBusStuck = 0x0103,
    kSpiTimeout = 0x0104,
    kAdcReadFail = 0x0105,
    kTouchCalibFail = 0x0106,
    kImuSelfTestFail = 0x0107,

    // Communication (0x02xx)
    kEspNowSendFail = 0x0201,
    kEspNowNoAck = 0x0202,
    kBleDisconnect = 0x0203,
    kBleWriteFail = 0x0204,
    kNetworkTimeout = 0x0205,
    kMasterLost = 0x0206,
    kSyncFail = 0x0207,

    // Resource (0x03xx)
    kOutOfMemory = 0x0301,
    kQueueFull = 0x0302,
    kTaskCreateFail = 0x0303,
    kNvsWriteFail = 0x0304,

    // Protocol (0x04xx)
    kInvalidMessage = 0x0401,
    kVersionMismatch = 0x0402,
    kUnexpectedState = 0x0403,
    kChecksumFail = 0x0404,

    // User (0x05xx)
    kInvalidConfig = 0x0501,
    kOtaInvalidImage = 0x0502,
};

ErrorCategory getCategory(ErrorCode code);
const char* errorToString(ErrorCode code);

}  // namespace domes
```

### Recovery Strategies

```cpp
// utils/error_recovery.hpp
#pragma once

#include "error_codes.hpp"
#include "esp_err.h"

namespace domes {

/**
 * @brief Recovery action to take
 */
enum class RecoveryAction : uint8_t {
    kRetry,         // Retry the operation
    kReset,         // Reset the subsystem
    kReboot,        // Reboot the device
    kIgnore,        // Log and continue
    kNotify,        // Notify user/master
    kFallback,      // Use fallback behavior
};

/**
 * @brief Error recovery manager
 */
class ErrorRecovery {
public:
    /**
     * @brief Handle an error and get recovery action
     * @param code Error code
     * @param retryCount Current retry count
     * @return Recommended recovery action
     */
    static RecoveryAction getAction(ErrorCode code, uint8_t retryCount);

    /**
     * @brief Execute recovery action
     * @param action Action to execute
     * @param context Subsystem-specific context
     */
    static esp_err_t execute(RecoveryAction action, void* context);

    /**
     * @brief Log error with context
     */
    static void logError(ErrorCode code, const char* context);

    static constexpr uint8_t kMaxRetries = 3;
};

}  // namespace domes
```

```cpp
// utils/error_recovery.cpp
#include "error_recovery.hpp"
#include "esp_log.h"
#include "esp_system.h"

namespace domes {

namespace {
    constexpr const char* kTag = "error";
}

RecoveryAction ErrorRecovery::getAction(ErrorCode code, uint8_t retryCount) {
    ErrorCategory cat = getCategory(code);

    // Exhausted retries
    if (retryCount >= kMaxRetries) {
        switch (cat) {
            case ErrorCategory::kHardware:
                return RecoveryAction::kReset;
            case ErrorCategory::kCommunication:
                return RecoveryAction::kFallback;
            case ErrorCategory::kResource:
                return RecoveryAction::kReboot;
            default:
                return RecoveryAction::kNotify;
        }
    }

    // Specific error handling
    switch (code) {
        case ErrorCode::kI2cBusStuck:
            return RecoveryAction::kReset;  // I2C bus recovery

        case ErrorCode::kMasterLost:
            return RecoveryAction::kFallback;  // Standalone mode

        case ErrorCode::kOutOfMemory:
            return RecoveryAction::kReboot;  // Can't recover

        case ErrorCode::kInvalidMessage:
            return RecoveryAction::kIgnore;  // Drop bad message

        default:
            return RecoveryAction::kRetry;
    }
}

esp_err_t ErrorRecovery::execute(RecoveryAction action, void* context) {
    switch (action) {
        case RecoveryAction::kRetry:
            // Caller handles retry
            return ESP_OK;

        case RecoveryAction::kReset:
            // Reset subsystem (context = reset function pointer)
            if (context) {
                auto resetFn = static_cast<esp_err_t(*)()>(context);
                return resetFn();
            }
            return ESP_ERR_INVALID_ARG;

        case RecoveryAction::kReboot:
            ESP_LOGW(kTag, "Recovery action: REBOOT");
            vTaskDelay(pdMS_TO_TICKS(100));  // Allow log flush
            esp_restart();
            return ESP_OK;  // Never reached

        case RecoveryAction::kIgnore:
            return ESP_OK;

        case RecoveryAction::kNotify:
            // Caller handles notification
            return ESP_OK;

        case RecoveryAction::kFallback:
            // Caller handles fallback
            return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

void ErrorRecovery::logError(ErrorCode code, const char* context) {
    ESP_LOGE(kTag, "Error 0x%04X (%s) in %s",
             static_cast<uint16_t>(code),
             errorToString(code),
             context ? context : "unknown");
}

}  // namespace domes
```

---

## Mock Driver Specifications

### Mock LED Driver

```cpp
// test/mocks/mock_led_driver.hpp
#pragma once

#include "interfaces/i_led_driver.hpp"
#include <array>

namespace domes::test {

class MockLedDriver : public ILedDriver {
public:
    // === Test Control ===
    esp_err_t initResult = ESP_OK;
    esp_err_t refreshResult = ESP_OK;
    bool failNextRefresh = false;

    int initCallCount = 0;
    int refreshCallCount = 0;
    int deinitCallCount = 0;

    // Captured state
    std::array<Color, kLedCount> colors{};
    uint8_t brightness = 255;
    bool initialized = false;

    // === Interface Implementation ===

    esp_err_t init() override {
        initCallCount++;
        if (initResult == ESP_OK) {
            initialized = true;
            std::fill(colors.begin(), colors.end(), Color::off());
        }
        return initResult;
    }

    esp_err_t deinit() override {
        deinitCallCount++;
        initialized = false;
        return ESP_OK;
    }

    esp_err_t setAll(Color color) override {
        std::fill(colors.begin(), colors.end(), color);
        return ESP_OK;
    }

    esp_err_t setLed(uint8_t index, Color color) override {
        if (index >= kLedCount) return ESP_ERR_INVALID_ARG;
        colors[index] = color;
        return ESP_OK;
    }

    esp_err_t setRange(uint8_t start, uint8_t count, Color color) override {
        if (start + count > kLedCount) return ESP_ERR_INVALID_ARG;
        for (uint8_t i = 0; i < count; i++) {
            colors[start + i] = color;
        }
        return ESP_OK;
    }

    esp_err_t setBuffer(etl::span<const Color> colorSpan) override {
        if (colorSpan.size() != kLedCount) return ESP_ERR_INVALID_SIZE;
        std::copy(colorSpan.begin(), colorSpan.end(), colors.begin());
        return ESP_OK;
    }

    Color getLed(uint8_t index) const override {
        if (index >= kLedCount) return Color::off();
        return colors[index];
    }

    esp_err_t setBrightness(uint8_t b) override {
        brightness = b;
        return ESP_OK;
    }

    uint8_t getBrightness() const override {
        return brightness;
    }

    esp_err_t refresh() override {
        refreshCallCount++;
        if (failNextRefresh) {
            failNextRefresh = false;
            return ESP_FAIL;
        }
        return refreshResult;
    }

    bool isInitialized() const override {
        return initialized;
    }

    // === Test Helpers ===

    void reset() {
        initCallCount = 0;
        refreshCallCount = 0;
        deinitCallCount = 0;
        initResult = ESP_OK;
        refreshResult = ESP_OK;
        failNextRefresh = false;
        initialized = false;
        brightness = 255;
        std::fill(colors.begin(), colors.end(), Color::off());
    }

    bool allLedsAre(Color color) const {
        return std::all_of(colors.begin(), colors.end(),
                          [&](const Color& c) { return c == color; });
    }
};

}  // namespace domes::test
```

### Mock Touch Driver

```cpp
// test/mocks/mock_touch_driver.hpp
#pragma once

#include "interfaces/i_touch_driver.hpp"
#include <queue>

namespace domes::test {

class MockTouchDriver : public ITouchDriver {
public:
    // === Test Control ===
    esp_err_t initResult = ESP_OK;
    bool enabled = false;
    bool touched = false;
    uint16_t threshold = 500;
    uint16_t rawValue = 0;
    TouchFusionMode fusionMode = TouchFusionMode::kEitherOr;

    std::queue<TouchEvent> pendingEvents;
    TouchCallback callback;

    int initCallCount = 0;
    int enableCallCount = 0;
    int calibrateCallCount = 0;

    // === Interface Implementation ===

    esp_err_t init() override {
        initCallCount++;
        return initResult;
    }

    esp_err_t deinit() override {
        enabled = false;
        return ESP_OK;
    }

    esp_err_t enable() override {
        enableCallCount++;
        enabled = true;
        return ESP_OK;
    }

    esp_err_t disable() override {
        enabled = false;
        return ESP_OK;
    }

    bool isEnabled() const override {
        return enabled;
    }

    bool isTouched() const override {
        return touched;
    }

    esp_err_t getEvent(TouchEvent* event) override {
        if (pendingEvents.empty()) {
            return ESP_ERR_NOT_FOUND;
        }
        *event = pendingEvents.front();
        pendingEvents.pop();
        return ESP_OK;
    }

    esp_err_t waitForTouch(TouchEvent* event, uint32_t timeoutMs) override {
        // In mock, just check queue immediately
        return getEvent(event);
    }

    void onTouch(TouchCallback cb) override {
        callback = cb;
    }

    esp_err_t setThreshold(uint16_t t) override {
        threshold = t;
        return ESP_OK;
    }

    uint16_t getThreshold() const override {
        return threshold;
    }

    esp_err_t setFusionMode(TouchFusionMode mode) override {
        fusionMode = mode;
        return ESP_OK;
    }

    TouchFusionMode getFusionMode() const override {
        return fusionMode;
    }

    esp_err_t calibrate() override {
        calibrateCallCount++;
        return ESP_OK;
    }

    uint16_t getRawValue() const override {
        return rawValue;
    }

    bool isInitialized() const override {
        return initCallCount > 0;
    }

    // === Test Helpers ===

    void simulateTouch(uint8_t strength = 128, TouchSource source = TouchSource::kFused) {
        TouchEvent event{
            .timestampUs = esp_timer_get_time(),
            .strength = strength,
            .isTap = true,
            .source = source,
        };
        pendingEvents.push(event);

        if (callback) {
            callback(event);
        }
    }

    void reset() {
        initCallCount = 0;
        enableCallCount = 0;
        calibrateCallCount = 0;
        initResult = ESP_OK;
        enabled = false;
        touched = false;
        threshold = 500;
        rawValue = 0;
        fusionMode = TouchFusionMode::kEitherOr;
        while (!pendingEvents.empty()) pendingEvents.pop();
        callback = nullptr;
    }
};

}  // namespace domes::test
```

### Mock Power Driver

```cpp
// test/mocks/mock_power_driver.hpp
#pragma once

#include "interfaces/i_power_driver.hpp"

namespace domes::test {

class MockPowerDriver : public IPowerDriver {
public:
    // === Test Control ===
    uint16_t batteryMv = 3700;
    uint8_t batteryPercent = 75;
    ChargingState chargingState = ChargingState::kNotConnected;
    uint8_t lowThresholdPercent = 20;
    uint8_t criticalThresholdPercent = 5;

    PowerEventCallback eventCallback;

    int initCallCount = 0;
    int sleepCallCount = 0;

    // === Interface Implementation ===

    esp_err_t init() override {
        initCallCount++;
        return ESP_OK;
    }

    esp_err_t deinit() override { return ESP_OK; }

    uint16_t getBatteryVoltageMv() override { return batteryMv; }
    uint8_t getBatteryPercent() override { return batteryPercent; }
    ChargingState getChargingState() override { return chargingState; }
    bool isCharging() override { return chargingState == ChargingState::kCharging; }

    bool isLowBattery() const override {
        return batteryPercent <= lowThresholdPercent;
    }

    bool isCriticalBattery() const override {
        return batteryPercent <= criticalThresholdPercent;
    }

    esp_err_t setLowBatteryThreshold(uint16_t t) override {
        lowThresholdPercent = (t <= 100) ? t : 20;
        return ESP_OK;
    }

    uint8_t getLowBatteryThresholdPercent() const override {
        return lowThresholdPercent;
    }

    esp_err_t setCriticalBatteryThreshold(uint16_t t) override {
        criticalThresholdPercent = (t <= 100) ? t : 5;
        return ESP_OK;
    }

    void onPowerEvent(PowerEventCallback cb) override {
        eventCallback = cb;
    }

    esp_err_t enterLightSleep(uint32_t durationMs) override {
        sleepCallCount++;
        return ESP_OK;
    }

    void enterDeepSleep() override {
        // Don't actually sleep in test
    }

    esp_err_t setDeepSleepWakePin(gpio_num_t pin, bool level) override {
        return ESP_OK;
    }

    esp_err_t setDeepSleepTimer(uint32_t seconds) override {
        return ESP_OK;
    }

    bool isInitialized() const override {
        return initCallCount > 0;
    }

    // === Test Helpers ===

    void simulateLowBattery() {
        batteryPercent = 15;
        batteryMv = 3350;
        if (eventCallback) {
            eventCallback(PowerEvent::kLowBattery);
        }
    }

    void simulateCharging() {
        chargingState = ChargingState::kCharging;
        if (eventCallback) {
            eventCallback(PowerEvent::kChargingStarted);
        }
    }

    void reset() {
        initCallCount = 0;
        sleepCallCount = 0;
        batteryMv = 3700;
        batteryPercent = 75;
        chargingState = ChargingState::kNotConnected;
        lowThresholdPercent = 20;
        criticalThresholdPercent = 5;
        eventCallback = nullptr;
    }
};

}  // namespace domes::test
```

---

## Usage Example: Full Integration

```cpp
// main/main.cpp excerpt showing policy usage

#include "platform/i2c_manager.hpp"
#include "config/memory_policy.hpp"
#include "config/pod_identity.hpp"
#include "utils/error_recovery.hpp"

esp_err_t initSystem() {
    // Initialize I2C bus (shared resource)
    auto& i2c = I2cManager::instance();
    ESP_RETURN_ON_ERROR(
        i2c.init(pins::kI2cSda, pins::kI2cScl, 400000),
        "main", "I2C init failed"
    );

    // Load pod identity
    PodIdentityManager identity;
    ESP_RETURN_ON_ERROR(identity.load(), "main", "Identity load failed");
    if (!identity.isConfigured()) {
        // Assign default ID from MAC
        identity.setPodId(identity.generateIdFromMac());
        identity.save();
    }

    // Initialize drivers (use I2C manager internally)
    // ...

    // Log memory stats at startup
    memory::logMemoryStats();

    return ESP_OK;
}

void handleDriverError(ErrorCode code) {
    ErrorRecovery::logError(code, "driver");

    RecoveryAction action = ErrorRecovery::getAction(code, retryCount_);

    switch (action) {
        case RecoveryAction::kRetry:
            retryCount_++;
            break;

        case RecoveryAction::kReset:
            // Reset I2C bus
            I2cManager::instance().recover();
            retryCount_ = 0;
            break;

        case RecoveryAction::kFallback:
            // Enter standalone mode
            stateMachine_.transitionTo(PodState::kStandalone);
            break;

        default:
            break;
    }
}
```

---

*Prerequisites: 03-driver-development.md, 09-reference.md*
*Related: 10-master-election.md, 11-clock-sync.md*
