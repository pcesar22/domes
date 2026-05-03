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
| ESP-IDF environment | `. ~/esp/esp-idf/export.sh` |
| First serial device | `/dev/ttyACM0` |
| Second serial device | `/dev/ttyACM1` |
| CLI project | `tools/domes-cli` |
| Serial monitor script | `scripts/monitor_serial.py` |
| Flash helper | `scripts/flash_and_verify.sh` |

Before running any `idf.py` command, source ESP-IDF:

```bash
. ~/esp/esp-idf/export.sh
```

## Choose A Workflow

- **Build only**: run `idf.py build` from `firmware/domes`.
- **Flash and verify**: use `scripts/flash_and_verify.sh` when a device is available.
- **Monitor serial**: use `scripts/monitor_serial.py`; do not read serial ports with `cat`, `dd`,
  `head`, or `tail`, and do not adjust serial devices with `stty` unless the user explicitly asks.
- **Hardware validation**: read `references/runbooks.md` and run the relevant subsystem runbook.
- **Config, Kconfig, or partition work**: read `references/configs.md`.
- **Platform, BLE, or multi-device setup**: read `.codex/PLATFORM.md`.

## Build And Flash

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
```

Single device:

```bash
.codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh firmware/domes /dev/ttyACM0 "DOMES"
```

Two devices:

```bash
.codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh firmware/domes /dev/ttyACM0,/dev/ttyACM1 "DOMES"
```

## Monitor Serial

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0 15
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 30
```

Look for boot messages, errors, warnings, feature init, mode transitions, IMU/touch readings,
ESP-NOW discovery, and heap diagnostics.

## Protocol Rules

All firmware/host/app protocol definitions come from `firmware/common/proto/*.proto`. Do not add
manual duplicate enums or message structs in C++, Rust, or Dart.

When protocol messages change:

1. Edit the `.proto` file.
2. Rebuild firmware so nanopb output updates.
3. Rebuild `tools/domes-cli` so prost output updates.
4. Regenerate Flutter protobufs if the app consumes the changed message.
5. Verify over at least one real transport when hardware is available.

## Resources

- `scripts/monitor_serial.py`: non-TTY serial monitor with multi-device labels.
- `scripts/flash_and_verify.sh`: build, flash one or more devices, and search boot output.
- `references/runbooks.md`: Codex-friendly conversions of the old DOMES slash-command workflows.
- `references/configs.md`: ESP-IDF config, partition, and pin reference.
