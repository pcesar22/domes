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
DEBUG_BUILD="$PWD/firmware/domes/build-debug"
rm -rf "$DEBUG_BUILD"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$DEBUG_BUILD" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$DEBUG_BUILD/sdkconfig" build)
```

```bash
CONSOLE="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '1p')"
test -n "$CONSOLE"
python3 tools/firmware/monitor_serial.py "$CONSOLE" 10
```

If symbols or breakpoints look wrong, rebuild and reflash before continuing.

## Standard Workflow

1. Source ESP-IDF.
2. Build with a fresh isolated SDKCONFIG so `firmware/domes/build-debug/domes.elf` matches the source.
3. Flash the same build if the device may be stale.
4. Start OpenOCD from `firmware/domes`.
5. Start GDB against `build-debug/domes.elf`.
6. Reset and halt.
7. Set a small number of breakpoints.
8. Continue, interrupt, inspect stack/locals/tasks, then close sessions cleanly.

Detailed CLI commands are in `references/gdb-cli.md`.

## Useful GDB Commands

```gdb
monitor reset halt
break app_main
break initInfrastructure
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
| ESP-IDF | v5.4.4, matching the running firmware build and CI |
| ELF | `firmware/domes/build-debug/domes.elf` from a fresh debug build |
| Flash/config/OTA port | CP2102N `/dev/serial/by-id/...` (`/dev/ttyUSB*`) |
| Console port | Native USB Serial/JTAG (`/dev/ttyACM*`) |
| Debug transport | Built-in USB JTAG when available |

## Multi-Device Debugging

Debug one pod at a time unless separate JTAG adapters are configured. While one pod is under GDB,
monitor the second pod over serial:

```bash
PEER_CONSOLE="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '2p')"
test -n "$PEER_CONSOLE"
python3 tools/firmware/monitor_serial.py "$PEER_CONSOLE" 30
```

For ESP-NOW issues, set breakpoints in callbacks on the debugged pod and monitor the peer's serial
logs for corresponding packets, role changes, and drill events.
