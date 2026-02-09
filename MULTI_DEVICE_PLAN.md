# Multi-Device Support Overhaul

**Branch:** `claude/feat/multi-device-support`
**Goal:** Scale from single-device to two+ connected devices. Enable full ESP-NOW testing and all multi-pod features.
**Last audit:** 2026-02-08

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| :white_check_mark: | Code written (in worktree, uncommitted) |
| :construction: | Partially done |
| :x: | Not started |
| :new: | Gap found in fresh audit (2026-02-08) |

---

## Audit Summary

Every file below assumed **one ESP32 device** connected to the host. This document catalogs each assumption and the work needed to generalize. A fresh full-codebase audit on 2026-02-08 found 6 additional gaps not in the original plan.

---

## 1. Firmware: Device Identity & Naming

### 1.1 :white_check_mark: Hardcoded BLE Name — ALL pods advertise as "DOMES-Pod"

| File | Line(s) | Was |
|------|---------|-----|
| `firmware/domes/sdkconfig.defaults` | 74 | `CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME="DOMES-Pod"` |
| `firmware/domes/main/transport/bleOtaService.cpp` | 302 | `ble_svc_gap_device_name_set("DOMES-Pod")` |

**Done:** `bleOtaService.cpp` now reads `pod_id` from NVS. If set, names as `DOMES-Pod-01`. Otherwise falls back to BT MAC suffix: `DOMES-Pod-3A2B`.

### 1.2 :white_check_mark: Pod ID System (NVS read at boot)

| File | Line(s) | Was |
|------|---------|-----|
| `firmware/domes/main/infra/nvsConfig.hpp` | 31 | `kPodId = "pod_id"` key exists but unused |

**Done:** `main.cpp` now calls `readPodId()` at boot, logging the value. `domes-cli system set-pod-id <N>` implemented. `pod_id` included in `GetSystemInfoResponse` and `ListFeaturesResponse`.

### 1.3 Single NimBLE Connection Limit

| File | Line(s) | Current |
|------|---------|---------|
| `firmware/domes/sdkconfig.defaults` | 66 | `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` |

**No change needed.** Intentional for phone→master→ESP-NOW architecture. Documented.

### 1.4 :new: :white_check_mark: config.proto Device Identity Fields

| File | Issue |
|------|-------|
| `firmware/common/proto/config.proto` | No `pod_id` or `device_id` in any request/response message |

**Done:** Added `uint32 pod_id` to `GetSystemInfoResponse` and `ListFeaturesResponse`. Added `SetPodIdRequest`/`SetPodIdResponse` messages with `MSG_TYPE_SET_POD_ID_REQ = 0x36` and `MSG_TYPE_SET_POD_ID_RSP = 0x37`. Firmware handler reads/writes NVS. CLI parses pod_id from responses and has `system set-pod-id` command.

### 1.5 :new: :white_check_mark: GameEvent Source Pod ID

| File | Line(s) | Current |
|------|---------|---------|
| `firmware/domes/main/game/gameEngine.hpp` | 54–63 | `GameEvent { type, reactionTimeUs, padIndex }` — no device ID |

**Done:** Added `uint8_t podId` field to `GameEvent` struct. `GameEngine` reads pod_id from NVS at construction (guarded with `#ifdef CONFIG_IDF_TARGET_ESP32S3` for test builds). Events now include pod identity for multi-pod game aggregation.

### 1.6 :new: :white_check_mark: mDNS Service Advertisement

**Done:** Added `initMdns(uint8_t podId)` to `main.cpp`. Advertises `_domes._tcp.local.` on port 5000. Hostname: `domes-pod-{pod_id}` (or MAC fallback). TXT records include `pod_id` and `version`. Added `mdns` to CMakeLists.txt REQUIRES.

---

## 2. CLI Tool (`tools/domes-cli/`)

### 2.1 :white_check_mark: CLI Arguments Accept Only One Target

**Done:** `--port`, `--wifi`, `--ble` are now `Vec<String>`. Added `--target Vec<String>` and `--all bool`.

### 2.2 :white_check_mark: Single Transport Instance

**Done:** New `device.rs` module provides `DeviceConnection` struct and `resolve_devices()` function that returns `Vec<DeviceConnection>`.

### 2.3 :white_check_mark: All Commands Hardwired to Single Device

**Done:** `main.rs` now loops over all resolved devices for each command, printing `[pod-name]` prefixes when multiple devices are targeted.

### 2.4 :white_check_mark: BLE Scan → Connect-All

