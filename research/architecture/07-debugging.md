# 07 - Debugging

## AI Agent Instructions

Load this file when:
- Setting up debugging tools
- Diagnosing crashes or hangs
- Analyzing core dumps
- Troubleshooting common issues

Prerequisites: `00-getting-started.md`

---

## Debug Setup Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Development Machine                       │
├─────────────────────────────────────────────────────────────┤
│  VS Code ◄──────► GDB ◄──────► OpenOCD ◄──────┐             │
│                                               │             │
│  Serial Monitor ◄─────────────────────────────┼──┐          │
└───────────────────────────────────────────────┼──┼──────────┘
                                                │  │
                                    USB-C ──────┘  │
                                    (JTAG)         │ (UART)
                                                   │
┌─────────────────────────────────────────────────┼───────────┐
│                    ESP32-S3                      │           │
│  ┌─────────────┐                                 │           │
│  │ USB-JTAG    │ GPIO19/20 (built-in)           │           │
│  └─────────────┘                                 │           │
│  ┌─────────────┐                                 │           │
│  │ UART0       │ TX/RX for logging ─────────────┘           │
│  └─────────────┘                                             │
└─────────────────────────────────────────────────────────────┘
```

**Key Advantage:** ESP32-S3 has built-in USB-JTAG - no external debugger needed!

---

## Logging

### Log Macros

```cpp
#include "esp_log.h"

// Define tag for each module
static constexpr const char* kTag = "module_name";

// Log levels (in order of severity)
ESP_LOGE(kTag, "Error: %s", esp_err_to_name(err));   // Always shown
ESP_LOGW(kTag, "Warning: threshold low");            // Important issues
ESP_LOGI(kTag, "Info: initialized");                 // Normal operation
ESP_LOGD(kTag, "Debug: register=0x%02X", reg);       // Debugging
ESP_LOGV(kTag, "Verbose: entering function");        // Trace-level
```

### Runtime Log Level Control

```cpp
// Set level for specific module
esp_log_level_set("haptic", ESP_LOG_DEBUG);

// Set default for all modules
esp_log_level_set("*", ESP_LOG_INFO);

// Silence noisy modules
esp_log_level_set("wifi", ESP_LOG_WARN);
esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);
```

### Log Output Format

```
I (1234) module: Message text
│  │     │       └── Message
│  │     └────────── Tag (module name)
│  └──────────────── Timestamp (ms since boot)
└─────────────────── Level: E/W/I/D/V
```

---

## GDB Debugging

### Start OpenOCD

```bash
# Terminal 1: Start OpenOCD
openocd -f board/esp32s3-builtin.cfg

# Expected output:
# Info : Listening on port 3333 for gdb connections
```

### Connect GDB

```bash
# Terminal 2: Connect GDB
xtensa-esp32s3-elf-gdb -x gdbinit build/domes.elf

# gdbinit contents:
# target remote :3333
# mon reset halt
# thb app_main
# c
```

### Common GDB Commands

```gdb
# Breakpoints
break app_main              # Break at function
break led_driver.cpp:42     # Break at file:line
info breakpoints            # List breakpoints
delete 1                    # Delete breakpoint 1

# Execution
continue                    # Resume
next                        # Step over
step                        # Step into
finish                      # Run until function returns
until 50                    # Run until line 50

# Inspection
print variable              # Print variable value
print/x variable            # Print as hex
info locals                 # Show local variables
info args                   # Show function arguments
backtrace                   # Show call stack
frame 2                     # Select stack frame

# Memory
x/10xw 0x3FC00000          # Examine 10 words at address
x/s 0x3FC00000             # Examine as string

# Registers
info registers              # All registers
print $pc                   # Program counter

# Reset
mon reset halt              # Reset and halt
mon reset run               # Reset and run
```

### VS Code Debug Configuration

```json
// .vscode/launch.json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "ESP-IDF Debug",
            "type": "espidf",
            "request": "launch",
            "mode": "auto",
            "debugPort": 9998,
            "env": {
                "IDF_PATH": "${config:idf.espIdfPath}"
            }
        }
    ]
}
```

---

## Core Dump Analysis

### Enable Core Dump

In `sdkconfig.defaults`:
```ini
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_SHA256=y
```

### Trigger and Retrieve

```bash
# After crash, retrieve core dump
idf.py coredump-info -p /dev/ttyUSB0

# For detailed analysis
idf.py coredump-debug -p /dev/ttyUSB0

# This opens GDB with the core dump loaded
```

### Core Dump Output Example

```
===============================================================
==================== ESP32 CORE DUMP START ====================

Crashed task handle: 0x3fceb8b0, name: 'game', GDB name: 'process 1073668272'

================== CURRENT THREAD REGISTERS ===================
exccause       0x1d (StoreProhibitedCause)
excvaddr       0x0
pc             0x42012abc
...

