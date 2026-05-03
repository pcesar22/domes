# ESP32-S3 GDB CLI Workflow

Use this reference when debugging DOMES firmware through shell commands rather than a dedicated
ESP32 MCP debugger.

## Build And Flash Matching Firmware

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash
```

## Start OpenOCD

Start OpenOCD in a long-running shell session from `firmware/domes`:

```bash
. ~/esp/esp-idf/export.sh
idf.py openocd
```

Leave it running while GDB connects.

## Start GDB

In another shell session:

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py gdb
```

If `idf.py gdb` is not usable in the environment, run the toolchain GDB directly against
`build/domes.elf` and connect to OpenOCD:

```bash
xtensa-esp32s3-elf-gdb build/domes.elf
target remote :3333
```

## Initial GDB Commands

```gdb
monitor reset halt
break app_main
continue
```

After the first breakpoint proves symbols work, set targeted breakpoints:

```gdb
break firmware/domes/main/main.cpp:120
break domes::EspNowService::onReceive
continue
```

## Inspect State

```gdb
bt
where
info locals
info args
info threads
thread apply all bt
p variableName
p *pointer
x/32xb address
```

## FreeRTOS Tips

- Use `info threads` to inspect tasks when the OpenOCD FreeRTOS plugin is active.
- Switch threads with `thread <n>`.
- If the target is running and commands block, interrupt first.
- For watchdog issues, inspect the task that owns the long-running loop and stack high-water
  logging around that path.

## Crash And Panic Workflow

1. Capture serial logs first, including the panic reason and backtrace addresses.
2. Check whether `domes-cli --port /dev/ttyACM0 system crash-dump` returns stored crash data.
3. Rebuild without changing source if symbol fidelity is uncertain.
4. Use GDB `info line *0xADDRESS` for backtrace addresses if needed.
5. Set breakpoints before the suspected failure path and reproduce.

## Common Problems

| Problem | Fix |
| --- | --- |
| Breakpoint never hit | Verify source matches ELF, set `app_main` first, reflash |
| GDB command hangs | Interrupt the target, then retry |
| OpenOCD cannot connect | Close stale sessions, unplug/replug, retry |
| Wrong source lines | Rebuild and reflash the exact source tree |
| Panic loop | Erase flash only if stale NVS is suspected, then reflash |
| Serial logs disappear | Serial OTA may own USB-CDC; debug earlier init or delay Serial OTA |

## Cleanup

In GDB:

```gdb
detach
quit
```

Stop the OpenOCD shell session after GDB exits.
