# 07 - Debugging

## AI Agent Instructions

Load this file when diagnosing crashes, hangs, or setting up debugging tools.

Prerequisites: `00-getting-started.md`

---

## Debug Setup

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DEBUG ARCHITECTURE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Development Machine                                                        │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  VS Code ◄──────► GDB ◄──────► OpenOCD ◄──────┐                     │   │
│   │                                               │                     │   │
│   │  Serial Monitor ◄─────────────────────────────┼──┐                  │   │
│   └───────────────────────────────────────────────┼──┼──────────────────┘   │
│                                                   │  │                      │
│                                       USB-C ──────┘  │ UART                 │
│                                       (JTAG)         │                      │
│                                                      │                      │
│   ┌──────────────────────────────────────────────────┼──────────────────┐   │
│   │                        ESP32-S3                  │                  │   │
│   │  ┌─────────────┐                                 │                  │   │
│   │  │ USB-JTAG    │ GPIO19/20 (built-in)           │                  │   │
│   │  └─────────────┘                                 │                  │   │
│   │  ┌─────────────┐                                 │                  │   │
│   │  │ UART0       │ TX/RX for logging ─────────────┘                  │   │
│   │  └─────────────┘                                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   Key: ESP32-S3 has built-in USB-JTAG - no external debugger needed         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Log Levels

| Level | Macro | When to Use |
|-------|-------|-------------|
| Error | `ESP_LOGE(tag, ...)` | Always shown, critical failures |
| Warning | `ESP_LOGW(tag, ...)` | Important issues |
| Info | `ESP_LOGI(tag, ...)` | Normal operation milestones |
| Debug | `ESP_LOGD(tag, ...)` | Debugging details |
| Verbose | `ESP_LOGV(tag, ...)` | Trace-level |

**Log Format:** `I (1234) module: Message`
- `I/W/E/D/V` = Level
- `(1234)` = Timestamp (ms since boot)
- `module` = Tag

---

## GDB Quick Reference

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           GDB COMMANDS                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Breakpoints:                    Execution:                                 │
│   ────────────                    ──────────                                 │
│   break app_main                  continue (c)      Resume                  │
│   break file.cpp:42               next (n)          Step over               │
│   info breakpoints                step (s)          Step into               │
│   delete 1                        finish            Run to return           │
│                                                                              │
│   Inspection:                     Memory:                                    │
│   ───────────                     ───────                                    │
│   print variable                  x/10xw 0x3FC00000  Examine 10 words       │
│   print/x variable                x/s 0x3FC00000     Examine as string      │
│   info locals                     info registers                            │
│   backtrace                                                                  │
│                                                                              │
│   Reset:                                                                     │
│   ──────                                                                     │
│   mon reset halt                  Reset and halt                            │
│   mon reset run                   Reset and run                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Core Dump Analysis

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CORE DUMP FLOW                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Crash Occurs                                                               │
│        │                                                                     │
│        ▼                                                                     │
│   ┌────────────┐                                                            │
│   │ Write core │ → Flash partition (64KB)                                   │
│   │ dump       │                                                            │
│   └─────┬──────┘                                                            │
│         │ Reboot                                                            │
│         ▼                                                                   │
│   ┌────────────┐        ┌────────────────────────────────────────┐         │
│   │  Retrieve  │───────►│  idf.py coredump-info -p /dev/ttyUSB0  │         │
│   │  via tool  │        └────────────────────────────────────────┘         │
│   └─────┬──────┘                                                            │
│         │                                                                   │
│         ▼                                                                   │
│   ┌────────────┐                                                            │
│   │  Analyze   │   Shows: crashed task, registers, call stack              │
│   └────────────┘                                                            │
│                                                                              │
│   Enable: CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Common Issues

### Connection Problems

| Symptom | Cause | Solution |
|---------|-------|----------|
| Can't connect | Charge-only cable | Use data-capable USB cable |
| Connection refused | Wrong boot mode | Hold BOOT + press RESET |
| Permission denied | No device access | `chmod 666 /dev/ttyUSB0` |
| Port busy | Another process | Close other serial monitors |

### Runtime Errors

| Error | Likely Cause | Solution |
|-------|--------------|----------|
| `ESP_ERR_WIFI_NOT_INIT` | Init order wrong | NVS → Event loop → WiFi |
| `ESP_ERR_TIMEOUT` (I2C) | No pull-ups | Add 4.7kΩ to SDA/SCL |
| `Task stack overflow` | Stack too small | Increase in `xTaskCreate()` |
| `Task watchdog triggered` | Blocking loop | Add `esp_task_wdt_reset()` |
| `malloc failed` | Heap exhausted | Check `esp_get_free_heap_size()` |
| PSRAM not detected | Wrong module/config | Verify N16R8, enable SPIRAM |

---

## Diagnostics

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DIAGNOSTIC CHECKS                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Memory:                                                                    │
│   • Free heap: esp_get_free_heap_size()           Should be > 50KB          │
│   • Min free: esp_get_minimum_free_heap_size()    Track low watermark       │
│   • Largest block: heap_caps_get_largest_free_block()                       │
│                                                                              │
│   Task Stack:                                                                │
│   • High water mark: uxTaskGetStackHighWaterMark()                          │
│   • Warning threshold: < 100 words remaining                                │
│                                                                              │
│   Timing:                                                                    │
│   • Measure: esp_timer_get_time() (microseconds)                            │
│   • Task stats: vTaskGetRunTimeStats() (requires trace facility)            │
│                                                                              │
│   I2C Scan:                                                                  │
│   • Probe addresses 0x01-0x7F                                               │
│   • Expected: 0x18 (IMU), 0x5A (Haptic)                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Panic Handler Output

```
Example crash output:

===============================================================
==================== ESP32 CORE DUMP START ====================

Crashed task: 'game', handle: 0x3fceb8b0

Exception cause: StoreProhibitedCause (0x1d)
Exception address: 0x0 (null pointer dereference)

Program counter: 0x42012abc

CALL STACK:
#0  feedbackService::playSuccess() at feedback_service.cpp:45
#1  gameEngine::onTouch() at game_engine.cpp:123
#2  gameTask() at main.cpp:89

REGISTERS:
pc: 0x42012abc  sp: 0x3fce0000  a0: 0x42011234

===============================================================
```

---

## Sdkconfig Debug Options

| Option | Purpose |
|--------|---------|
| `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` | Save crash data |
| `CONFIG_FREERTOS_USE_TRACE_FACILITY` | Task runtime stats |
| `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` | CPU usage |
| `CONFIG_LOG_DEFAULT_LEVEL_DEBUG` | Verbose logging |
| `CONFIG_COMPILER_OPTIMIZATION_DEBUG` | Better stack traces |

---

*Prerequisites: 00-getting-started.md*
*Related: 02-build-system.md (debug builds)*
