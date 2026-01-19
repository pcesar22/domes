# DOMES Firmware Development Milestones

## Current Status Summary

**Last Updated:** 2026-01-18

| Category | Status |
|----------|--------|
| **Implemented & Tested** | LED driver, FreeRTOS infrastructure, NVS config, watchdog, logging, runtime config protocol, OTA system, performance tracing |
| **Implemented (Needs Hardware)** | WiFi manager, TCP config server, BLE OTA service |
| **Not Started** | Touch driver, I2C drivers (haptic, IMU), audio driver, game engine |
| **Blocking** | NFF devboard assembly (enables I2C/audio/full LED ring testing) |

### What Actually Works Right Now

On **ESP32-S3-DevKitC-1**:
- ✅ Firmware boots and runs stable
- ✅ Single LED color cycling demo
- ✅ USB-CDC serial communication
- ✅ Runtime config protocol (list/set features)
- ✅ Serial OTA firmware updates
- ✅ Performance tracing with Perfetto export
- ✅ NVS persistence (boot counter, settings)
- ✅ WiFi connection (if credentials configured)
- ✅ TCP config server (if WiFi enabled)

On **NFF Development Board** (pending assembly):
- 🔲 16x RGBW LED ring
- 🔲 LIS2DW12 accelerometer + tap detection
- 🔲 DRV2605L haptic feedback
- 🔲 MAX98357A audio playback

---

## Hardware Progression

| Stage | Platform | Features Available |
|-------|----------|-------------------|
| **Current** | ESP32-S3-DevKitC-1 | 1x LED, USB, touch pins |
| **Next** | [NFF Development Board](../hardware/nff-devboard/) | 16x RGBW LEDs, IMU, haptics, audio |
| **Future** | Production PCB | Full system in enclosure |

**Pin Reference:** See [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) for complete GPIO mappings.

## Current Hardware: ESP32-S3-DevKitC-1 (bare, no peripherals)

---

## Phase 1: Project Skeleton
**Status:** ✅ Complete
**Goal:** Proper ESP-IDF project structure that builds and runs

- [x] Create directory structure matching architecture docs
- [x] Configure sdkconfig.defaults for ESP32-S3 + USB console
- [x] Create partitions.csv for OTA layout
- [x] Set up config.hpp with pin definitions
- [x] Verify build succeeds

---

## Phase 2: RGB LED Driver (DevKit Built-in)
**Status:** ✅ Complete
**Goal:** Validate LED driver pattern using DevKitC-1's onboard LED

- [x] Create ILedDriver interface
- [x] Implement SPI-based WS2812 driver (see learnings below)
- [x] Test color cycling on devkit LED
- [x] Validate timing accuracy

**Hardware:** DevKitC-1 v1.1 has 1x WS2812 RGB LED on GPIO38

### Phase 2 Learnings

**Critical Discovery: SPI Backend Required on DevKitC-1 v1.1**

The ESP32-S3-DevKitC-1 v1.1 board has 8MB Octal PSRAM which creates timing conflicts
with the RMT (Remote Control) peripheral on certain GPIOs. The RMT backend failed
silently - LED initialized without errors but never lit up.

**Solution:** Use SPI backend instead of RMT for the `led_strip` component:

```cpp
// SPI backend configuration (works on v1.1 with PSRAM)
led_strip_spi_config_t spi_config = {};
memset(&spi_config, 0, sizeof(spi_config));
spi_config.spi_bus = SPI2_HOST;
spi_config.flags.with_dma = true;

esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &led_strip);
```

**Key Points:**
- DevKitC-1 v1.0 uses GPIO48, v1.1 uses GPIO38
- Always zero-initialize config structs with `memset()` in C++ to avoid garbage values
- SPI backend uses DMA and is more reliable on boards with Octal PSRAM
- The `led_strip` component v2.5.x supports both RMT and SPI backends

---

## Phase 3: Touch Sensing (No External Hardware)
**Status:** Not Started
**Goal:** Validate touch driver using bare GPIO pins

- [ ] Create ITouchDriver interface
- [ ] Implement ESP32-S3 touch peripheral driver
- [ ] Test touch detection on GPIO1-4 (touch pins with finger)
- [ ] Implement baseline calibration
- [ ] Implement threshold detection

**Hardware:** Touch GPIO1-14 with finger directly

---

## Phase 4: Infrastructure
**Status:** ✅ Complete
**Goal:** Core firmware infrastructure

