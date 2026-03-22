# DOMES Development Roadmap
## Dependency-Based Execution Plan

---

## EXECUTIVE SUMMARY

| Aspect | Decision |
|--------|----------|
| **Strategy** | Parallel dev: Claude writes firmware while you design PCB |
| **Dev Boards** | Required for system validation + ESP-NOW/RF testing |
| **Breadboarding** | Skip - go straight to PCB |
| **Testing** | System validation on DevKit FIRST, then POSIX unit tests |
| **CI** | Host unit tests + target compile (every PR) + DevKit smoke (nightly) |

## Hardware Terminology

| Term | Refers To | Description |
|------|-----------|-------------|
| **Dev Boards** | ESP32-S3-DevKitC-1 | Off-the-shelf boards for initial firmware development |
| **Dev PCB / NFF Devboard** | [`hardware/nff-devboard/`](../hardware/nff-devboard/) | Custom sensor board with all peripherals (LEDs, IMU, haptics, audio) |
| **Form-Factor PCB** | Production PCB | Final integrated design for enclosed pods |

---

## MILESTONE SEQUENCE

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         MILESTONE SEQUENCE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   M1: Target Compiles                                                        │
│         │                                                                    │
│         ▼                                                                    │
│   M2: DevKit Boots ─────────────────┐                                        │
│         │                           │                                        │
│         ▼                           │ System Firmware                        │
│   M3: Debug Infrastructure          │ Validation                             │
│         │                           │ (MUST pass before                      │
│         ▼                           │  investing in unit tests)              │
│   M4: RF Stacks Init ───────────────┘                                        │
│         │                                                                    │
│         ▼                                                                    │
│   M5: Unit Tests Pass (host)                                                 │
│         │                                                                    │
│         ▼                                                                    │
│   M6: ESP-NOW Latency Validated                                              │
│         │                                                                    │
│         ▼                                                                    │
│   M6.5: Observability & Diagnostics                                          │
│         │                                                                    │
│         ▼                                                                    │
│   M7: Dev PCB Works                                                          │
│         │                                                                    │
│         ▼                                                                    │
│   M8: 6-Pod Demo                                                             │
│         │                                                                    │
│         ▼                                                                    │
│   M9: Demo Ready                                                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Rationale for Sequence:**
1. Debug infrastructure is foundational - every future bug depends on it
2. RF stack *init* is low-risk on ESP32-S3 - coexistence issues only manifest under sustained concurrent load
3. Unit tests are more valuable when target is proven to work
4. Real RF coexistence validation happens during integration (M7/M8) with actual game traffic

---

## DEPENDENCY GRAPH