==================== CURRENT THREAD STACK =====================
#0  0x42012abc in feedbackService::playSuccess() at feedback_service.cpp:45
#1  0x42011234 in gameEngine::onTouch() at game_engine.cpp:123
#2  0x42010000 in gameTask() at main.cpp:89
```

---

## Common Issues & Solutions

### Issue: "Failed to connect to ESP32"

**Symptoms:** `esptool.py` can't connect

**Solutions:**
1. Check USB cable is data-capable (not charge-only)
2. Hold BOOT button while pressing RESET
3. Check permissions: `sudo chmod 666 /dev/ttyUSB0`
4. Try different USB port
5. Check if another process has the port open

### Issue: WiFi init fails

**Symptoms:** `ESP_ERR_WIFI_NOT_INIT` or crash during wifi_init

**Checklist:**
```cpp
// 1. NVS must be initialized first
ESP_ERROR_CHECK(nvs_flash_init());

// 2. Event loop must exist
ESP_ERROR_CHECK(esp_event_loop_create_default());

// 3. Check heap before init
ESP_LOGI(kTag, "Free heap: %lu", esp_get_free_heap_size());
// Need > 100KB free

// 4. Initialize with default config
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
```

### Issue: I2C device not responding

**Symptoms:** `ESP_ERR_TIMEOUT` on I2C operations

**Checklist:**
1. Verify pull-ups on SDA/SCL (4.7kΩ to 3.3V)
2. Check device address (7-bit vs 8-bit notation)
3. Verify power to device
4. Run I2C scan:

```cpp
void i2cScan() {
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            ESP_LOGI(kTag, "Found device at 0x%02X", addr);
        }
    }
}
```

### Issue: Stack overflow

**Symptoms:** `Task stack overflow` panic, random crashes

**Diagnosis:**
```cpp
// Check stack high water mark
void checkStack() {
    UBaseType_t remaining = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(kTag, "Stack remaining: %u words (%u bytes)",
             remaining, remaining * sizeof(StackType_t));

    if (remaining < 100) {
        ESP_LOGW(kTag, "Stack low!");
    }
}
```

**Fix:** Increase stack size in `xTaskCreate()`:
```cpp
// Increase from 4096 to 8192
xTaskCreate(myTask, "name", 8192, nullptr, 5, nullptr);
```

### Issue: Watchdog timeout

**Symptoms:** `Task watchdog got triggered` in logs

**Causes:**
1. Blocking operation without feeding watchdog
2. Infinite loop
3. Long ISR

**Solutions:**
```cpp
// Feed watchdog in long operations
esp_task_wdt_reset();

// Disable watchdog for specific task (temporary debug)
esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
```

### Issue: Heap exhaustion

**Symptoms:** `malloc failed` or `out of memory`

**Diagnosis:**
```cpp
// Check heap
ESP_LOGI(kTag, "Free heap: %lu", esp_get_free_heap_size());
ESP_LOGI(kTag, "Min free: %lu", esp_get_minimum_free_heap_size());
ESP_LOGI(kTag, "Largest block: %lu", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
```

**Common causes:**
- WiFi + BLE use ~80KB
- Memory leaks (allocate without free)
- Large static buffers

### Issue: PSRAM not detected

**Symptoms:** `esp_psram_get_size()` returns 0

**Checklist:**
1. Verify module is N16R8 (not N16 without R)
2. Check `sdkconfig`:
```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```
3. Run `idf.py menuconfig` → Component config → ESP PSRAM

---

## Performance Profiling

### Task Runtime Stats

```cpp
// Enable in sdkconfig
// CONFIG_FREERTOS_USE_TRACE_FACILITY=y
// CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y

void printTaskStats() {
    char buffer[1024];
    vTaskGetRunTimeStats(buffer);
    printf("%s\n", buffer);
}
```

### Timing Measurement

```cpp
#include "esp_timer.h"

int64_t start = esp_timer_get_time();
// ... operation ...
int64_t elapsed = esp_timer_get_time() - start;
ESP_LOGI(kTag, "Operation took %lld us", elapsed);
```

### CPU Usage

```cpp
void printCpuUsage() {
    // Requires trace facility enabled
    TaskStatus_t tasks[20];
    uint32_t totalRuntime;

    UBaseType_t count = uxTaskGetSystemState(tasks, 20, &totalRuntime);

    for (int i = 0; i < count; i++) {
        uint32_t percent = (tasks[i].ulRunTimeCounter * 100) / totalRuntime;
        ESP_LOGI(kTag, "%s: %lu%%", tasks[i].pcTaskName, percent);
    }
}
```

---

## Verification Commands

```bash
# Check OpenOCD connects
openocd -f board/esp32s3-builtin.cfg
# Expected: "Listening on port 3333"

# Test GDB
xtensa-esp32s3-elf-gdb -ex "target remote :3333" -ex "info reg" -ex "quit" build/domes.elf
# Expected: Register dump

# Check for core dump
idf.py coredump-info -p /dev/ttyUSB0
# Expected: Core dump info or "No core dump found"
```

---

*Prerequisites: 00-getting-started.md*
*Related: 02-build-system.md (debug builds)*
