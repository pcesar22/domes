# 08 - OTA Updates

## AI Agent Instructions

Load this file when:
- Implementing OTA update functionality
- Configuring partition table for OTA
- Implementing rollback protection
- Testing firmware updates

Prerequisites: `02-build-system.md`, `04-communication.md`

---

## OTA Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    FLASH MEMORY (16MB)                       │
├─────────────────────────────────────────────────────────────┤
│  0x000000  ┌─────────────────────┐                          │
│            │    Bootloader       │  32KB                    │
│  0x008000  ├─────────────────────┤                          │
│            │  Partition Table    │  4KB                     │
│  0x009000  ├─────────────────────┤                          │
│            │    NVS              │  24KB                    │
│  0x00F000  ├─────────────────────┤                          │
│            │    OTA Data         │  8KB (boot selection)    │
│  0x020000  ├─────────────────────┤                          │
│            │                     │                          │
│            │    OTA_0            │  4MB (Firmware A)        │
│            │                     │                          │
│  0x420000  ├─────────────────────┤                          │
│            │                     │                          │
│            │    OTA_1            │  4MB (Firmware B)        │
│            │                     │                          │
│  0x820000  ├─────────────────────┤                          │
│            │                     │                          │
│            │    SPIFFS           │  6MB (Audio, config)     │
│            │                     │                          │
│  0xE20000  ├─────────────────────┤                          │
│            │    Core Dump        │  64KB                    │
│  0xE30000  └─────────────────────┘                          │
└─────────────────────────────────────────────────────────────┘
```

---

## Partition Table

```csv
# firmware/partitions.csv
# Name,    Type, SubType,  Offset,   Size,    Flags
nvs,       data, nvs,      0x9000,   0x6000,
otadata,   data, ota,      0xf000,   0x2000,
ota_0,     app,  ota_0,    0x20000,  0x400000,
ota_1,     app,  ota_1,    0x420000, 0x400000,
spiffs,    data, spiffs,   0x820000, 0x600000,
coredump,  data, coredump, 0xE20000, 0x10000,
```

**Key Points:**
- OTA partitions must be same size (4MB each)
- Binary must be < 4MB to fit
- `otadata` tracks which partition to boot

---

## OTA Update Flow

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│  Phone App  │         │  Master Pod │         │  Target Pod │
└──────┬──────┘         └──────┬──────┘         └──────┬──────┘
       │                       │                       │
       │ 1. OTA_BEGIN          │                       │
       │ (size, version, hash) │                       │
       ├──────────────────────►│                       │
       │                       │ 2. Relay OTA_BEGIN    │
       │                       ├──────────────────────►│
       │                       │                       │
       │                       │◄──────────────────────┤
       │◄──────────────────────┤       ACK             │
       │        ACK            │                       │
       │                       │                       │
       │ 3. OTA_DATA           │                       │
       │ (offset, chunk)       │                       │
       ├──────────────────────►│ 4. Relay chunks       │
       │                       ├──────────────────────►│
       │                       │                       │ 5. Write
       │                       │                       │    to flash
       │                       │◄──────────────────────┤
       │◄──────────────────────┤       ACK             │
       │        ACK            │                       │
       │                       │                       │
       │  ... repeat for all chunks ...               │
       │                       │                       │
       │ 6. OTA_END            │                       │
       │ (final hash)          │                       │
       ├──────────────────────►│ 7. Relay OTA_END      │
       │                       ├──────────────────────►│
       │                       │                       │ 8. Verify
       │                       │                       │    SHA-256
       │                       │                       │
       │                       │                       │ 9. Set boot
       │                       │                       │    partition
       │                       │◄──────────────────────┤
       │◄──────────────────────┤     SUCCESS           │
       │       SUCCESS         │                       │
       │                       │                       │
       │                       │                       │ 10. Reboot
       │                       │                       ├───┐
       │                       │                       │   │
       │                       │                       │◄──┘
       │                       │                       │
       │                       │                       │ 11. Self-test
       │                       │                       │     & confirm
```

---

## OTA Manager Implementation

