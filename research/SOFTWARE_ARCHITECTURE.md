# DOMES Pod - Software Architecture Document

> **Note**: This document was the original comprehensive architecture spec. It has been
> split into focused documents under `research/architecture/`. The key decisions below
> remain authoritative.

---

## 1. EXECUTIVE SUMMARY

### 1.1 Key Decisions (TL;DR)

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **RTOS** | ESP-IDF (FreeRTOS) | Best ESP32 support, vendor-maintained, production-proven |
| **Language** | C++20 (with C for drivers) | Modern features, ESP-IDF native support, your experience |
| **Build System** | ESP-IDF + CMake | Industry standard for ESP32, excellent tooling |
| **Debug** | OpenOCD + GDB via USB-JTAG | Native USB on ESP32-S3, no extra hardware |
| **OTA** | ESP-IDF OTA with rollback | Battle-tested, secure, supports anti-rollback |

### 1.2 Why NOT Alternatives

| Alternative | Why Not |
|-------------|---------|
| **Zephyr** | Incomplete ESP32-S3 support, I2C slave issues, steep learning curve, less community examples |
| **Rust (esp-rs)** | Promising but still maturing; WiFi/BLE stack not as stable as ESP-IDF; team velocity risk |
| **Baremetal** | Reinventing the wheel; ESP32 peripherals (WiFi, BLE) require OS anyway |
| **Arduino** | Limited to C++11, less control, harder to optimize power |

---

## 2. DETAILED DOCUMENTATION INDEX

All detailed content has been moved to standalone documents:

| Topic | Authoritative Document |
|-------|----------------------|
| Project structure | `research/architecture/01-project-structure.md` |
| Build system | `research/architecture/02-build-system.md` |
| Driver development | `research/architecture/03-driver-development.md` |
| Communication (ESP-NOW, BLE) | `research/architecture/04-communication.md` |
| Game engine | `research/architecture/05-game-engine.md` |
| Testing | `research/architecture/06-testing.md` |
| Debugging & tracing | `research/architecture/07-debugging.md` |
| OTA updates | `research/architecture/08-ota-updates.md` |
| Hardware reference | `research/architecture/09-reference.md` |
| Host simulation | `research/architecture/10-host-simulation.md` |
| System modes | `research/architecture/11-system-modes.md` |
| Multi-pod orchestration | `research/architecture/12-multi-pod-orchestration.md` |
| Coding standards | `firmware/CLAUDE.md` |
| Hardware architecture | `research/SYSTEM_ARCHITECTURE.md` |
| Pin mappings | `docs/PIN_REFERENCE.md` |
| Milestone status | `firmware/MILESTONES.md` |