**Done:** Added `--connect-all-ble` flag. Scans BLE, filters for `DOMES-Pod-` prefix, adds MAC addresses to `cli.ble` Vec for automatic connection to all discovered pods.

### 2.5 :white_check_mark: Multi-Device OTA

**Done:** OTA loops sequentially over multiple devices with per-device progress. Partial failures are reported (which succeeded/failed). Sufficient for 2 devices.

### 2.6 :white_check_mark: Device Registry

**Done:** `~/.domes/devices.toml` support implemented in `device.rs`. Supports `[devices.pod1]` sections with `transport` and `address` fields.

### 2.7 :white_check_mark: `devices` Subcommand

**Done:** `devices list`, `devices add`, `devices remove`, `devices scan` subcommands added to CLI.

---

## 3. CI/CD (`.github/workflows/firmware-hw-test.yml`)

### 3.1 :white_check_mark: Hardcoded Single Port

**Done:** Workflow updated with multi-port support via `SERIAL_PORTS` env var.

### 3.2 :white_check_mark: Single-Device Test Steps

**Done:** Test steps updated to iterate over devices.

### 3.3 :construction: Sequential Concurrency Group

**Done in workflow file**, but needs testing with actual multi-device runner.

### 3.4 :x: ESP-NOW CI Test

**Blocked by:** ESP-NOW firmware implementation (section 7).

**Task:** New test step after both devices are flashed:
1. Flash pod-1 and pod-2
2. Assign pod IDs
3. Trigger ESP-NOW peer discovery
4. Verify pods see each other
5. Send a test message pod-1 → pod-2, verify receipt

---

## 4. Scripts & Tools

### 4.1 :white_check_mark: `tools/trace/trace_dump.py`

**Done:** Updated to accept `--ports` (comma-separated).

### 4.2 :white_check_mark: `.claude/skills/esp32-firmware/scripts/flash_and_verify.sh`

**Done:** Updated to accept multiple ports.

### 4.3 :white_check_mark: `.claude/skills/esp32-firmware/scripts/monitor_serial.py`

**Done:** Updated to accept multiple ports with colored, labeled output.

---

## 5. Claude Skills & Commands

### 5.1 :white_check_mark: `/flash` Skill

**Done:** Updated `.claude/commands/flash.md` with multi-device support.

### 5.2 :white_check_mark: `esp32-firmware` Skill

**Done:** Updated `.claude/skills/esp32-firmware/SKILL.md` with multi-device workflows section.

### 5.3 :white_check_mark: `debug-esp32` Skill

**Done:** Updated `.claude/skills/debug-esp32/SKILL.md` with multi-device debugging section.

---

## 6. Documentation

### 6.1 :white_check_mark: `CLAUDE.md`

**Done:** Multi-device testing section added. Examples updated with `--all`, `--target`, multi-port syntax.

### 6.2 :white_check_mark: `DEVELOPER_QUICKSTART.md`

**Done:** "Setting Up Multiple Pods" section added.

### 6.3 :white_check_mark: `README.md`

**Done:** Added "Multi-Pod Operations" section with device scan, registration, set-pod-id, connect-all-ble, and multi-OTA examples.

### 6.4 :white_check_mark: `tools/domes-cli/README.md`

**Done:** Multi-device examples and `--target` / `--all` flags documented.

### 6.5 :white_check_mark: `tools/README.md`

**Done:** Updated with multi-device examples.

### 6.6 :white_check_mark: Architecture Docs

| File | Status |
|------|--------|
| `research/architecture/06-testing.md` | :white_check_mark: Added "Multi-Device Testing" section |
| `research/architecture/07-debugging.md` | :white_check_mark: Added "Multi-Device Debugging" section |
| `research/architecture/08-ota-updates.md` | :white_check_mark: Added "Multi-Device OTA" section |
| `research/architecture/10-host-simulation.md` | :white_check_mark: Added "Multi-Device Simulation Support" section |
| `research/architecture/11-system-modes.md` | :white_check_mark: Replaced hardcoded `/dev/ttyACM0`, added "Multi-Device Mode Queries" |
| `research/architecture/12-multi-pod-orchestration.md` | :white_check_mark: Already multi-pod by design |

### 6.7 :new: :white_check_mark: `.claude/PLATFORM.md`

**Done:** Fully rewritten with multi-device setup section: udev rules, device registry, pod identity, discovery, CI multi-device setup.

### 6.8 :new: :white_check_mark: `firmware/CLAUDE.md`

**Done:** Expanded multi-device architecture section explaining per-pod identity (NVS pod_id, BLE naming), mDNS, GameEvent tagging, and what NOT to change.

