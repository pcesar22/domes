# ESP32-S3 GDB CLI Workflow

Use this reference when debugging DOMES firmware through shell commands rather than a dedicated
ESP32 MCP debugger. Use ESP-IDF v5.4.4 and the exact ELF/configuration that produced the running
image.

## Build And Flash Matching Firmware

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
DEBUG_BUILD="$PWD/firmware/domes/build-debug"
rm -rf "$DEBUG_BUILD"
(cd firmware/domes && \
  . ~/esp/esp-idf/export.sh && \
  idf.py -B "$DEBUG_BUILD" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$DEBUG_BUILD/sdkconfig" build && \
  idf.py -B "$DEBUG_BUILD" -D "SDKCONFIG=$DEBUG_BUILD/sdkconfig" -p "$PORT" flash)
```

## Start OpenOCD

Start OpenOCD in a long-running shell session from `firmware/domes`:

```bash
. ~/esp/esp-idf/export.sh
idf.py -B "$PWD/build-debug" openocd
```

Leave it running while GDB connects.

## Start GDB

In another shell session:

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py -B "$PWD/build-debug" gdb
```

If `idf.py gdb` is not usable in the environment, run the toolchain GDB directly against the same
fresh-build ELF and connect to OpenOCD:

```bash
xtensa-esp32s3-elf-gdb -ex 'target remote :3333' build-debug/domes.elf
```

## Initial GDB Commands

```gdb
monitor reset halt
break app_main
continue
```

After the first breakpoint proves symbols work, set targeted breakpoints:

```gdb
break initInfrastructure
break domes::EspNowService::handleReceived
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
2. Check whether `domes-cli --port "$PORT" system crash-dump` returns a stored clean-restart
   snapshot. This command does not retrieve a panic backtrace or ESP-IDF core dump.
3. Preserve the exact source, configuration, and `build-debug/domes.elf` used for the running image. If
   the artifact is missing, rebuild only from that matching commit and configuration.
4. Read the flash panic dump through the CP2102N programming port:

   ```bash
   (cd firmware/domes && \
     . ~/esp/esp-idf/export.sh && \
     idf.py -B "$PWD/build-debug" -p "$PORT" coredump-info)
   ```

5. Open the decoded dump in GDB when interactive inspection is needed:

   ```bash
   (cd firmware/domes && \
     . ~/esp/esp-idf/export.sh && \
     idf.py -B "$PWD/build-debug" -p "$PORT" coredump-debug)
   ```

6. Use GDB `info line *0xADDRESS` for log-only backtrace addresses if needed.
7. Set breakpoints before the suspected failure path and reproduce when the stored dump is
   insufficient.

## Common Problems

| Problem | Fix |
| --- | --- |
| Breakpoint never hit | Verify source matches ELF, set `app_main` first, reflash |
| GDB command hangs | Interrupt the target, then retry |
| OpenOCD cannot connect | Close stale sessions, unplug/replug, retry |
| Wrong source lines | Rebuild and reflash the exact source tree |
| Panic loop | Erase flash only if stale NVS is suspected, then reflash |
| Console logs disappear | Confirm the separate native USB connection; UART0 carries framed protocol only |

## Cleanup

In GDB:

```gdb
detach
quit
```

Stop the OpenOCD shell session after GDB exits.
