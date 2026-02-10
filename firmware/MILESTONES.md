# DOMES Firmware Development Milestones

## Current Status Summary

**Last Updated:** 2026-02-09

| Category | Status |
|----------|--------|
| **Implemented & Tested** | LED driver, FreeRTOS infrastructure, NVS config, watchdog, logging, runtime config protocol, OTA system, performance tracing, WiFi, TCP config, BLE OTA, game engine, ESP-NOW service, multi-pod simulation |
| **Implemented (Hardware Verified)** | WiFi manager, TCP config server, BLE OTA service, ESP-NOW 2-pod drill |
| **Implemented (Needs NFF Board)** | Touch driver, IMU service (tap detection), LED ring (16x RGBW) |
| **Not Started** | Audio driver, haptic driver, observability tooling (M6.5) |
| **Blocking** | NFF devboard assembly (enables I2C/audio/full LED ring testing) |

### What Actually Works Right Now

On **ESP32-S3-DevKitC-1** (2 pods):
- ✅ Firmware boots and runs stable
- ✅ LED color cycling + solid color via CLI
- ✅ USB-CDC serial communication
- ✅ Runtime config protocol (list/set features, system info)
- ✅ Serial OTA firmware updates
- ✅ BLE OTA service (NimBLE GATT)
- ✅ Performance tracing with Perfetto export
- ✅ NVS persistence (boot counter, settings, pod_id)
- ✅ WiFi connection (if credentials configured)
- ✅ TCP config server (if WiFi enabled)
- ✅ ESP-NOW 2-pod discovery + ping-pong + role assignment
- ✅ ESP-NOW drill orchestration (10-round MASTER/SLAVE game loop)
- ✅ Game engine FSM (arm → touch/timeout → feedback)
- ✅ IMU service (accelerometer + tap detection)
- ✅ Touch service (capacitive 4-pad sensing)
- ✅ 195 unit tests passing (including 25 multi-pod sim tests)

On **NFF Development Board** (pending assembly):
- 🔲 16x RGBW LED ring (driver ready, needs hardware)
- 🔲 LIS2DW12 accelerometer (driver ready, needs I2C bus)
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
**Status:** ✅ Complete (February 2026)
**Goal:** ESP-NOW and BLE initialization and basic functionality

- [x] WiFi stack init for ESP-NOW (verify `wifi:mode : sta` in logs)
- [x] ESP-NOW point-to-point test (2-pod ping-pong, 10/10 success)
- [x] ESP-NOW broadcast (beacons for discovery)
- [x] ESP-NOW unicast (ping/pong, game commands)
- [x] ESP-NOW game service (discovery → role assignment → drill loop)
- [x] BLE stack init (verify `NimBLE: GAP` in logs)
- [x] BLE GATT server setup (config protocol over BLE)
- [x] BLE OTA service (NimBLE, works with domes-cli --ble)

**Hardware Verified:** 2x ESP32-S3 DevKits, both flashed and running drills

**ESP-NOW bugs fixed (PR #63):** Counting semaphore, WiFi channel pinning,
stale TX drain, atomic cross-core flags, unicast ping/pong, slave heartbeat timeout.

---

## Phase 7.5: Observability & Diagnostics
**Status:** Not Started
**Goal:** Tooling to debug complex multi-pod issues systematically

**Tier 1 (Immediate):**
- [ ] Runtime diagnostics: `system health` (heap, stack, uptime) + `espnow status` (peers, channel, stats)
- [ ] Multi-device trace correlation tool (align N trace files → unified Perfetto JSON)
- [ ] ESP-NOW latency benchmark: `espnow bench` CLI (P50/P95/P99 over 1000 pings)

**Tier 2 (Short-term):**
- [ ] Synchronization instrumentation (TRACE_MUTEX macros, semaphore contention)
- [ ] Protocol sniffer (`domes-cli sniff` for frame capture + decode)
- [ ] CI trace collection (dump after hw-test, upload artifact)
- [ ] Multi-device CI (2-pod flash + ESP-NOW verification)

**Tier 3 (Medium-term):**
- [ ] Crash dump handler (panic → NVS → CLI retrieval + decode)
- [ ] Live trace streaming (WiFi/TCP to host, real-time Perfetto)
- [ ] Memory profiler (heap by task, leak detection)

**Motivation:** 10 bugs in ESP-NOW bringup were found via manual code review and
log reading. These tools would have caught most of them faster.

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
