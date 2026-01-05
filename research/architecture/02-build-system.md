# 02 - Build System

## AI Agent Instructions

Load this file when:
- Configuring build options
- Setting up different build targets (DevKit, PCB, Linux)
- Troubleshooting build issues
- Checking binary size

Prerequisites: `00-getting-started.md` completed

---

## Build Configurations

### Configuration Matrix

| Target | Command | Use Case |
|--------|---------|----------|
| DevKit (default) | `idf.py build` | Development on ESP32-S3-DevKitC |
| DevKit Debug | `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.debug" build` | Verbose logging |
| PCB v1 | `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.pcb" build` | Custom PCB |
| Linux Host | `idf.py --preview set-target linux && idf.py build` | Unit tests |
| Release | `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" build` | Production |

---

## SDK Configuration Files

### sdkconfig.defaults (Base Configuration)

```ini
# firmware/sdkconfig.defaults

# Target chip
CONFIG_IDF_TARGET="esp32s3"

# Flash configuration (16MB module)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# Partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# PSRAM (8MB octal SPI)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y

# Compiler
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_COMPILER_CXX_EXCEPTIONS=n
CONFIG_COMPILER_CXX_RTTI=n
CONFIG_COMPILER_STACK_CHECK_MODE_STRONG=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3072
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y

# Task watchdog
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y

# Core dump
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_SHA256=y

# WiFi + BLE coexistence
CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE=y
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3

# Power management
CONFIG_PM_ENABLE=y
```

### sdkconfig.debug (Debug Build)

```ini
# firmware/sdkconfig.debug
# Overlay for debug builds

CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_COMPILER_OPTIMIZATION_DEBUG=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=30
CONFIG_BOOTLOADER_LOG_LEVEL_DEBUG=y

# Enable GDB stub on panic
CONFIG_ESP_SYSTEM_PANIC_GDBSTUB=y
```

### sdkconfig.release (Release Build)

```ini
# firmware/sdkconfig.release
# Overlay for production builds

CONFIG_LOG_DEFAULT_LEVEL_WARN=y
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y

# Disable debug features
CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=n

# Security
CONFIG_SECURE_BOOT=n  # Enable for production with signing keys
```

### sdkconfig.pcb (PCB v1 Target)

```ini
# firmware/sdkconfig.pcb
# Overlay for custom PCB

# Different pin assignments handled via Kconfig
CONFIG_DOMES_PLATFORM_PCB_V1=y
```

---

## Platform Selection via Kconfig

### Kconfig.projbuild

```kconfig
# firmware/main/Kconfig.projbuild

menu "DOMES Configuration"

choice DOMES_PLATFORM
    prompt "Target Platform"
    default DOMES_PLATFORM_DEVKIT
    help
        Select the target hardware platform

    config DOMES_PLATFORM_DEVKIT
        bool "ESP32-S3-DevKitC"
        help
            Development kit with standard pinout

    config DOMES_PLATFORM_PCB_V1
        bool "DOMES PCB v1"
        help
            First revision custom PCB

endchoice

config DOMES_LED_COUNT
    int "Number of LEDs in ring"
    default 16
    help
        Number of SK6812 LEDs in the ring

config DOMES_AUDIO_SAMPLE_RATE
    int "Audio sample rate (Hz)"
    default 16000
    help
        Sample rate for audio playback

endmenu
```

---

## CMake Configuration

### Root CMakeLists.txt

```cmake
# firmware/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Include component directories
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(domes)
```

### Main Component CMakeLists.txt

```cmake
# firmware/main/CMakeLists.txt
idf_component_register(
    SRCS
        "main.cpp"
        "drivers/led_driver.cpp"
        "drivers/audio_driver.cpp"
        "drivers/haptic_driver.cpp"
        "drivers/touch_driver.cpp"
        "drivers/imu_driver.cpp"
        "drivers/power_driver.cpp"
        "services/feedback_service.cpp"
        "services/comm_service.cpp"
        "services/timing_service.cpp"
        "services/config_service.cpp"
        "game/game_engine.cpp"
        "game/drill_manager.cpp"
        "game/state_machine.cpp"
        "game/protocol.cpp"
    INCLUDE_DIRS
        "."
        "interfaces"
        "drivers"
        "services"
        "game"
        "platform"
        "utils"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        esp_wifi
        esp_now
        bt
        esp_psram
        spiffs
)

# Enable C++20
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
```

---

## Build Commands

### Development Workflow

```bash
# Full clean and rebuild
idf.py fullclean && idf.py build

# Incremental build
idf.py build

# Build and flash
idf.py -p /dev/ttyUSB0 flash

# Build, flash, and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Just monitor (Ctrl+] to exit)
idf.py -p /dev/ttyUSB0 monitor
```

### Size Analysis

```bash
# Quick size check
idf.py size

# Detailed component breakdown
idf.py size-components

# File-level breakdown
idf.py size-files

# Check binary fits OTA partition
stat -c%s build/domes.bin
# Must be < 4194304 (4MB)
```

### Configuration

```bash
# Open menuconfig GUI
idf.py menuconfig

# Save configuration
# (automatically saved to sdkconfig)

# Reset to defaults
rm sdkconfig && idf.py reconfigure
```

---

## Linux Host Build (Unit Tests)

```bash
# Set target to Linux
idf.py --preview set-target linux

# Build for host
idf.py build

# Run the test binary
./build/domes.elf

# Switch back to ESP32-S3
idf.py set-target esp32s3
```

**Note:** Linux target requires mocking hardware drivers. See `06-testing.md`.

---

## Build Verification Checklist

| Check | Command | Pass Criteria |
|-------|---------|---------------|
| Clean build | `idf.py fullclean && idf.py build` | Exit 0, no errors |
| No warnings | Check build output | Zero warnings |
| Binary size | `stat -c%s build/domes.bin` | < 4194304 bytes |
| All symbols resolved | `idf.py build` | No undefined references |
| Size budget | `idf.py size-components` | Main < 1MB |

---

## Troubleshooting

### "undefined reference to..."

1. Check source file is listed in CMakeLists.txt SRCS
2. Check function has `extern "C"` if called from C
3. Check REQUIRES includes needed components

### "fatal error: xyz.h: No such file"

1. Check INCLUDE_DIRS in CMakeLists.txt
2. Check REQUIRES includes the component providing the header
3. Run `idf.py reconfigure`

### Binary too large

```bash
# Find largest components
idf.py size-components | head -20

# Common bloat sources:
# - <iostream> adds ~200KB - use ESP_LOG instead
# - Unused WiFi/BLE - disable in menuconfig
# - Debug symbols - use release config
```

### Build takes too long

```bash
# Parallel builds (usually automatic)
idf.py build -j$(nproc)

# Use ccache
export IDF_CCACHE_ENABLE=1
idf.py build
```

---

## Continuous Integration

```yaml
# .github/workflows/build.yml
name: Build Firmware
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: ESP-IDF Build
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.2
          target: esp32s3
          path: firmware

      - name: Check Size
        run: |
          SIZE=$(stat -c%s firmware/build/domes.bin)
          echo "Binary size: $SIZE bytes"
          if [ $SIZE -gt 4194304 ]; then
            echo "ERROR: Binary exceeds 4MB OTA limit"
            exit 1
          fi
```

---

*Prerequisites: 00-getting-started.md, 01-project-structure.md*
*Next: 03-driver-development.md*
