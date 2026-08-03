---
name: domes-esp32-firmware
description: Build, flash, monitor, test, and validate DOMES ESP32-S3 firmware with ESP-IDF, serial logs, domes-cli hardware checks, OTA, BLE, ESP-NOW, and multi-device workflows. Use when working on firmware under firmware/domes or firmware/common, ESP-IDF builds, device flashing, runtime config protocol, hardware validation, or any task that previously referenced the DOMES Claude slash commands such as flash, monitor, lint-fw, size, test-ble, test-espnow, test-imu, test-leds, test-touch, erase-flash, validate-hw, or new-driver.
---

# DOMES ESP32 Firmware

Use this skill for ESP-IDF firmware work, hardware-facing verification, serial monitoring, and
the DOMES CLI workflows that exercise real pods.

## Defaults

| Item | Value |
| --- | --- |
| Firmware project | `firmware/domes` |
| ESP-IDF version | v5.4.4, matching CI and `dependencies.lock` |
| ESP-IDF environment | `. ~/esp/esp-idf/export.sh` |
| Flash/config/OTA device | CP2102N `/dev/serial/by-id/...` link (`/dev/ttyUSB*`) |
| Console/JTAG device | Native USB Serial/JTAG (`/dev/ttyACM*`) |
| CLI project | `tools/domes-cli` |
| Serial monitor script | `tools/firmware/monitor_serial.py` |
| Flash helper | `tools/firmware/flash_and_verify.sh` |

Before running any `idf.py` command, source ESP-IDF:

```bash
. ~/esp/esp-idf/export.sh
idf.py --version  # Must report ESP-IDF v5.4.4
```

## Choose A Workflow

- **Build only**: use `scripts/verify.sh` for final evidence or the isolated command below.
- **Flash and verify**: use `tools/firmware/flash_and_verify.sh` when a device is available.
- **Monitor serial**: use `tools/firmware/monitor_serial.py`; do not read serial ports with `cat`, `dd`,
  `head`, or `tail`, and do not adjust serial devices with `stty` unless the user explicitly asks.
- **Hardware validation**: read `references/runbooks.md` and run the relevant subsystem runbook.
- **Config, Kconfig, or partition work**: read `references/configs.md`.
- **Platform, BLE, or multi-device setup**: read `.codex/PLATFORM.md`.

## Build And Flash

```bash
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
```

An ignored project-local `firmware/domes/sdkconfig` can override updated defaults. Do not use a
build that reused it as final evidence.

Single device:

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh firmware/domes "$PORT"
```

Two devices:

```bash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1,$PORT2"
```

The helper builds and flashes over each CP2102N port, then verifies the exact embedded version,
system health, and the complete on-device self-test over the framed runtime path. It does not look
for console text on the protocol UART.

## Monitor Serial

```bash
mapfile -t CONSOLES < <(
  find -L /dev/serial/by-id -maxdepth 1 -type c \
    -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort
)
test "${#CONSOLES[@]}" -ge 1
python3 tools/firmware/monitor_serial.py "${CONSOLES[0]}" 15

test "${#CONSOLES[@]}" -ge 2
python3 tools/firmware/monitor_serial.py \
  "${CONSOLES[0]},${CONSOLES[1]}" 30
```

These monitor commands require the separate native USB connection. Look for boot messages, errors,
warnings, feature init, mode transitions, IMU/touch readings, ESP-NOW discovery, and heap
diagnostics. Use CP2102N ports for flashing and CLI commands, not console monitoring.

## Protocol Rules

Config and trace protocol definitions come from `firmware/common/proto/*.proto`. Do not add manual
duplicates in C++, Rust, or Dart. OTA transfer structs, compact trace recorder events, and internal
ESP-NOW peer packets are bounded existing fixed-binary exceptions; keep their consumers compatible
and do not create another exception family.

When protocol messages change:

1. Edit the `.proto` file.
2. Run `tools/generate_protocols.sh`; firmware builds only compile committed nanopb output.
3. Rebuild `tools/domes-cli` so prost output updates.
4. Regenerate Flutter protobufs if the app consumes the changed message.
5. Verify over at least one real transport when hardware is available.

Most config command responses wrap the response protobuf as `[Status:u8][Protobuf payload]`.
List and diagnostic responses without command status, plus unsolicited notifications, contain the
protobuf directly. Preserve the paired firmware/host envelope when changing a message.

## Resources

- `tools/firmware/monitor_serial.py`: canonical non-TTY serial monitor with multi-device labels.
- `tools/firmware/flash_and_verify.sh`: canonical build, flash, and boot-marker helper.
- `scripts/`: compatibility wrappers for existing skill invocations.
- `references/runbooks.md`: Codex-friendly conversions of the old DOMES slash-command workflows.
- `references/configs.md`: ESP-IDF config, partition, and pin reference.