---

## 7. Firmware: ESP-NOW (Not Yet Implemented)

| File | Current |
|------|---------|
| `firmware/common/proto/config.proto:68` | `FEATURE_ESP_NOW = 4` defined but not implemented |
| `research/architecture/04-communication.md` | Full ESP-NOW protocol spec exists |
| `research/architecture/12-multi-pod-orchestration.md` | Multi-pod orchestration architecture exists |

**Task (large, separate feature):** Implement ESP-NOW per architecture docs:
1. `CommService` singleton — send/recv ESP-NOW messages
2. Peer discovery via broadcast beacons
3. Peer roster management (add/remove MACs)
4. Message protocol: `ARM_TOUCH`, `SET_COLOR`, `TOUCH_EVENT`, etc.
5. Master/slave role assignment at runtime
6. `DrillInterpreter` on master, `PodCommandHandler` on slave

**Note:** This is a large feature tracked separately. The tooling/CI/docs changes in this plan enable _testing_ it once implemented.

---

## 8. :new: Infrastructure: Stable Device Naming

### 8.1 :white_check_mark: udev Rules for Consistent Serial Ports

**Done:** Created `tools/udev/99-domes-pods.rules` matching ESP32-S3 VID/PID (303a:1001, 303a:1002). Creates `/dev/domes-pod-usb*` symlinks. Documented in PLATFORM.md. `domes-cli devices scan` checks for symlinks.

### 8.2 :white_check_mark: Multi-Device Serial Port Auto-Detection

**Done:** `domes-cli devices scan` now:
- Enumerates `/dev/ttyACM*` and `/dev/domes-pod-*` ports
- Probes each with `system_info` for DOMES identity
- Shows pod_id, firmware version, mode for discovered devices
- BLE scan filters for `DOMES-Pod-*` prefix
- Reports device table with transport, address, and identity

---

## Implementation Priority (Updated)

### Phase 1: Firmware Identity (enables distinguishing devices)
- [x] 1.1 Dynamic BLE name from pod_id/MAC
- [x] 1.2 Pod ID read at boot + `set-pod-id` command
- [x] 1.4 config.proto `pod_id` fields in responses
- [x] 1.5 GameEvent `podId` field

### Phase 2: CLI Multi-Device Core (enables controlling both devices)
- [x] 2.1 Multi-value `--port` / `--wifi` / `--ble` args
- [x] 2.2 Transport pool (`DeviceConnection` list)
- [x] 2.3 Command dispatch loop with per-device output
- [x] 2.6 Device registry (`~/.domes/devices.toml`)
- [x] 2.7 `devices` subcommand (scan, list, add, remove)

### Phase 3: CLI Commands (each command works across devices)
- [x] 2.4 BLE scan → connect-all (`--connect-all-ble`)
- [x] 2.5 Multi-device OTA with per-device progress

### Phase 4: CI/CD (automated multi-device testing)
- [x] 3.1 Multi-port workflow inputs
- [x] 3.2 Loop-based test steps
- [x] 3.3 Per-device concurrency groups (in workflow, needs real testing)
- [ ] 3.4 ESP-NOW CI test (blocked by ESP-NOW implementation)

### Phase 5: Scripts & Skills (developer workflow)
- [x] 4.1 trace_dump.py multi-port
- [x] 4.2 flash_and_verify.sh multi-port
- [x] 4.3 monitor_serial.py multi-port with colored output
- [x] 5.1–5.3 Skill updates (/flash, esp32-firmware, debug-esp32)

### Phase 6: Documentation (everything updated)
- [x] 6.1 CLAUDE.md
- [x] 6.2 DEVELOPER_QUICKSTART.md
- [x] 6.3 README.md
- [x] 6.4 tools/domes-cli/README.md
- [x] 6.5 tools/README.md
- [x] 6.6 Architecture docs (06, 07, 08, 10, 11)
- [x] 6.7 PLATFORM.md
- [x] 6.8 firmware/CLAUDE.md (expanded multi-device section)

### Phase 7: Infrastructure (stable multi-device setup) :new:
- [x] 8.1 udev rules for stable serial port naming
- [x] 8.2 Auto-detection in `domes-cli devices scan`
- [x] 1.6 mDNS service advertisement

---

## What's NOT Changing (Already Multi-Device Safe)

