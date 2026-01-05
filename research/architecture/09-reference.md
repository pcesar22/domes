# 09 - Reference

## AI Agent Instructions

Load this file when:
- Looking up pin assignments
- Checking NVS key names
- Finding error codes
- Referencing timing constants
- Checking audio format specs

This file is a quick-reference lookup table. Load specific sections as needed.

---

## Pin Assignments

### DevKit (ESP32-S3-DevKitC)

| Function | GPIO | Peripheral | Notes |
|----------|------|------------|-------|
| **LEDs** |
| LED Data | GPIO48 | RMT Ch0 | SK6812 data line |
| **Audio** |
| I2S BCLK | GPIO15 | I2S0 | Bit clock |
| I2S LRCLK | GPIO16 | I2S0 | Word select |
| I2S DOUT | GPIO17 | I2S0 | Data out |
| **I2C Bus** |
| I2C SDA | GPIO1 | I2C0 | Shared: IMU + Haptic |
| I2C SCL | GPIO2 | I2C0 | Shared: IMU + Haptic |
| **Haptic** |
| Haptic EN | GPIO7 | GPIO | DRV2605L enable |
| **IMU** |
| IMU INT1 | GPIO6 | GPIO | Tap detection interrupt |
| **Touch** |
| Touch Pad | GPIO4 | Touch4 | Capacitive sense |
| **Power** |
| Battery ADC | GPIO5 | ADC1_CH4 | Voltage divider input |
| Charge Status | GPIO8 | GPIO | TP4056 CHRG pin |
| **USB** |
| USB D+ | GPIO20 | USB | Built-in USB-JTAG |
| USB D- | GPIO19 | USB | Built-in USB-JTAG |

### PCB v1 (Custom Board)

| Function | GPIO | Notes |
|----------|------|-------|
| LED Data | GPIO38 | Moved for layout |
| I2S BCLK | GPIO35 | |
| I2S LRCLK | GPIO36 | |
| I2S DOUT | GPIO37 | |
| I2C SDA | GPIO15 | |
| I2C SCL | GPIO16 | |
| Touch Pad | GPIO14 | Touch14 |
| Battery ADC | GPIO4 | ADC1_CH3 |
| IMU INT1 | GPIO18 | |
| Haptic EN | GPIO17 | |

### Pin Configuration Code

```cpp
// platform/pins_devkit.hpp
#pragma once
#include "driver/gpio.h"

namespace domes::pins {

// LEDs
constexpr gpio_num_t kLedData = GPIO_NUM_48;

// Audio (I2S)
constexpr gpio_num_t kI2sBclk = GPIO_NUM_15;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_16;
constexpr gpio_num_t kI2sDout = GPIO_NUM_17;

// I2C
constexpr gpio_num_t kI2cSda = GPIO_NUM_1;
constexpr gpio_num_t kI2cScl = GPIO_NUM_2;
constexpr i2c_port_t kI2cPort = I2C_NUM_0;

// IMU
constexpr gpio_num_t kImuInt1 = GPIO_NUM_6;

// Haptic
constexpr gpio_num_t kHapticEn = GPIO_NUM_7;

// Touch
constexpr gpio_num_t kTouchPad = GPIO_NUM_4;
constexpr touch_pad_t kTouchChannel = TOUCH_PAD_NUM4;

// Power
constexpr gpio_num_t kBatteryAdc = GPIO_NUM_5;
constexpr adc_channel_t kBatteryAdcChannel = ADC_CHANNEL_4;
constexpr gpio_num_t kChargeStatus = GPIO_NUM_8;

}  // namespace domes::pins
```

---

## I2C Device Addresses

| Device | Address (7-bit) | Address (8-bit R) | Address (8-bit W) |
|--------|-----------------|-------------------|-------------------|
| DRV2605L (Haptic) | 0x5A | 0xB5 | 0xB4 |
| LIS2DW12 (IMU) | 0x18 or 0x19 | 0x31/0x33 | 0x30/0x32 |
| MAX98357A | N/A | N/A | I2S only |

**Note:** LIS2DW12 address depends on SA0 pin: 0x18 (SA0=GND), 0x19 (SA0=VCC)

---

## NVS Schema

### Namespace: `config`

| Key | Type | Size | Description | Default |
|-----|------|------|-------------|---------|
| `pod_id` | u8 | 1 | Unique pod identifier (0-255) | 0 |
| `led_brightness` | u8 | 1 | Default LED brightness (0-255) | 128 |
| `haptic_intensity` | u8 | 1 | Default vibration (0-255) | 200 |
| `audio_volume` | u8 | 1 | Default volume (0-255) | 180 |
| `touch_threshold` | u16 | 2 | Capacitive threshold | 500 |
| `is_master` | u8 | 1 | Master pod flag (0/1) | 0 |

