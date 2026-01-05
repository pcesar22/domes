# 13 - Validation Test Hooks

## AI Agent Instructions

Load this file when setting up milestone validation or test infrastructure.

Prerequisites: `06-testing.md`

---

## Overview

Test hooks for validating each development milestone:
- **Serial commands** for triggering tests
- **BLE characteristics** for phone-triggered validation
- **Scripts** for CI/nightly testing

---

## Milestone Test Matrix

| Milestone | Method | Hardware | Automation |
|-----------|--------|----------|------------|
| M1: Compiles | CI | None | ✅ Full |
| M2: Boots | Serial + script | 1 DevKit | ✅ Full |
| M3: Debug | Serial | 1 DevKit | Manual |
| M4: RF Valid | Serial + script | 1 DevKit | ✅ Full |
| M5: Unit Tests | CI | None (host) | ✅ Full |
| M6: Latency | Serial + script | 2 DevKits | ✅ Full |
| M7: PCB Works | Serial/BLE | Custom PCB | Semi-auto |
| M8: 6-Pod Demo | Serial | 6 Pods | Semi-auto |
| M9: Demo Ready | Full app | 6 Pods + Phone | Manual |

---

## Serial Commands

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SERIAL COMMAND INTERFACE                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Format: command_name [arg1] [arg2] ...                                    │
│   Example: test_led 255 0 0                                                 │
│                                                                              │
│   Boot/Health:           RF Tests:              Hardware Tests:             │
│   ────────────           ─────────              ────────────────            │
│   boot_info              wifi_scan              test_all                    │
│   health_check           ble_advertise          test_led [r] [g] [b]        │
│   mem_stats              coex_test [duration]   test_audio [freq] [ms]      │
│   stack_check            espnow_ping <mac>      test_haptic [effect]        │
│                                                 test_touch                   │
│   Latency/Sync:          Multi-Pod:             test_imu                    │
│   ────────────           ──────────             test_battery                │
│   latency_test <mac>     network_info                                       │
│   sync_stats             sync_flash                                         │
│                          touch_relay                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Test Output Format

All tests output machine-readable results for scripting:

| Test | Output Format |
|------|---------------|
| Boot | `BOOT_RESULT:heap:psram:nvs:time_ms` |
| RF Coex | `RF_RESULT:coex_ok:scans:errors:ble_errors` |
| Latency | `LATENCY_RESULT:p50:p95:p99:lost:pass` |
| Smoke | `SMOKE_RESULT:passed:failed` |

---

## M2: Boot Validation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           BOOT VALIDATION                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Checks:                          Pass Criteria:                           │
│   ───────                          ─────────────                            │
│   • Heap free                      > 100KB                                  │
│   • PSRAM free                     > 4MB                                    │
│   • NVS accessible                 Opens without error                      │
│   • Boot time                      < 3 seconds                              │
│                                                                              │
│   Script: scripts/validate_boot.sh /dev/ttyUSB0                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## M4: RF Validation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RF COEXISTENCE TEST                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Test sequence (30s default):                                              │
│   ────────────────────────────                                              │
│   1. Initialize WiFi (STA mode)                                             │
│   2. Initialize BLE (NimBLE)                                                │
│   3. Loop:                                                                  │
│      • WiFi scan                                                            │
│      • Toggle BLE advertising                                               │
│   4. Count errors                                                           │
│                                                                              │
│   Pass: No errors during stress test                                        │
│                                                                              │
│   Serial: coex_test 30000                                                   │
│   Script: scripts/validate_rf.sh /dev/ttyUSB0                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## M6: Latency Validation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LATENCY TEST                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Pod A                              Pod B                                   │
│     │                                  │                                     │
│     │──── PING ───────────────────────►│                                     │
│     │◄─── PONG ────────────────────────│                                     │
│     │                                  │                                     │
│     └──── Measure RTT ─────────────────┘                                     │
│                                                                              │
│   Repeat 1000x, calculate percentiles                                       │
│                                                                              │
│   Pass Criteria:                                                            │
│   • P50 one-way < 1ms                                                       │
│   • P95 one-way < 2ms                                                       │
│   • Packet loss < 1%                                                        │
│                                                                              │
│   Serial: latency_test AA:BB:CC:DD:EE:FF 1000                               │
│   Script: scripts/validate_latency.sh /dev/ttyUSB0 /dev/ttyUSB1             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## M7: Smoke Test Suite

| Test | Method | Pass Criteria |
|------|--------|---------------|
| I2C Bus | Probe devices | IMU + Haptic respond |
| LED Ring | Color cycle | All 16 LEDs visible |
| Audio | Tone sequence | Audible 440/880/1320 Hz |
| Haptic | Effect sequence | Vibration felt |
| IMU | Read accel | ~1g magnitude |
| Battery | Read voltage | 3.0-4.25V |
| Touch | Wait for input | Detects within 5s |
| Tap | Wait for tap | IMU detects tap |

Serial: `test_all`

---

## M8: Multi-Pod Tests

| Test | Command | Purpose |
|------|---------|---------|
| Network info | `network_info` | Show peer count, master status |
| Sync flash | `sync_flash` | All pods flash simultaneously |
| Touch relay | `touch_relay` | Touch one → lights next |

Requires master pod. Measure sync accuracy with oscilloscope.

---

## BLE Test Service

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         BLE TEST SERVICE (0x1900)                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   UUID      │ Name            │ Properties  │ Purpose                       │
│   ──────────┼─────────────────┼─────────────┼───────────────────────────────│
│   0x1901    │ Trigger Test    │ Write       │ Start smoke test suite        │
│   0x1902    │ Test Result     │ Notify      │ Receive test results          │
│   0x1903    │ Run Specific    │ Write       │ Run individual test by ID     │
│   0x1904    │ Diagnostics     │ Read        │ Get device diagnostics        │
│   0x1905    │ Force State     │ Write       │ Force master/follower/etc     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## CI Pipeline

```yaml
# .github/workflows/validation.yml

jobs:
  compile:
    runs-on: ubuntu-latest
    steps:
      - run: cd firmware && idf.py build

  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - run: idf.py --preview set-target linux && idf.py build
      - run: ./build/domes.elf

  nightly-hardware:
    runs-on: [self-hosted, devkit]
    schedule: [cron: '0 3 * * *']
    steps:
      - run: ./scripts/validate_boot.sh
      - run: ./scripts/validate_rf.sh
      - run: ./scripts/validate_latency.sh
```

---

## Validation Checklist

| Milestone | Serial | BLE | Script | CI |
|-----------|--------|-----|--------|-----|
| M1 | - | - | - | ✅ |
| M2 | `boot_info` | - | ✅ | ✅ |
| M3 | `crash_test` | - | - | - |
| M4 | `coex_test` | - | ✅ | ✅ |
| M5 | `run_tests` | - | - | ✅ |
| M6 | `latency_test` | - | ✅ | ✅ |
| M7 | `test_all` | 0x1901 | - | - |
| M8 | `sync_flash` | - | - | - |
| M9 | - | Full app | - | - |

---

*Related: `06-testing.md`, all architecture docs*