- **WiFi per-pod**: Each ESP32 gets its own IP — already fine
- **TCP config server**: Port 5000 per-IP — already fine
- **FeatureManager**: Per-device singleton — already fine
- **TraceRecorder**: Per-device singleton — already fine
- **GameEngine**: Per-pod design is intentional per architecture
- **ModeManager**: Per-pod singleton — correct; cross-pod mode queries happen via host CLI, not firmware-to-firmware (until ESP-NOW is implemented)
- **Transport trait**: Single-connection trait is correct; multi-device is handled at the CLI dispatch layer
- **ESP-NOW protocol design**: Architecture docs 04 and 12 already describe multi-pod — implementation is tracked separately

---

## Files Changed (Uncommitted in Worktree)

### 35 files changed, +1764 / -343 lines

| File | What changed |
|------|-------------|
| `.claude/PLATFORM.md` | Rewritten with multi-device setup section |
| `.claude/commands/flash.md` | Multi-port support, `--all` flag |
| `.claude/skills/debug-esp32/SKILL.md` | Multi-device debugging section |
| `.claude/skills/esp32-firmware/SKILL.md` | Multi-device workflows section |
| `.claude/skills/esp32-firmware/scripts/flash_and_verify.sh` | Multi-port loop |
| `.claude/skills/esp32-firmware/scripts/monitor_serial.py` | Multi-port colored output |
| `.github/workflows/firmware-hw-test.yml` | Multi-port env, loop-based steps |
| `CLAUDE.md` | Multi-device testing section, updated examples |
| `DEVELOPER_QUICKSTART.md` | "Setting Up Multiple Pods" section |
| `README.md` | Added "Multi-Pod Operations" section |
| `firmware/CLAUDE.md` | Expanded multi-device architecture section |
| `firmware/common/proto/config.options` | Comments for new messages |
| `firmware/common/proto/config.proto` | pod_id fields, SetPodId messages, new msg types |
| `firmware/domes/main/CMakeLists.txt` | Added `mdns` to REQUIRES |
| `firmware/domes/main/config/configCommandHandler.cpp` | handleSetPodId, pod_id in responses |
| `firmware/domes/main/config/configCommandHandler.hpp` | handleSetPodId declaration |
| `firmware/domes/main/config/configProtocol.hpp` | kSetPodIdReq/Rsp enum values |
| `firmware/domes/main/game/gameEngine.cpp` | NVS pod_id read, event tagging |
| `firmware/domes/main/game/gameEngine.hpp` | podId field in GameEvent + GameEngine |
| `firmware/domes/main/main.cpp` | initMdns(), readPodId() |
| `firmware/domes/main/transport/bleOtaService.cpp` | Dynamic BLE name from pod_id/MAC |
| `firmware/domes/sdkconfig.defaults` | Updated BLE name comment |
| `research/architecture/06-testing.md` | Multi-Device Testing section |
| `research/architecture/07-debugging.md` | Multi-Device Debugging section |
| `research/architecture/08-ota-updates.md` | Multi-Device OTA section |
| `research/architecture/10-host-simulation.md` | Multi-Device Simulation Support section |
| `research/architecture/11-system-modes.md` | Multi-device examples, mode queries |
| `tools/README.md` | Multi-device examples |
| `tools/domes-cli/CLAUDE.md` | Multi-device testing section |
| `tools/domes-cli/README.md` | Multi-device docs |
| `tools/domes-cli/src/commands/mod.rs` | system_set_pod_id export |
| `tools/domes-cli/src/commands/system.rs` | system_set_pod_id function |
| `tools/domes-cli/src/main.rs` | Vec args, connect-all-ble, set-pod-id, scan probing |
| `tools/domes-cli/src/protocol/mod.rs` | SetPodId serialize/parse, pod_id in system info |
| `tools/trace/trace_dump.py` | `--ports` support |

### New files (2)
| File | What |
|------|------|
| `tools/domes-cli/src/device.rs` | Device registry, connection resolver |
| `tools/udev/99-domes-pods.rules` | ESP32-S3 udev rules for stable symlinks |

---

## Remaining Work Summary

| Category | Done | Remaining | Notes |
|----------|------|-----------|-------|
| Firmware identity | 6/6 | 0 | All done |
| CLI multi-device | 7/7 | 0 | All done |
| CI/CD | 3/4 | 1 | ESP-NOW CI test blocked by ESP-NOW impl |
| Scripts & skills | 6/6 | 0 | All done |
| Documentation | 10/10 | 0 | All done |
| Infrastructure | 3/3 | 0 | All done |

**Total: 35/36 items complete (~97%)**

The only remaining item (3.4 ESP-NOW CI test) is blocked by the ESP-NOW firmware implementation, which is a separate large feature tracked independently.