```mermaid
graph TD
    subgraph "Track A: Hardware (You)"
        A1[Order Dev Boards<br/>3x ESP32-S3]
        A2[Order PCB Components<br/>Full BOM]
        A3[Design Schematic]
        A5[Design PCB Layout]
        A6[Order Dev PCB]
        A7[Assemble Dev Units<br/>6x pods]
        A8[Design Form-Factor PCB]
        A9[Order Form-Factor PCB]
        A10[Assemble Final Units]
    end

    subgraph "Track B: System Validation (Claude + DevKit)"
        B1[Set Up ESP-IDF Project]
        B2[Flash Hello World]
        B3[Validate UART Output]
        B4[Validate GDB Debugging]
        B5[Validate Core Dump]
        B6[Init WiFi Stack]
        B7[Init BLE Stack]
        B9[ESP-NOW Latency Test]
    end

    subgraph "Track C: Unit Tests (Claude + Host)"
        C1[Write Driver Interfaces]
        C2[Set Up CMock Framework]
        C3[Write Mock Drivers]
        C4[Write Unit Tests]
        C5[Set Up CI Pipeline]
    end

    subgraph "Track D: Drivers (Claude)"
        D1[LED Driver - SK6812]
        D2[Audio Driver - MAX98357A]
        D3[Haptic Driver - DRV2605L]
        D4[Touch Driver]
        D5[IMU Driver - LIS2DW12]
        D6[Power Driver]
    end

    subgraph "Track E: Application (Claude)"
        E1[ESP-NOW Service]
        E2[BLE Service]
        E3[Protocol Layer]
        E4[Game Engine]
        E5[Feedback Service]
        E6[OTA System]
        E7[Smoke Test Suite]
    end

    subgraph "Track F: Integration (Claude + Custom PCB)"
        F1[Flash to Dev PCB]
        F2[Debug GPIO/Timing]
        F3[Calibrate Sensors]
        F4[Multi-Pod Test]
        F5[Flash Form-Factor]
        F6[Final Validation]
    end

    subgraph "Track G: Observability (Claude)"
        G1[Runtime Diagnostics<br/>system health + espnow status]
        G2[Multi-Device Trace<br/>Correlation Tool]
        G3[ESP-NOW Latency<br/>Benchmark CLI]
        G4[Sync Instrumentation<br/>mutex/semaphore tracing]
        G5[Protocol Sniffer]
        G6[CI Trace Collection]
        G7[Multi-Device CI]
        G8[Crash Dump Handler]
    end

    subgraph "Milestones"
        M1((M1: Target<br/>Compiles))
        M2((M2: DevKit<br/>Boots))
        M3((M3: Debug<br/>Infra))
        M4((M4: RF<br/>Init))
        M5((M5: Unit<br/>Tests))
        M6((M6: ESP-NOW<br/>Latency))
        M6_5((M6.5: Observability<br/>& Diagnostics))
        M7((M7: Dev PCB<br/>Works))
        M8((M8: 6-Pod<br/>Demo))
        M9((M9: Demo<br/>Ready))
    end

    %% System Validation sequence (critical path)
    B1 --> M1
    M1 --> B2
    A1 --> B2
    B2 --> B3
    B3 --> M2
    M2 --> B4
    B4 --> B5
    B5 --> M3
    M3 --> B6
    B6 --> B7
    B7 --> M4

    %% Unit Tests (after system validation)
    M4 --> C1
    C1 --> C2
    C2 --> C3
    C3 --> C4
    C4 --> M5
    M5 --> C5

    %% Drivers (after unit test framework)
    C1 --> D1
    C1 --> D2
    C1 --> D3
    C1 --> D4
    C1 --> D5
    C1 --> D6

    %% ESP-NOW latency (after RF validated)
    M4 --> B9
    B9 --> M6

    %% Observability (after ESP-NOW working)
    M6 --> G1
    M6 --> G2
    M6 --> G3
    G1 --> G4
    G1 --> G5
    G3 --> G6
    G3 --> G7
    G4 --> G8
    G1 --> M6_5
    G2 --> M6_5
    G3 --> M6_5
    M6_5 --> F4

    %% Application layer
    M5 --> E1
    M6 --> E1
    D1 --> E1
    E1 --> E2
    E2 --> E3
    D4 --> E4
    D5 --> E4
    E3 --> E4
    D2 --> E5
    D3 --> E5
    E4 --> E5
    E5 --> E6
    E6 --> E7

    %% Hardware track
    A3 --> A5
    A5 --> A6
    A2 --> A7
    A6 --> A7

    %% Integration
    A7 --> F1
    E7 --> F1
    F1 --> F2
    F2 --> F3
    F3 --> F4
    F4 --> M7
    M7 --> M8
    M8 --> A8
    A8 --> A9
    A9 --> A10
    A10 --> F5
    F5 --> F6
    F6 --> M9
```

---

## MILESTONE DEFINITIONS

### Status Summary

| Milestone | Description | Status |
|-----------|-------------|--------|
| M1 | Target Compiles | COMPLETE |
| M2 | DevKit Boots | COMPLETE |
| M3 | Debug Infrastructure | COMPLETE |
| M4 | RF Stacks Init | COMPLETE |
| M5 | Unit Tests Pass | COMPLETE (195 tests) |
| M6 | ESP-NOW Latency Validated | IN PROGRESS (2-pod drill works, latency needs formal bench) |
| M6.5 | Observability & Diagnostics | Not Started |
| M7 | Dev PCB Works | Not Started |
| M8 | 6-Pod Demo | Not Started |
| M9 | Demo Ready | Not Started |

