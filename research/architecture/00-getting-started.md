# 00 - Getting Started

## AI Agent Instructions

Load this file when:
- Setting up the project for the first time
- Creating the minimal bootable firmware
- Verifying the toolchain works

Prerequisites: None (this is the starting point)

---

## Step 1: Verify ESP-IDF Installation

```bash
# Check ESP-IDF is installed and sourced
. $HOME/esp/esp-idf/export.sh  # Adjust path as needed
idf.py --version
```

**Expected output:**
```
ESP-IDF v5.x.x
```

**If not installed:**
```bash
# Install ESP-IDF v5.2 (recommended)
mkdir -p ~/esp
cd ~/esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

---

## Step 2: Create Project Scaffold

```bash
cd /home/user/domes/firmware

# Create directory structure
mkdir -p main/{interfaces,drivers,services,game,utils}
mkdir -p main/platform
mkdir -p components
mkdir -p test/mocks
mkdir -p tools
```

**Verify:**
```bash
tree -L 2 /home/user/domes/firmware
```

---

## Step 3: Create Minimal Bootable Firmware

### 3.1 CMakeLists.txt (Root)

```cmake
# firmware/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(domes)
```

### 3.2 CMakeLists.txt (Main Component)

```cmake
# firmware/main/CMakeLists.txt
idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
        "interfaces"
        "drivers"
        "services"
        "game"
        "utils"
        "platform"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        esp_psram
)
```

### 3.3 Minimal main.cpp

```cpp
// firmware/main/main.cpp
// Milestone 0: Hello World - Minimal Bootable Firmware

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_psram.h"
#include "nvs_flash.h"

namespace {
    constexpr const char* kTag = "main";
}

extern "C" void app_main() {
    // Initialize NVS (required for WiFi/BLE later)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Log system info
    ESP_LOGI(kTag, "=================================");
    ESP_LOGI(kTag, "DOMES Firmware v0.0.1");
    ESP_LOGI(kTag, "=================================");

    // Chip info
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    ESP_LOGI(kTag, "Chip: ESP32-S3, %d cores, WiFi%s%s",
             chipInfo.cores,
             (chipInfo.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chipInfo.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    // Memory info
    ESP_LOGI(kTag, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(kTag, "PSRAM size: %lu bytes", esp_psram_get_size());

    // Heartbeat loop
    ESP_LOGI(kTag, "Entering main loop...");
    uint32_t counter = 0;

    while (true) {
        ESP_LOGI(kTag, "Heartbeat #%lu, heap: %lu",
                 counter++, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 3.4 SDK Configuration Defaults

```ini
# firmware/sdkconfig.defaults

# Target
CONFIG_IDF_TARGET="esp32s3"

# Flash size (16MB module)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# PSRAM (8MB octal)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# C++ settings
CONFIG_COMPILER_CXX_EXCEPTIONS=n
CONFIG_COMPILER_CXX_RTTI=n

# FreeRTOS
CONFIG_FREERTOS_HZ=1000

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y

# Watchdog
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
```

### 3.5 Partition Table

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

---

## Step 4: Build and Flash

### 4.1 Configure Target

```bash
cd /home/user/domes/firmware
idf.py set-target esp32s3
```

**Expected output:**
```
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

### 4.2 Build

```bash
idf.py build
```

**Expected output:**
```
...
Project build complete. To flash, run this command:
...
```

**Verify binary size:**
```bash
ls -lh build/domes.bin
# Should be < 1MB for hello world
```

### 4.3 Flash (requires DevKit connected)

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Expected UART output:**
```
I (XXX) main: =================================
I (XXX) main: DOMES Firmware v0.0.1
I (XXX) main: =================================
I (XXX) main: Chip: ESP32-S3, 2 cores, WiFi/BT/BLE
I (XXX) main: Free heap: XXXXXX bytes
I (XXX) main: PSRAM size: 8388608 bytes
I (XXX) main: Entering main loop...
I (XXX) main: Heartbeat #0, heap: XXXXXX
I (XXX) main: Heartbeat #1, heap: XXXXXX
...
```

**Exit monitor:** Press `Ctrl+]`

---

## Step 5: Verification Checklist

Run these checks to confirm Milestone 0 is complete:

| Check | Command | Pass Criteria |
|-------|---------|---------------|
| ESP-IDF version | `idf.py --version` | Shows v5.x |
| Build succeeds | `idf.py build` | Exit code 0 |
| Binary size | `stat -c%s build/domes.bin` | < 4194304 (4MB) |
| Flash succeeds | `idf.py flash` | Exit code 0 |
| Boot message | Monitor UART | "DOMES Firmware" appears |
| PSRAM detected | Monitor UART | "PSRAM size: 8388608" |
| No crash loop | Monitor 60s | Heartbeats continue |
| Heap stable | Monitor heap | No continuous decrease |

---

## Troubleshooting

### "No such file or directory: idf.py"
```bash
# Source ESP-IDF environment
. $HOME/esp/esp-idf/export.sh
```

### "Failed to connect to ESP32-S3"
1. Check USB cable (must be data cable, not charge-only)
2. Hold BOOT button while pressing RESET
3. Try different USB port
4. Check permissions: `sudo chmod 666 /dev/ttyUSB0`

### "PSRAM size: 0 bytes"
1. Verify module is N16R8 variant (not N16 without R)
2. Check sdkconfig has SPIRAM enabled
3. Run `idf.py menuconfig` → Component config → ESP PSRAM

### Build fails with C++ errors
1. Ensure `extern "C"` wrapper on `app_main`
2. Check sdkconfig has exceptions/RTTI disabled

---

## Next Steps

After Milestone 0 is verified:

1. **Add project structure** → See `01-project-structure.md`
2. **Configure build variants** → See `02-build-system.md`
3. **Implement first driver (LED)** → See `03-driver-development.md`

---

## Quick Commands Reference

```bash
# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Just monitor
idf.py -p /dev/ttyUSB0 monitor

# Clean build
idf.py fullclean

# Open menuconfig
idf.py menuconfig

# Check binary size
idf.py size

# Detailed size analysis
idf.py size-components
```

---

*Milestone: M0 (Hello World)*
*Prerequisites: ESP-IDF v5.x installed*
*Estimated time: 15 minutes*