### Namespace: `network`

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `master_mac` | blob | 6 | Master pod MAC address |
| `channel` | u8 | 1 | WiFi/ESP-NOW channel (1-13) |
| `peer_count` | u8 | 1 | Number of known peers |
| `peers` | blob | 120 | Up to 20 peer MACs (6 bytes each) |

### Namespace: `calibration`

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `imu_offset_x` | i16 | 2 | X-axis offset (mg) |
| `imu_offset_y` | i16 | 2 | Y-axis offset (mg) |
| `imu_offset_z` | i16 | 2 | Z-axis offset (mg) |
| `touch_baseline` | u16 | 2 | Touch baseline reading |

### Namespace: `stats`

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `boot_count` | u32 | 4 | Total boot count |
| `total_touches` | u32 | 4 | Lifetime touch count |
| `total_runtime` | u32 | 4 | Runtime in seconds |
| `last_error` | u32 | 4 | Last error code |

### NVS Usage Example

```cpp
#include "nvs_flash.h"
#include "nvs.h"

esp_err_t loadConfig() {
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(
        nvs_open("config", NVS_READONLY, &handle),
        kTag, "NVS open failed"
    );

    uint8_t brightness = 128;  // Default
    nvs_get_u8(handle, "led_brightness", &brightness);

    uint16_t threshold = 500;  // Default
    nvs_get_u16(handle, "touch_threshold", &threshold);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t saveConfig() {
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(
        nvs_open("config", NVS_READWRITE, &handle),
        kTag, "NVS open failed"
    );

    nvs_set_u8(handle, "led_brightness", currentBrightness);
    nvs_set_u16(handle, "touch_threshold", currentThreshold);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}
```

---

## Error Codes

### Project-Specific Errors

```cpp
// utils/error_codes.hpp
#pragma once
#include "esp_err.h"

namespace domes {

// Base for project errors (ESP-IDF uses 0x0000-0x0FFF)
constexpr esp_err_t kErrBase = 0x10000;

// Driver errors (0x10000-0x100FF)
constexpr esp_err_t kErrDriverNotInit      = kErrBase + 0x01;
constexpr esp_err_t kErrDriverAlreadyInit  = kErrBase + 0x02;
constexpr esp_err_t kErrDriverBusy         = kErrBase + 0x03;
constexpr esp_err_t kErrDriverTimeout      = kErrBase + 0x04;
constexpr esp_err_t kErrDriverHwFault      = kErrBase + 0x05;

// Communication errors (0x10100-0x101FF)
constexpr esp_err_t kErrEspNowPeerNotFound = kErrBase + 0x100;
constexpr esp_err_t kErrEspNowSendFailed   = kErrBase + 0x101;
constexpr esp_err_t kErrEspNowQueueFull    = kErrBase + 0x102;
constexpr esp_err_t kErrBleNotConnected    = kErrBase + 0x110;
constexpr esp_err_t kErrBleNotifyFailed    = kErrBase + 0x111;

// Game errors (0x10200-0x102FF)
constexpr esp_err_t kErrDrillInvalidState  = kErrBase + 0x200;
constexpr esp_err_t kErrDrillTimeout       = kErrBase + 0x201;
constexpr esp_err_t kErrDrillAborted       = kErrBase + 0x202;

// OTA errors (0x10300-0x103FF)
constexpr esp_err_t kErrOtaInProgress      = kErrBase + 0x300;
constexpr esp_err_t kErrOtaVerifyFailed    = kErrBase + 0x301;
constexpr esp_err_t kErrOtaRollback        = kErrBase + 0x302;

const char* domesErrToName(esp_err_t err);

}  // namespace domes
```

---

## Timing Constants

### System Timing

| Constant | Value | Description |
|----------|-------|-------------|
| FreeRTOS tick | 1ms | `CONFIG_FREERTOS_HZ=1000` |
| Watchdog timeout | 10s | Task watchdog |
| Boot timeout | 5s | Max boot time |
| Heartbeat interval | 1s | Status LED blink |

### Communication Timing

| Constant | Value | Description |
|----------|-------|-------------|
| ESP-NOW P50 latency | < 1ms | Target median |
| ESP-NOW P95 latency | < 2ms | Target 95th percentile |
| BLE conn interval | 30-50ms | Responsive mode |
| BLE conn interval | 100-200ms | Power save mode |
| Clock sync interval | 100ms | Master broadcasts |

### Game Timing

| Constant | Value | Description |
|----------|-------|-------------|
| Touch debounce | 50ms | Ignore rapid touches |
| Reaction timeout | 3-5s | Max wait for touch |
| Feedback duration | 200ms | LED/haptic pulse |
| Min random delay | 500ms | Before arming |
| Max random delay | 2000ms | Before arming |