---

### M1: Target Compiles

**Status:** COMPLETE (January 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| ESP-IDF installed | `idf.py --version` | Version 5.x | v5.2.2 |
| Project scaffolding | Directory structure | Per SOFTWARE_ARCHITECTURE.md | Done |
| Cross-compile | `idf.py build` | Exit code 0, no errors | Pass |
| Binary size | Check .bin | < 4MB (fits OTA partition) | ~350KB |

**Owner:** Claude
**Depends On:** Nothing
**Blocks:** M2

---

### M2: DevKit Boots

**Status:** COMPLETE (January 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| Flash succeeds | `idf.py flash` | Exit code 0 | Pass |
| Boot completes | UART monitor | `app_main` log within 5s | Pass |
| UART parseable | Log regex | Timestamps + levels parse correctly | Pass |
| No crash loops | Monitor 60s | No repeated boot messages | Pass |
| Heap OK | `heap_caps_get_info()` | Free heap > 100KB | Pass |
| PSRAM OK | Log check | `Octal SPI RAM enabled` | Pass |

**Owner:** Claude (requires DevKit from You)
**Depends On:** M1, A1 (dev boards ordered/received)
**Blocks:** M3

---

### M3: Debug Infrastructure

**Status:** COMPLETE (January 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| OpenOCD connects | `openocd -f board/esp32s3-builtin.cfg` | "Listening on port 3333" | Pass |
| GDB connects | `target remote :3333` | Connected, no errors | Pass |
| Breakpoint works | Set BP on `app_main` | Hits breakpoint | Pass |
| Step works | `next`, `step` | Executes, shows source | Pass |
| Variable inspect | `print variable` | Shows correct value | Pass |
| Core dump to flash | Trigger panic via code | Core dump saved | Pass |
| Core dump decode | `idf.py coredump-info` | Stack trace readable | Pass |
| NVS config storage | Boot counter persistence | Increments across reboots | Pass |
| Task Watchdog (TWDT) | 30-second stability test | No watchdog triggers | Pass |
| TaskManager | LED demo task via TaskManager | Task runs with watchdog | Pass |

**Owner:** Claude
**Depends On:** M2
**Blocks:** M4

---

### M4: RF Stacks Init

**Status:** COMPLETE (January 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| WiFi init | UART log | `wifi:mode : sta`, no errors | Pass |
| BLE init | UART log | `NimBLE: GAP` init, no errors | Pass |
| BLE advertise | nRF Connect app | Device visible | Pass (DOMES-Pod-XX) |
| BLE GATT service | domes-cli --ble | Config protocol works | Pass |
| ESP-NOW init | UART log | Transport initialized | Pass |
| Memory stable | heap check | No leak after WiFi+BLE init | Pass |

**Owner:** Claude
**Depends On:** M3
**Blocks:** M5, M6

**Note:** WiFi + BLE + ESP-NOW all coexist. WiFi is STA mode pinned to channel 1
for ESP-NOW. BLE OTA service runs concurrently. Coexistence causes occasional
~80ms latency spikes on ESP-NOW (BLE scan interference) but is functionally stable.

---

### M5: Unit Tests Pass

**Status:** COMPLETE (January 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| Test framework | Google Test + GMock | Compiles with mocks | Pass |
| Host build works | CMake (native Linux) | Exit code 0 | Pass |
| Tests run | `./build/test_app` | All tests execute | Pass |
| All pass | Test output | 0 failures | 195/195 pass |
| CI pipeline | GitHub Actions | Runs on every PR | Pass |
| Multi-pod sim | SimOrchestrator | Deterministic sim tests | 25 sim tests pass |

**Owner:** Claude
**Depends On:** M4
**Blocks:** Application development

**Test suites:** Frame codec, OTA protocol, config protocol, feature manager,
game engine, mode manager, trace recorder, multi-pod simulation, drill orchestration.

---

### M6: ESP-NOW Latency Validated

**Status:** IN PROGRESS (February 2026)

| Check | Method | Pass Criteria | Result |
|-------|--------|---------------|--------|
| ESP-NOW init | UART log | `ESPNOW: initialized` | Pass |
| Peer discovery | 2 DevKits | Devices find each other | Pass (beacons + addPeer) |
| Ping-pong test | 10 iterations | All packets received | Pass (10/10) |
| Full game drill | 2 pods, 10 rounds | All rounds complete | Pass (MASTER/SLAVE, SET_COLOR, ARM, TIMEOUT_EVENT, STOP_ALL) |
| Auto-restart | After drill | Discovery restarts | Pass |
| Latency typical | Manual measurement | < 10ms | Pass (3-8ms typical) |
| Latency P50 | Formal benchmark | < 1ms | **Not yet measured (needs bench tool)** |
| Latency P95 | Formal benchmark | < 2ms | **Not yet measured** |
| Latency P99 | Formal benchmark | < 5ms | **Not yet measured** |
| Coex impact | BLE active | Latency still < 5ms P99 | Spikes to ~80-99ms observed (BLE coex) |

**Owner:** Claude (requires 2+ DevKits)
**Depends On:** M4
**Blocks:** M6.5, E1 (ESP-NOW Service)

**What works:** Full 3-phase lifecycle (discovery → role assignment → game loop) with
10-round drills completing end-to-end on 2 physical pods. 10 bugs fixed during bringup
(PR #63). See `firmware/domes/main/services/espNowService.cpp`.

**Remaining:** Formal latency benchmark tool (`espnow bench` CLI command) to measure
P50/P95/P99 over 1000+ iterations. BLE coexistence tuning to reduce ~80ms spikes.

---

### M6.5: Observability & Diagnostics

**Status:** Not Started

**Goal:** Fill observability gaps exposed during ESP-NOW bringup (10 bugs required
manual log reading and code review to find). Build the tooling to make complex
multi-pod, multi-transport debugging fast and systematic.

**Depends On:** M6 (ESP-NOW working end-to-end)
**Blocks:** M7 (critical for debugging 6-pod integration on custom PCB)

#### Tier 1: Immediate (highest value, lowest effort)

| Task | Description | Location | Priority |
|------|-------------|----------|----------|
| **G1: Runtime diagnostics command** | `system health` → heap, stack high-water, uptime, reset reason. `espnow status` → peer table, channel, TX/RX counts, drops | Firmware: new proto messages + handler. CLI: new commands | HIGH |
| **G2: Multi-device trace correlation** | Align N per-device trace dumps to shared timeline using sync markers (beacon timestamps). Output unified Perfetto JSON with pod-tagged tracks | `tools/trace/correlate_traces.py` | HIGH |
| **G3: ESP-NOW latency benchmark** | `domes-cli espnow bench` → run N pings, collect RTTs, output min/max/mean/P50/P95/P99 histogram. Replaces manual log reading | CLI: new command. Firmware: respond to bench pings | HIGH |

#### Tier 2: Short-term (high value, moderate effort)

| Task | Description | Location | Priority |
|------|-------------|----------|----------|
| **G4: Synchronization instrumentation** | TRACE_MUTEX_TAKE/GIVE macros that log blocking duration. Instrument FreeRTOS hooks for semaphore contention visibility | `trace/traceApi.hpp`, `trace/traceHooks.cpp` | MEDIUM |
| **G5: Protocol sniffer** | `domes-cli sniff` captures raw frames on serial/TCP/BLE, decodes message types, shows request/response latency | CLI: new command | MEDIUM |
| **G6: CI trace collection** | After hw-test flash+OTA, dump trace buffer and upload as artifact. Enables post-mortem CI debugging | `.github/workflows/firmware-hw-test.yml` | MEDIUM |
| **G7: Multi-device CI** | Extend hw-test to detect 2+ devices, flash both, verify ESP-NOW discovery + ping-pong completes | `.github/workflows/firmware-hw-test.yml` | MEDIUM |

#### Tier 3: Medium-term (important for 6-pod scale)

| Task | Description | Location | Priority |
|------|-------------|----------|----------|
| **G8: Crash dump handler** | On panic: save registers + backtrace to NVS. CLI retrieves with `system crash-dump`, decodes against ELF | Firmware: `services/crashService.hpp`. CLI: new command | MEDIUM |
| **G9: Live trace streaming** | Stream trace events over WiFi/TCP to host in real-time (not post-mortem). Enables "watch in Perfetto" during test | Firmware: `trace/traceStreamer.hpp` | LOW |
| **G10: Memory profiler** | Track heap allocations by task, report in `system stats`, detect leaks over time | Firmware: `infra/memProfiler.hpp` | LOW |
| **G11: Simulation-to-hardware diff** | Run sim scenario → export expected trace. Run on hardware → collect actual trace. Diff-view showing divergence | `tools/trace/sim_vs_hw_diff.py` | LOW |

#### Pass Criteria

| Check | Method | Pass Criteria |
|-------|--------|---------------|
| Runtime diagnostics | `domes-cli system health` | Returns heap, stack, uptime on both pods |
| Trace correlation | `correlate_traces.py` | Unified Perfetto JSON shows both pods on shared timeline |
| Latency bench | `domes-cli espnow bench -n 1000` | Outputs P50/P95/P99, all values measured |
| CI trace dump | hw-test workflow | Trace artifact uploaded after flash |
| Crash recovery | Trigger panic, reboot, `system crash-dump` | Backtrace decoded from NVS |

---

### M7: Dev PCB Works

| Check | Method | Pass Criteria |
|-------|--------|---------------|
| Flash succeeds | `idf.py flash` | Exit code 0 |
| All GPIOs correct | Smoke test | LEDs, audio, haptic respond |
| Touch works | Smoke test | Touch detected |
| IMU works | Smoke test | Acceleration reads |
| Power circuit | Multimeter | 3.3V stable, battery charges |
| Full smoke test | Built-in suite | All 8 tests pass |

**Owner:** Claude + You
**Depends On:** A7 (assembled PCB), E7 (smoke tests)
**Blocks:** M8

---

### M8: 6-Pod Demo

| Check | Method | Pass Criteria |
|-------|--------|---------------|
| 6 pods boot | UART | All 6 show `app_main` |
| All connect | ESP-NOW | Master sees 6 peers |
| Drill runs | Trigger drill | LEDs light in sequence |
| Touch response | Hit each pod | < 50ms visual feedback |
| Audio sync | Listen | No noticeable delay |
| 10 min soak | Run drills | No crashes, no desyncs |
| RF coexistence | BLE + ESP-NOW | No packet loss under game load |

**Owner:** Claude + You
**Depends On:** M7
**Blocks:** M9, A8 (form-factor design)

**Note:** This is where real RF coexistence validation happens - with actual game traffic
patterns (ESP-NOW mesh active, BLE connected to phone, sustained drill sequences).
If issues surface here, tune BLE scan params per Espressif guidelines.

---

### M9: Demo Ready

| Check | Method | Pass Criteria |
|-------|--------|---------------|
| Form-factor boots | Flash | Exit code 0 |
| Enclosure fits | Physical | PCB in shell |
| Battery life | Runtime test | > 10 hours standby |
| OTA works | Push update | New version boots |
| Demo script | Run through | Looks professional |

**Owner:** Claude + You
**Depends On:** A10 (final assembly)
**Blocks:** External demos

---

## CI PIPELINE STRATEGY

### Pipeline Tiers

| Tier | Trigger | Hardware | Tests | Duration |
|------|---------|----------|-------|----------|
| **Tier 1** | Every PR | None (GitHub-hosted) | Host unit tests | < 2 min |
| **Tier 2** | Every PR | None (GitHub-hosted) | Target compile check | < 5 min |
| **Tier 3** | Nightly | Self-hosted + DevKit | DevKit smoke test | < 10 min |
| **Tier 4** | Manual/Release | Self-hosted + DevKit | Full validation suite | < 30 min |

### Tier 1: Host Unit Tests (Every PR)

```yaml
# .github/workflows/unit-tests.yml
name: Unit Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.2
      - name: Build for Linux target
        run: |
          cd firmware
          idf.py --preview set-target linux
          idf.py build
      - name: Run tests
        run: ./firmware/build/test_app
      - name: Upload coverage
        uses: codecov/codecov-action@v3
```

### Tier 2: Target Compile (Every PR)

```yaml
# .github/workflows/build.yml
name: Build
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
      - name: Build for ESP32-S3
        run: |
          cd firmware
          idf.py set-target esp32s3
          idf.py build
      - name: Check binary size
        run: |
          SIZE=$(stat -c%s firmware/build/domes.bin)
          if [ $SIZE -gt 4194304 ]; then
            echo "Binary too large: $SIZE bytes (max 4MB)"
            exit 1
          fi
```

### Tier 3: DevKit Smoke Test (Nightly)

```yaml
# .github/workflows/devkit-smoke.yml
name: DevKit Smoke Test
on:
  schedule:
    - cron: '0 3 * * *'  # 3 AM daily
  workflow_dispatch:  # Manual trigger

jobs:
  smoke:
    runs-on: [self-hosted, devkit]
    steps:
      - uses: actions/checkout@v4
      - name: Flash firmware
        run: |
          cd firmware
          idf.py flash -p /dev/ttyUSB0
      - name: Capture UART (30s)
        run: |
          timeout 30 cat /dev/ttyUSB0 > uart_log.txt || true
      - name: Validate boot
        run: |
          grep -q "app_main" uart_log.txt || exit 1
          grep -q "wifi:mode" uart_log.txt || exit 1
          grep -q "NimBLE" uart_log.txt || exit 1
          ! grep -q "Guru Meditation" uart_log.txt || exit 1
      - name: GDB sanity check
        run: |
          # Start OpenOCD in background
          openocd -f board/esp32s3-builtin.cfg &
          sleep 2
          # Connect GDB and verify
          echo -e "target remote :3333\ninfo reg\nquit" | \
            xtensa-esp32s3-elf-gdb -batch firmware/build/domes.elf
          killall openocd
      - name: Upload logs
        uses: actions/upload-artifact@v3
        with:
          name: uart-logs
          path: uart_log.txt
```

### Self-Hosted Runner Setup

```bash
# On your CI machine with DevKit connected:

# 1. Install GitHub Actions runner
mkdir actions-runner && cd actions-runner
curl -o actions-runner-linux-x64.tar.gz -L https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64.tar.gz
tar xzf actions-runner-linux-x64.tar.gz
./config.sh --url https://github.com/your-org/domes --token YOUR_TOKEN --labels devkit

# 2. Grant USB access
sudo usermod -aG dialout $USER
sudo chmod 666 /dev/ttyUSB0

# 3. Start runner as service
sudo ./svc.sh install
sudo ./svc.sh start
```

---

## TASK BREAKDOWN BY OWNER

### Track A: Hardware (You)

| Task | Depends On | Blocks | Notes |
|------|------------|--------|-------|
| **A1: Order Dev Boards** (3x ESP32-S3-DevKitC) | - | M2 | Off-the-shelf boards |
| **A2: Order PCB Components** | - | A7 | BOM from `hardware/nff-devboard/production/bom.csv` |
| **A3: Design Schematic** | - | A5 | ✅ Complete - see `hardware/nff-devboard/docs/schematic.pdf` |
| **A5: Design PCB Layout** | A3 | A6 | ✅ Complete - NFF devboard |
| **A6: Order Dev PCB** | A5 | A7 | Upload `hardware/nff-devboard/production/gerbers/` to JLCPCB |
| **A7: Assemble Dev Units** | A2, A6 | M7 | NFF devboard + DevKit modules |
| **A8: Design Form-Factor PCB** | M8 | A9 | Production PCB (future) |
| **A9: Order Form-Factor PCB** | A8 | A10 | - |
| **A10: Assemble Final Units** | A9 | M9 | - |

### Track B: System Validation (Claude + DevKit)

| Task | Depends On | Blocks | Validation Method |
|------|------------|--------|-------------------|
| **B1: Set Up ESP-IDF Project** | - | M1 | `idf.py build` succeeds |
| **B2: Flash Hello World** | M1, A1 | B3 | Binary runs on DevKit |
| **B3: Validate UART Output** | B2 | M2 | Logs parse correctly |
| **B4: Validate GDB Debugging** | M2 | B5 | Breakpoints work |
| **B5: Validate Core Dump** | B4 | M3 | Panic decoded correctly |
| **B6: Init WiFi Stack** | M3 | B7 | `wifi:mode` in logs |
| **B7: Init BLE Stack** | B6 | M4 | `NimBLE` init OK |
| **B9: ESP-NOW Latency Test** | M4 | M6 | P95 < 2ms |

### Track C: Unit Tests (Claude + Host)

| Task | Depends On | Blocks |
|------|------------|--------|
| **C1: Write Driver Interfaces** | M4 | C2, D1-D6 |
| **C2: Set Up CMock Framework** | C1 | C3 |
| **C3: Write Mock Drivers** | C2 | C4 |
| **C4: Write Unit Tests** | C3 | M5 |
| **C5: Set Up CI Pipeline** | M5 | - |

### Track D: Drivers (Claude)

| Task | Depends On | Blocks |
|------|------------|--------|
| **D1: LED Driver** | C1 | E1 |
| **D2: Audio Driver** | C1 | E5 |
| **D3: Haptic Driver** | C1 | E5 |
| **D4: Touch Driver** | C1 | E4 |
| **D5: IMU Driver** | C1 | E4 |
| **D6: Power Driver** | C1 | E7 |

### Track E: Application (Claude)

| Task | Depends On | Blocks |
|------|------------|--------|
| **E1: ESP-NOW Service** | M5, M6, D1 | E2 |
| **E2: BLE Service** | E1 | E3 |
| **E3: Protocol Layer** | E2 | E4 |
| **E4: Game Engine** | D4, D5, E3 | E5 |
| **E5: Feedback Service** | D2, D3, E4 | E6 |
| **E6: OTA System** | E5 | E7 |
| **E7: Smoke Test Suite** | E6, D6 | M7 |

### Track F: Integration (Claude + Custom PCB)

| Task | Depends On | Blocks |
|------|------------|--------|
| **F1: Flash to Dev PCB** | A7, E7 | F2 |
| **F2: Debug GPIO/Timing** | F1 | F3 |
| **F3: Calibrate Sensors** | F2 | F4 |
| **F4: Multi-Pod Test** | F3, M6.5 | M7 |
| **F5: Flash Form-Factor** | A10, M8 | F6 |
| **F6: Final Validation** | F5 | M9 |

### Track G: Observability & Diagnostics (Claude)

| Task | Depends On | Blocks | Notes |
|------|------------|--------|-------|
| **G1: Runtime Diagnostics** | M6 | G4, G5, M6.5 | `system health` + `espnow status` CLI commands; new proto messages |
| **G2: Multi-Device Trace Correlation** | M6 | M6.5 | Align N trace files → unified Perfetto JSON with pod-tagged tracks |
| **G3: ESP-NOW Latency Benchmark** | M6 | G6, G7, M6.5 | `espnow bench` CLI: N pings → P50/P95/P99 histogram |
| **G4: Sync Instrumentation** | G1 | G8 | TRACE_MUTEX macros, FreeRTOS semaphore/mutex tracing |
| **G5: Protocol Sniffer** | G1 | - | `domes-cli sniff`: capture + decode frames, show latency |
| **G6: CI Trace Collection** | G3 | - | Dump trace buffer after hw-test, upload as artifact |
| **G7: Multi-Device CI** | G3 | - | 2-pod flash + ESP-NOW discovery + ping verification |
| **G8: Crash Dump Handler** | G4 | - | Panic → NVS, `system crash-dump` CLI retrieval + decode |

---

## CRITICAL PATH

```
A1 (Order DevKits) ──► B2 (Flash) ──► B3-B7 (System Validation) ──► M4
                                                                      │
┌─────────────────────────────────────────────────────────────────────┘
│
▼
C1-C4 (Unit Tests) ──► M5 ──► E1-E7 (Application) ──► F1 ◄── A7 (Assemble)
                                                        │
                                                        ▼
                                                   F2-F4 ──► M7 ──► M8
                                                                     │
                                                                     ▼
                                                              A8-A10 ──► M9
```

**Bottlenecks:**
1. A1 (Dev boards) - blocks all hardware validation
2. M4 (RF init) - gates unit test development (low risk, just needs hardware)
3. A7 (PCB assembly) - blocks integration

---

## PARALLEL EXECUTION

```
YOU                              CLAUDE
───                              ──────

A1: Order dev boards ──────────► [Wait for delivery]
A2: Order components                    │
A3: Design schematic                    │
        │                               │
        │                    [DevKit arrives]
        │                               │
        │                        B1: Project setup
        │                        B2: Flash hello world
        │                        B3: Validate UART
        │                        B4: Validate GDB
        │                        B5: Validate core dump
        │                        B6-B7: RF stacks init ► M4
        │                               │
        ▼                               ▼
A5: PCB Layout               C1-C4: Unit tests ────► M5
        │                               │
        ▼                               ▼
A6: Order PCB                D1-D6: Drivers
        │                    E1-E7: Application
        ▼                               │
[PCB fabrication]                       │
        │                               │
        ▼                               ▼
A7: Assemble ──────────────────► F1: Flash
                                 F2-F4: Integration
                                        │
                                        ▼
                                  M7: Dev PCB Works
                                        │
                                        ▼
                                  M8: 6-Pod Demo
                                        │
A8: Form-factor design ◄────────────────┘
A9: Order
A10: Assemble ─────────────────► F5-F6 ──► M9
```

---

## DECISION GATES

### Gate 1: After M4 (RF Init)
**Question:** Do WiFi and BLE stacks initialize without errors?
- ✅ Yes → Proceed with unit tests and drivers
- ❌ No → Debug RF init issues (likely sdkconfig or partition table)

### Gate 2: After M6 (ESP-NOW Latency)
**Question:** Is ESP-NOW latency acceptable (P95 < 2ms)?
- ✅ Yes → Use ESP-NOW for pod-to-pod comms
- ❌ No → Fallback to BLE mesh (higher latency but works)

### Gate 3: After M7 (Dev PCB Works)
**Question:** Does custom PCB work end-to-end?
- ✅ Yes → Proceed to multi-pod testing
- ❌ No → Debug, potentially respin PCB

### Gate 4: After M8 (6-Pod Demo)
**Question:** Do 6 pods work reliably together?
- ✅ Yes → Proceed to form-factor design
- ❌ No → Fix issues before form-factor investment

---

## RISKS & MITIGATIONS

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| RF coexistence under load | Low | Medium | ESP32-S3 STA+BLE is stable per Espressif; tune BLE scan params if needed |
| DevKit delayed | Medium | Medium | Order from multiple suppliers |
| PCB layout errors | Medium | Medium | Careful review; dev PCB is throwaway |
| CI DevKit flaky | Medium | Low | Retry logic; manual fallback |
| Touch through diffuser | Low | Medium | Test materials early |
| ESP-NOW latency spikes | Low | High | Validate P99 < 5ms; fallback to time-sliced BLE if needed |

**Note on RF coexistence:** Init code is low-risk. Real issues surface under sustained load
with specific patterns (e.g., BLE scan window = scan interval starves WiFi). These are
config issues, not fundamental blockers. Validated during M7/M8 integration.

---

*Document Updated: 2026-02-09*
*Project: DOMES*
