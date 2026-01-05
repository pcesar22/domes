# 09 - Reference

## AI Agent Instructions

Load this file for quick lookups: pins, NVS keys, error codes, timing constants.

---

## Pin Assignments

### DevKit (ESP32-S3-DevKitC)

| Function | GPIO | Peripheral | Notes |
|----------|------|------------|-------|
| LED Data | 48 | RMT Ch0 | SK6812 data |
| I2S BCLK | 15 | I2S0 | Bit clock |
| I2S LRCLK | 16 | I2S0 | Word select |
| I2S DOUT | 17 | I2S0 | Data out |
| I2C SDA | 1 | I2C0 | Shared bus |
| I2C SCL | 2 | I2C0 | Shared bus |
| IMU INT1 | 6 | GPIO | Tap interrupt |
| Haptic EN | 7 | GPIO | DRV2605L enable |
| Touch Pad | 4 | Touch4 | Capacitive |
| Battery ADC | 5 | ADC1_CH4 | Voltage divider |
| Charge Status | 8 | GPIO | TP4056 CHRG |
| USB D+ | 20 | USB | Built-in JTAG |
| USB D- | 19 | USB | Built-in JTAG |

### PCB v1 (Custom Board)

| Function | GPIO | Notes |
|----------|------|-------|
| LED Data | 38 | Moved for layout |
| I2S BCLK | 35 | |
| I2S LRCLK | 36 | |
| I2S DOUT | 37 | |
| I2C SDA | 15 | |
| I2C SCL | 16 | |
| Touch Pad | 14 | Touch14 |
| Battery ADC | 4 | ADC1_CH3 |
| IMU INT1 | 18 | |
| Haptic EN | 17 | |

---

## I2C Devices

| Device | 7-bit Addr | 8-bit Write | 8-bit Read | Notes |
|--------|------------|-------------|------------|-------|
| DRV2605L (Haptic) | 0x5A | 0xB4 | 0xB5 | Fixed |
| LIS2DW12 (IMU) | 0x18 | 0x30 | 0x31 | SA0=GND |
| LIS2DW12 (IMU) | 0x19 | 0x32 | 0x33 | SA0=VCC |
| MAX98357A | N/A | N/A | N/A | I2S only |

---

## NVS Schema

### Namespace: `config`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `pod_id` | u8 | 0 | Pod identifier (0-255) |
| `led_brightness` | u8 | 128 | Default brightness |
| `haptic_intensity` | u8 | 200 | Vibration level |
| `audio_volume` | u8 | 180 | Sound volume |
| `touch_threshold` | u16 | 500 | Capacitive threshold |
| `is_master` | u8 | 0 | Master flag (0/1) |

### Namespace: `network`

| Key | Type | Size | Description |
|-----|------|------|-------------|
| `master_mac` | blob | 6 | Master MAC address |
| `channel` | u8 | 1 | WiFi/ESP-NOW channel |
| `peer_count` | u8 | 1 | Known peer count |
| `peers` | blob | 120 | Up to 20 peer MACs |

### Namespace: `calibration`

| Key | Type | Description |
|-----|------|-------------|
| `imu_offset_x` | i16 | X-axis offset (mg) |
| `imu_offset_y` | i16 | Y-axis offset (mg) |
| `imu_offset_z` | i16 | Z-axis offset (mg) |
| `touch_baseline` | u16 | Touch baseline |

### Namespace: `stats`

| Key | Type | Description |
|-----|------|-------------|
| `boot_count` | u32 | Total boots |
| `total_touches` | u32 | Lifetime touches |
| `total_runtime` | u32 | Runtime (seconds) |
| `last_error` | u32 | Last error code |

---

## Error Codes

| Range | Category | Examples |
|-------|----------|----------|
| 0x10001-0x100FF | Driver | Not init, already init, busy, timeout, HW fault |
| 0x10100-0x101FF | ESP-NOW | Peer not found, send failed, queue full |
| 0x10110-0x1011F | BLE | Not connected, notify failed |
| 0x10200-0x102FF | Game | Invalid state, timeout, aborted |
| 0x10300-0x103FF | OTA | In progress, verify failed, rollback |

---

## Timing Constants

### System

| Constant | Value | Description |
|----------|-------|-------------|
| FreeRTOS tick | 1ms | CONFIG_FREERTOS_HZ=1000 |
| Watchdog timeout | 10s | Task watchdog |
| Boot timeout | 5s | Max boot time |
| Heartbeat | 1s | Status LED blink |

### Communication

| Constant | Value | Description |
|----------|-------|-------------|
| ESP-NOW P50 | < 1ms | Median latency |
| ESP-NOW P95 | < 2ms | 95th percentile |
| BLE conn interval | 30-50ms | Responsive mode |
| BLE conn interval | 100-200ms | Power save mode |
| Clock sync interval | 100ms | Master broadcasts |

### Game

| Constant | Value | Description |
|----------|-------|-------------|
| Touch debounce | 50ms | Ignore rapid |
| Reaction timeout | 3-5s | Max wait |
| Feedback duration | 200ms | LED/haptic pulse |
| Min random delay | 500ms | Before arming |
| Max random delay | 2000ms | Before arming |
| Time sync accuracy | ±1ms | Target |

---

## Audio Format

| Parameter | Value |
|-----------|-------|
| Format | Raw PCM (no header) |
| Sample rate | 16000 Hz |
| Bit depth | 16-bit signed |
| Channels | Mono |
| Byte order | Little-endian |
| Max duration | ~10s (320KB) |

**Conversion:** `ffmpeg -i input.mp3 -f s16le -ar 16000 -ac 1 output.raw`

### Sound Files

```
spiffs/sounds/
├── feedback/
│   ├── hit_success.raw
│   ├── hit_fail.raw
│   └── countdown_beep.raw
├── system/
│   ├── boot.raw
│   ├── connected.raw
│   └── low_battery.raw
└── drills/
    ├── go.raw
    └── complete.raw
```

---

## Memory Budget

### SRAM (~512KB)

| Component | Size | Notes |
|-----------|------|-------|
| WiFi stack | 40KB | Static + buffers |
| BLE (NimBLE) | 40KB | |
| FreeRTOS | 20KB | Kernel + idle |
| Task stacks | 32KB | 8 tasks avg |
| Static data | 16KB | Globals |
| App heap | 100KB | Available |
| ESP-IDF | ~250KB | Reserved |

### PSRAM (8MB)

| Component | Size | Notes |
|-----------|------|-------|
| Audio buffers | 64KB | Double-buffered |
| Large data | As needed | Heap overflow |
| Available | ~7.9MB | Future use |

### Flash (16MB)

| Partition | Size | Purpose |
|-----------|------|---------|
| Bootloader | 32KB | |
| Partition table | 4KB | |
| NVS | 24KB | Config |
| OTA data | 8KB | Boot selection |
| OTA_0 | 4MB | Firmware A |
| OTA_1 | 4MB | Firmware B |
| SPIFFS | 6MB | Audio files |
| Core dump | 64KB | Crash data |

---

## BLE UUIDs

### Service

| Name | UUID (16-bit) |
|------|---------------|
| DOMES Service | 0x1234 |

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

*Quick reference document. See specific architecture docs for details.*