```cpp
// services/ota_manager.hpp
#pragma once

#include "esp_ota_ops.h"
#include "esp_err.h"
#include <functional>

namespace domes {

enum class OtaState {
    kIdle,
    kReceiving,
    kVerifying,
    kRebooting,
    kError,
};

using OtaProgressCallback = std::function<void(size_t received, size_t total)>;

class OtaManager {
public:
    static OtaManager& instance();

    /**
     * @brief Begin OTA update
     * @param imageSize Total firmware size in bytes
     * @param expectedHash SHA-256 hash (32 bytes)
     * @return ESP_OK on success
     */
    esp_err_t begin(size_t imageSize, const uint8_t* expectedHash);

    /**
     * @brief Write chunk of firmware data
     * @param data Firmware data
     * @param len Chunk length
     * @return ESP_OK on success
     */
    esp_err_t writeChunk(const uint8_t* data, size_t len);

    /**
     * @brief Finalize OTA update and reboot
     * @return ESP_OK on success (will reboot, never returns)
     */
    esp_err_t finish();

    /**
     * @brief Abort current OTA
     */
    void abort();

    /**
     * @brief Get current state
     */
    OtaState state() const { return state_; }

    /**
     * @brief Get progress (bytes received)
     */
    size_t bytesReceived() const { return bytesReceived_; }

    /**
     * @brief Set progress callback
     */
    void onProgress(OtaProgressCallback callback);

    /**
     * @brief Confirm current firmware is good (call after self-test)
     *
     * Must be called after OTA boot, otherwise rollback occurs.
     */
    static esp_err_t confirmFirmware();

    /**
     * @brief Force rollback to previous firmware
     */
    static esp_err_t rollback();

    /**
     * @brief Get running firmware version
     */
    static const char* getVersion();

private:
    OtaManager() = default;

    const esp_partition_t* updatePartition_ = nullptr;
    esp_ota_handle_t otaHandle_ = 0;
    OtaState state_ = OtaState::kIdle;
    size_t imageSize_ = 0;
    size_t bytesReceived_ = 0;
    OtaProgressCallback progressCb_;
};

}  // namespace domes
```

```cpp
// services/ota_manager.cpp
#include "ota_manager.hpp"
#include "esp_log.h"
#include "esp_app_format.h"

namespace domes {

namespace {
    constexpr const char* kTag = "ota";
}

OtaManager& OtaManager::instance() {
    static OtaManager inst;
    return inst;
}

esp_err_t OtaManager::begin(size_t imageSize, const uint8_t* expectedHash) {
    if (state_ != OtaState::kIdle) {
        return ESP_ERR_INVALID_STATE;
    }

    // Get next OTA partition
    updatePartition_ = esp_ota_get_next_update_partition(nullptr);
    if (!updatePartition_) {
        ESP_LOGE(kTag, "No OTA partition available");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(kTag, "OTA target partition: %s at 0x%lx, size %lu",
             updatePartition_->label,
             updatePartition_->address,
             updatePartition_->size);

    // Check size fits
    if (imageSize > updatePartition_->size) {
        ESP_LOGE(kTag, "Image too large: %u > %lu",
                 imageSize, updatePartition_->size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Begin OTA
    esp_err_t err = esp_ota_begin(updatePartition_, imageSize, &otaHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }

    imageSize_ = imageSize;
    bytesReceived_ = 0;
    state_ = OtaState::kReceiving;

    ESP_LOGI(kTag, "OTA started, expecting %u bytes", imageSize);
    return ESP_OK;
}

esp_err_t OtaManager::writeChunk(const uint8_t* data, size_t len) {
    if (state_ != OtaState::kReceiving) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_write(otaHandle_, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA write failed: %s", esp_err_to_name(err));
        state_ = OtaState::kError;
        return err;
    }

    bytesReceived_ += len;

    if (progressCb_) {
        progressCb_(bytesReceived_, imageSize_);
    }

    ESP_LOGD(kTag, "OTA progress: %u / %u bytes", bytesReceived_, imageSize_);
    return ESP_OK;
}

esp_err_t OtaManager::finish() {
    if (state_ != OtaState::kReceiving) {
        return ESP_ERR_INVALID_STATE;
    }

    state_ = OtaState::kVerifying;

    // Finalize OTA
    esp_err_t err = esp_ota_end(otaHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA end failed: %s", esp_err_to_name(err));
        state_ = OtaState::kError;
        return err;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(updatePartition_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Set boot partition failed: %s", esp_err_to_name(err));
        state_ = OtaState::kError;
        return err;
    }

    ESP_LOGI(kTag, "OTA complete! Rebooting...");
    state_ = OtaState::kRebooting;

    vTaskDelay(pdMS_TO_TICKS(100));  // Allow logs to flush
    esp_restart();

    return ESP_OK;  // Never reached
}

void OtaManager::abort() {
    if (state_ == OtaState::kReceiving) {
        esp_ota_abort(otaHandle_);
    }
    state_ = OtaState::kIdle;
    bytesReceived_ = 0;
    ESP_LOGW(kTag, "OTA aborted");
}

esp_err_t OtaManager::confirmFirmware() {
    esp_ota_img_states_t state;
    const esp_partition_t* running = esp_ota_get_running_partition();

    esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err != ESP_OK) {
        return err;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(kTag, "Confirming new firmware");
        return esp_ota_mark_app_valid_cancel_rollback();
    }

    return ESP_OK;  // Already confirmed
}

esp_err_t OtaManager::rollback() {
    ESP_LOGW(kTag, "Rolling back to previous firmware");
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}

const char* OtaManager::getVersion() {
    static char version[32];
    const esp_app_desc_t* desc = esp_app_get_description();
    snprintf(version, sizeof(version), "%s", desc->version);
    return version;
}

}  // namespace domes
```

