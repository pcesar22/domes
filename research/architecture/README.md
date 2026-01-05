# DOMES Firmware Architecture

## AI Agent Instructions

This folder contains modular architecture documentation. **Load only the files relevant to your current task** to optimize context usage.

### Quick Reference: Which File to Load

| Task | Load This File |
|------|----------------|
| First-time setup, hello world | `00-getting-started.md` |
| Creating new files, naming conventions | `01-project-structure.md` |
| Build commands, configurations | `02-build-system.md` |
| Writing a driver (LED, haptic, audio, etc.) | `03-driver-development.md` |
| ESP-NOW, BLE, protocols | `04-communication.md` |
| Game logic, state machine, drills | `05-game-engine.md` |
| Unit tests, mocks, CI | `06-testing.md` |
| Debugging, logging, crash analysis | `07-debugging.md` |
| OTA updates, partitions | `08-ota-updates.md` |
| Pin maps, NVS keys, error codes | `09-reference.md` |

---

## System Overview

**DOMES** (Distributed Open-source Motion & Exercise System) is a reaction training pod system.

### Hardware Summary
- **MCU:** ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB PSRAM)
- **LEDs:** 16x SK6812 RGBW in ring
- **Audio:** MAX98357A I2S amp + 23mm speaker
- **Haptic:** DRV2605L + LRA motor
- **Touch:** ESP32 capacitive + LIS2DW12 IMU fusion
- **Power:** 1200mAh LiPo, TP4056 charger
- **Connectivity:** ESP-NOW (pod-to-pod) + BLE (phone control)

### Tech Stack
| Aspect | Choice |
|--------|--------|
| Framework | ESP-IDF v5.x |
| RTOS | FreeRTOS (dual-core SMP) |
| Language | C++20 (app), C (drivers) |
| Build | CMake via `idf.py` |
| Testing | Unity + CMock |

---

## Architecture Layers

```
┌─────────────────────────────────────────────────┐
│              APPLICATION LAYER                   │
│  Game Engine, Drill Manager, State Machine      │
│  → See: 05-game-engine.md                       │
├─────────────────────────────────────────────────┤
│              SERVICE LAYER                       │
│  Feedback, Communication, Timing, Config        │
│  → See: 03-driver-development.md (services)     │
├─────────────────────────────────────────────────┤
│              DRIVER LAYER                        │
│  LED, Audio, Haptic, Touch, IMU, Power          │
│  → See: 03-driver-development.md                │
├─────────────────────────────────────────────────┤
│              PLATFORM LAYER                      │
│  FreeRTOS, ESP-IDF APIs, HAL                    │
│  → See: 09-reference.md                         │
└─────────────────────────────────────────────────┘
```

---

## Development Milestones

| Milestone | Description | Verification | Docs |
|-----------|-------------|--------------|------|
| M0 | Hello World boots | UART shows heartbeat | `00-getting-started.md` |
| M1 | Target compiles | `idf.py build` exits 0 | `02-build-system.md` |
| M2 | DevKit boots | `app_main` runs | `00-getting-started.md` |
| M3 | Debug works | GDB breakpoints hit | `07-debugging.md` |
| M4 | RF validated | WiFi + BLE coexist | `04-communication.md` |
| M5 | Unit tests pass | Host tests green | `06-testing.md` |
| M6 | Drivers work | All peripherals respond | `03-driver-development.md` |
| M7 | ESP-NOW latency | P95 < 2ms | `04-communication.md` |

---

## Critical Rules (Always Apply)

### DO
- Use ESP-IDF v5.x (never Arduino)
- Use `ESP_LOGx` macros for logging
- Use `esp_err_t` or `tl::expected` for errors
- Use ETL containers (`etl::vector`, `etl::string`)
- Create interfaces for all drivers (`IHapticDriver`, etc.)
- Pin protocol tasks to Core 0, app tasks to Core 1
- Check every `esp_err_t` return value

### DON'T
- Use `std::vector`, `std::string`, `std::map` (heap fragmentation)
- Use `<iostream>` (adds 200KB binary size)
- Allocate heap after initialization phase
- Use exceptions or RTTI (disabled)
- Ignore error returns

---

## File Index

| File | Purpose | Lines |
|------|---------|-------|
| `00-getting-started.md` | Setup, hello world, first flash | ~200 |
| `01-project-structure.md` | Directory layout, naming conventions | ~150 |
| `02-build-system.md` | Build configs, targets, sdkconfig | ~180 |
| `03-driver-development.md` | Interfaces, driver templates, DI | ~400 |
| `04-communication.md` | ESP-NOW, BLE, protocol messages | ~300 |
| `05-game-engine.md` | State machine, drills, timing | ~250 |
| `06-testing.md` | Unity, CMock, mocks, CI pipeline | ~300 |
| `07-debugging.md` | OpenOCD, GDB, logging, core dumps | ~200 |
| `08-ota-updates.md` | OTA flow, partitions, rollback | ~150 |
| `09-reference.md` | Pin maps, NVS schema, error codes | ~300 |

---

## Cross-References

- **Hardware specs:** `research/SYSTEM_ARCHITECTURE.md`
- **Coding standards:** `firmware/GUIDELINES.md`
- **AI guidelines:** `firmware/CLAUDE.md`
- **Development roadmap:** `research/DEVELOPMENT_ROADMAP.md`
- **AI recommendations:** `research/AI_DEVELOPMENT_RECOMMENDATIONS.md`

---

*Document Version: 2.0*
*Last Updated: 2026-01-05*