### Constants in Code

```cpp
// config/timing.hpp
#pragma once
#include <cstdint>

namespace domes::timing {

// System
constexpr uint32_t kWatchdogTimeoutS = 10;
constexpr uint32_t kHeartbeatIntervalMs = 1000;

// Communication
constexpr uint32_t kEspNowLatencyTargetUs = 2000;   // P95
constexpr uint32_t kClockSyncIntervalMs = 100;
constexpr uint32_t kBleConnIntervalMinMs = 30;
constexpr uint32_t kBleConnIntervalMaxMs = 50;

// Game
constexpr uint32_t kTouchDebounceMs = 50;
constexpr uint32_t kDefaultReactionTimeoutMs = 3000;
constexpr uint32_t kFeedbackDurationMs = 200;
constexpr uint32_t kMinRandomDelayMs = 500;
constexpr uint32_t kMaxRandomDelayMs = 2000;

// Accuracy target
constexpr uint32_t kTimeSyncAccuracyUs = 1000;  // ±1ms

}  // namespace domes::timing
```

---

## Audio Format

### Specification

| Parameter | Value |
|-----------|-------|
| Format | Raw PCM (no header) |
| Sample rate | 16000 Hz |
| Bit depth | 16-bit signed |
| Channels | Mono |
| Byte order | Little-endian |
| Max duration | ~10s per file (320KB) |

### File Structure

```
spiffs/
└── sounds/
    ├── feedback/
    │   ├── hit_success.raw     # Touch registered
    │   ├── hit_fail.raw        # Wrong pod / timeout
    │   └── countdown_beep.raw  # Countdown tick
    ├── system/
    │   ├── boot.raw            # Startup sound
    │   ├── connected.raw       # BLE connected
    │   ├── low_battery.raw     # Battery warning
    │   └── ota_complete.raw    # Update finished
    └── drills/
        ├── go.raw              # Start signal
        └── complete.raw        # Drill finished
```

### Conversion Command

```bash
# Convert any audio to correct format
ffmpeg -i input.mp3 \
    -f s16le \
    -acodec pcm_s16le \
    -ar 16000 \
    -ac 1 \
    output.raw

# Check file info
ffprobe -f s16le -ar 16000 -ac 1 -i sound.raw
```

### Audio Playback

```cpp
void playRawAudio(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return;

    int16_t buffer[256];
    size_t bytesWritten;

    while (fread(buffer, sizeof(int16_t), 256, f) > 0) {
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer),
                  &bytesWritten, portMAX_DELAY);
    }

    fclose(f);
}
```

---

## Memory Budget

### SRAM Usage (~512KB total)

| Component | Estimated | Notes |
|-----------|-----------|-------|
| WiFi stack | 40KB | Static + buffers |
| BLE stack | 40KB | NimBLE |
| FreeRTOS | 20KB | Kernel + idle tasks |
| Task stacks | 32KB | 8 tasks × 4KB avg |
| Static data | 16KB | Global variables |
| Heap (app) | 100KB | Available for app |
| **Reserved** | ~250KB | For ESP-IDF internals |

### PSRAM Usage (8MB total)

| Component | Allocated | Notes |
|-----------|-----------|-------|
| Audio buffers | 64KB | Double-buffered playback |
| Large data | As needed | Heap overflow |
| **Available** | ~7.9MB | For future use |

### Flash Usage (16MB total)

| Partition | Size | Notes |
|-----------|------|-------|
| Bootloader | 32KB | |
| Partition table | 4KB | |
| NVS | 24KB | Config storage |
| OTA data | 8KB | Boot selection |
| OTA_0 | 4MB | Firmware A |
| OTA_1 | 4MB | Firmware B |
| SPIFFS | 6MB | Audio files |
| Core dump | 64KB | Crash data |

---

## BLE UUIDs

### Service

| Name | UUID (16-bit) | UUID (128-bit) |
|------|---------------|----------------|
| DOMES Service | 0x1234 | 00001234-0000-1000-8000-00805F9B34FB |

### Characteristics

| Name | UUID | Properties |
|------|------|------------|
| Pod Status | 0x1235 | Read, Notify |
| Drill Control | 0x1236 | Write |
| Touch Event | 0x1237 | Notify |
| Configuration | 0x1238 | Read, Write |
| OTA Control | 0x1239 | Write |
| OTA Data | 0x123A | Write |

---

## ESP-IDF Component Versions

```ini
# Recommended versions (in idf_component.yml or managed manually)
esp_wifi: "^5.0"
nvs_flash: "^5.0"
esp_timer: "^5.0"
driver: "^5.0"
bt: "^5.0"  # NimBLE
spiffs: "^5.0"
```

---

*This reference document is for quick lookups.*
*See specific architecture docs for detailed implementation.*
