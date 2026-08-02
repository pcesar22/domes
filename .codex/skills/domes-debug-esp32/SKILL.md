---
name: domes-debug-esp32
description: Debug DOMES ESP32-S3 firmware crashes, panics, breakpoints, backtraces, task state, and runtime behavior with ESP-IDF OpenOCD/GDB workflows. Use when investigating firmware crashes, stack traces, watchdog resets, breakpoints, variable inspection, FreeRTOS task state, or issues that require stepping through firmware rather than only reading logs.
---

# DOMES ESP32 Debugging

Use this skill when serial logs and tests are not enough and the firmware needs GDB/OpenOCD
debugging.

## First Checks

Before starting GDB, verify the build and capture serial output if possible:

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
```

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0 10
```

If symbols or breakpoints look wrong, rebuild and reflash before continuing.

## Standard Workflow

1. Source ESP-IDF.
2. Build the firmware so `firmware/domes/build/domes.elf` matches the source.
3. Flash the same build if the device may be stale.
4. Start OpenOCD from `firmware/domes`.
5. Start GDB against `build/domes.elf`.
6. Reset and halt.
7. Set a small number of breakpoints.
8. Continue, interrupt, inspect stack/locals/tasks, then close sessions cleanly.

Detailed CLI commands are in `references/gdb-cli.md`.

## Useful GDB Commands

```gdb
monitor reset halt
break app_main
break main.cpp:94
continue
interrupt
bt
info locals
info threads
thread apply all bt
p variableName
x/16xb 0x3FC94200
delete
```

## DOMES Defaults

| Item | Value |
| --- | --- |
| Project | `firmware/domes` |
| ELF | `firmware/domes/build/domes.elf` |
| First serial port | `/dev/ttyACM0` |
| Second serial port | `/dev/ttyACM1` |
| Debug transport | Built-in USB JTAG when available |

## Multi-Device Debugging

Debug one pod at a time unless separate JTAG adapters are configured. While one pod is under GDB,
monitor the second pod over serial:

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM1 30
```

For ESP-NOW issues, set breakpoints in callbacks on the debugged pod and monitor the peer's serial
logs for corresponding packets, role changes, and drill events.