- [x] FreeRTOS task structure and pinning (`infra/taskManager`, `infra/taskConfig`)
- [x] Logging framework (ESP_LOG wrappers in `infra/logging`)
- [x] Configuration management (NVS via `infra/nvsConfig`)
- [x] Error handling patterns (interfaces with `esp_err_t`)
- [x] Watchdog setup (`infra/watchdog` with RAII guard)

### Phase 4 Implementation Details

**Files Created:**
- `main/infra/taskConfig.hpp` - Task priorities, core affinity, stack sizes
- `main/infra/taskManager.hpp/.cpp` - Managed task lifecycle with core pinning
- `main/infra/watchdog.hpp/.cpp` - TWDT wrapper with WatchdogGuard RAII
- `main/infra/nvsConfig.hpp/.cpp` - NVS configuration with namespaces
- `main/infra/logging.hpp` - DOMES_LOG* macros and module tags
- `main/utils/mutex.hpp` - RAII mutex wrapper
- `main/interfaces/iTaskRunner.hpp` - Task abstraction interface
- `main/interfaces/iConfigStorage.hpp` - Config storage interface

**NVS Namespaces:**
- `config` - User settings (brightness, volume, thresholds)
- `stats` - Runtime statistics (boot count, uptime)
- `calibration` - Sensor calibration data

**Boot Counter:** Firmware now tracks and logs boot count on each startup

---

## Phase 5: I2C Peripherals (Requires Hardware)
**Status:** Blocked - waiting for hardware
**Goal:** Haptic and IMU drivers

- [ ] I2C bus initialization
- [ ] DRV2605L haptic driver
- [ ] LIS2DW12 accelerometer driver
- [ ] I2C device scanning utility

**Hardware Required:**
- DRV2605L breakout board
- LIS2DW12 breakout board
- LRA motor

---

## Phase 6: Audio (Requires Hardware)
**Status:** Blocked - waiting for hardware
**Goal:** I2S audio playback

- [ ] I2S driver configuration
- [ ] MAX98357A amplifier driver
- [ ] Audio sample playback from SPIFFS
- [ ] Volume control

**Hardware Required:**
- MAX98357A breakout board
- 8Ω speaker

---

## Phase 7: Communication Stack
**Status:** Not Started
**Goal:** ESP-NOW and BLE initialization and basic functionality

- [ ] WiFi stack init for ESP-NOW (verify `wifi:mode : sta` in logs)
- [ ] ESP-NOW point-to-point test
- [ ] ESP-NOW broadcast/multicast
- [ ] BLE stack init (verify `NimBLE: GAP` in logs)
- [ ] BLE GATT server setup
- [ ] BLE OTA service

**Hardware Required:** 2+ ESP32-S3 DevKits

**Note:** WiFi is only used as a dependency for ESP-NOW (P2P radio mode) - we don't
connect to any AP or use WiFi networking. OTA is done via BLE, not WiFi.
Real coexistence validation happens during Phase 8 with actual game traffic.

---

## Phase 8: Integration (Requires Dev PCB)
**Status:** Blocked - waiting for PCB
**Goal:** All drivers working together on custom hardware

- [ ] Flash to dev PCB
- [ ] Validate all GPIO mappings
- [ ] Calibrate touch thresholds
- [ ] Multi-pod synchronization test

---

## Pin Mapping Reference

**Single source of truth:** [`docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md)

Quick reference for common platforms:

| Function | DevKitC-1 v1.1 | NFF Devboard | Production |
|----------|----------------|--------------|------------|
| LED Data | GPIO38 | GPIO48 | GPIO14 |
| I2C SDA | - | GPIO8 | GPIO8 |
| I2C SCL | - | GPIO9 | GPIO9 |
| I2S BCLK | - | GPIO12 | GPIO12 |

See the full pin reference for complete mappings and peripheral specifications.

---

## Success Criteria

| Phase | Pass Criteria |
|-------|---------------|
| 1 | `idf.py build` succeeds, binary < 1MB |
| 2 | DevKit LED cycles through R-G-B-W colors |
| 3 | Touch detection works on bare GPIO pins |
| 4 | Tasks run stable for 10+ minutes |
| 5 | Haptic effects play, IMU detects taps |
| 6 | Audio samples play clearly |
| 7 | RF stacks init, ESP-NOW ping works |
| 8 | All peripherals work on custom PCB |