---

## Self-Test After OTA

```cpp
// In main.cpp - run after boot

esp_err_t performSelfTest() {
    ESP_LOGI(kTag, "Running self-test...");

    // Test 1: All drivers initialize
    ESP_RETURN_ON_ERROR(ledDriver->init(), kTag, "LED init failed");
    ESP_RETURN_ON_ERROR(audioDriver->init(), kTag, "Audio init failed");
    ESP_RETURN_ON_ERROR(hapticDriver->init(), kTag, "Haptic init failed");

    // Test 2: Basic functionality
    ledDriver->setAll(Color::green());
    ledDriver->refresh();

    // Test 3: Check heap is reasonable
    if (esp_get_free_heap_size() < 50000) {
        ESP_LOGE(kTag, "Heap too low after init");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Self-test passed!");
    return ESP_OK;
}

extern "C" void app_main() {
    // Run self-test
    esp_err_t err = performSelfTest();

    if (err == ESP_OK) {
        // Confirm firmware is good
        OtaManager::confirmFirmware();
    } else {
        // Self-test failed - rollback!
        ESP_LOGE(kTag, "Self-test failed, rolling back");
        OtaManager::rollback();
    }

    // Continue normal boot...
}
```

---

## Version Management

```cpp
// CMakeLists.txt - set version at build time
set(PROJECT_VER "1.0.0")

// Or use git describe
execute_process(
    COMMAND git describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(PROJECT_VER ${GIT_VERSION})
```

---

## OTA via BLE

```cpp
// Handle OTA commands in BLE service
void handleOtaCommand(const uint8_t* data, size_t len) {
    switch (data[0]) {
        case 0x01:  // OTA_BEGIN
        {
            uint32_t size = *(uint32_t*)(data + 1);
            const uint8_t* hash = data + 5;
            OtaManager::instance().begin(size, hash);
            break;
        }
        case 0x02:  // OTA_DATA
        {
            OtaManager::instance().writeChunk(data + 1, len - 1);
            break;
        }
        case 0x03:  // OTA_END
        {
            OtaManager::instance().finish();
            break;
        }
        case 0x04:  // OTA_ABORT
        {
            OtaManager::instance().abort();
            break;
        }
    }
}
```

---

## Verification

```bash
# Check partition table
idf.py partition-table

# Check current boot partition
esptool.py --port /dev/ttyUSB0 read_flash 0xF000 0x2000 otadata.bin
# Parse with: python -c "import struct; print(struct.unpack('<I', open('otadata.bin','rb').read(4)))"

# Build and check size
idf.py build
stat -c%s build/domes.bin
# Must be < 4194304 bytes (4MB)

# Test OTA via HTTP (for debugging)
python -m http.server 8080 --directory build/
# Then trigger HTTP OTA from device
```

---

*Prerequisites: 02-build-system.md, 04-communication.md*
*Related: 09-reference.md (partition layout)*
