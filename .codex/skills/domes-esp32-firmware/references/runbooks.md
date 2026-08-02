# DOMES Firmware Runbooks

Use these runbooks when a user asks to flash, monitor, validate, lint, size-check, or run hardware
tests. They replace the old Claude slash-command markdown with direct Codex shell workflows.

## Shared Setup

Build the CLI if a runbook uses `domes-cli` and `target/debug/domes-cli` does not exist:

```bash
(cd tools/domes-cli && cargo build --locked)
```

Use this CLI variable from the repo root:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
```

Discover the CP2102N programming/runtime ports and assign stable serial-number paths:

```bash
find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
```

The numeric variable suffix is sorted discovery order, not a firmware pod ID. Query `system info`
before reassigning an existing board.

CP2102N ports (`/dev/ttyUSB*`) carry flashing, `domes-cli`, and serial OTA. Native ESP32-S3 USB
Serial/JTAG ports (`/dev/ttyACM*`) are separate console/JTAG interfaces. Set `CONSOLE1` and
`CONSOLE2` only when those second USB connections are attached.

## Flash Firmware

Use when the user asks to flash, build-and-flash, or verify firmware on a pod.

```bash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1"
```

Multiple ports:

```bash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1,$PORT2"
```

After flashing, run the feature list over the relevant transport:

```bash
tools/domes-cli/target/debug/domes-cli --port "$PORT1" feature list
```

The helper builds and flashes each CP2102N port and verifies framed UART operation with `system
info`. Boot text is not expected on the protocol UART.

If flashing fails, check USB cable, BOOT button, serial permissions, and whether ESP-IDF was
sourced.

## Monitor Serial

Use this instead of `idf.py monitor` in non-TTY Codex sessions:

```bash
CONSOLE1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '1p')"
CONSOLE2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '2p')"
test -n "$CONSOLE1"
CONSOLES=("$CONSOLE1")
if [[ -n "$CONSOLE2" ]]; then
  CONSOLES+=("$CONSOLE2")
fi
CONSOLE_LIST="$(IFS=,; echo "${CONSOLES[*]}")"
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 15
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 30
```

Do not use `cat`, `dd`, `head`, or `tail` on `/dev/ttyACM*` or `/dev/ttyUSB*`. Do not use `stty`
against serial devices unless the user explicitly asks for low-level port configuration.

Filtered examples:

```bash
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 20 2>&1 | rg -i "espnow|esp-now|beacon|discover"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 30 2>&1 | rg -i "game|arm|hit|miss|drill|round"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 10 2>&1 | rg -i "touch|pad"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 10 2>&1 | rg -i "heap|mem|diag"
```

## Erase Flash And Reflash

Use when stale NVS or corrupted state may be causing ESP-NOW, calibration, boot counter, or feature
state problems.

```bash
. ~/esp/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.4.4"
python -m esptool --chip esp32s3 --port "$PORT1" erase_flash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1"
```

Verify:

```bash
tools/domes-cli/target/debug/domes-cli --port "$PORT1" feature list
```

Warn the user that erase-flash clears NVS, pod ID, feature defaults, boot counters, and OTA
partitions.

## Size Analysis

Use after adding large features or when the app may approach partition limits.

```bash
SIZE_ROOT="$(mktemp -d)"
(cd firmware/domes && \
  . ~/esp/esp-idf/export.sh && \
  idf.py -B "$SIZE_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" build && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size-components && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size-files)
```

Report total binary size, IRAM, DRAM, top components by size, and whether the binary fits the app
partition in `partitions.csv`.

## Firmware Lint Pass

Use when the user asks to lint firmware or review against embedded rules.

Read `firmware/AGENTS.md`, then check the requested scope, defaulting to `firmware/domes/main`.

Critical searches:

```bash
rg -n "\b(new|malloc|calloc|realloc)\b" firmware/domes/main
rg -n "#include\s*<(iostream|fstream)>" firmware/domes/main
rg -n "\b(throw|try|catch|typeid|dynamic_cast)\b" firmware/domes/main
rg -n "ESP_LOG[EWIDV]?\s*\(" firmware/domes/main
```

For ISR findings, inspect only functions marked `IRAM_ATTR`; logging and non-`FromISR` FreeRTOS
APIs are blocking issues.

Standard-library containers are not categorically forbidden. Flag unbounded allocation only in
ISRs, deterministic loops, and latency-critical tasks. ETL and `tl::expected` are not dependencies.

Report findings as critical violations, warnings, and style issues with file and line references.

## BLE Transport Test

Prerequisites: native Linux, firmware flashed and booted for at least 8 seconds, Bluetooth powered
on with `bluetoothctl power on`.

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --scan-ble
$CLI --ble "DOMES-Pod-XX" feature list
$CLI --ble "DOMES-Pod-XX" system info
$CLI --ble "DOMES-Pod-XX" system memory
$CLI --ble "DOMES-Pod-XX" led solid --color ff0000
sleep 2
$CLI --ble "DOMES-Pod-XX" led off
$CLI --ble "DOMES-Pod-XX" feature disable haptic
$CLI --ble "DOMES-Pod-XX" feature list
$CLI --ble "DOMES-Pod-XX" feature enable haptic
```

Also test connect-by-MAC if scan output provides the address.

Report devices found, feature list, system info, fragmented memory-response completion, LED command
acceptance, haptic feature-state round trip, and MAC connect status. Ask for visual confirmation for
LEDs. The toggle verifies BLE config behavior; it does not trigger or prove physical haptic output.

## ESP-NOW Integration Test

Requires two pods.

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --port "$PORT1" feature disable esp-now
$CLI --port "$PORT2" feature disable esp-now
sleep 2
$CLI --port "$PORT1" feature enable esp-now
$CLI --port "$PORT2" feature enable esp-now
sleep 25
$CLI --port "$PORT1" espnow status
$CLI --port "$PORT2" espnow status
```

Both devices should show one peer, one master/slave pairing, and RX packets greater than zero.

Monitor drill execution:

```bash
python3 tools/firmware/monitor_serial.py "$CONSOLE1,$CONSOLE2" 20 2>&1 | \
  rg -i "round|arm|hit|miss|drill|game|espnow"
```

Cleanup:

```bash
$CLI --port "$PORT1" feature disable esp-now
$CLI --port "$PORT2" feature disable esp-now
```

Do not use WiFi feature commands as ESP-NOW setup. The default build omits the WiFi client feature,
while a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build treats it as independent stored-credential client
state and preserves it across mode changes.

## IMU Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --port "$PORT1" imu triage --enable
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 5 2>&1 | \
  rg -i "imu|lis2dw12|accel|tap|mag"
```

Expected:

- LIS2DW12 data streaming at about 5 Hz in logs.
- Stationary magnitude around 0.95 g to 1.05 g.
- Flat board Z axis dominates near 1 g.

For tap detection, ask the user to tap the board:

```bash
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 10 2>&1 | rg -i "tap|triage|mode"
$CLI --port "$PORT1" imu triage --disable
```

Hardware details: LIS2DW12 at I2C address 0x19, SDA GPIO8, SCL GPIO9, INT1 GPIO5.

## LED Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

for color in ff0000 00ff00 0000ff ffffff; do
  $CLI --port "$PORT" led solid --color "$color"
  sleep 1
done

$CLI --port "$PORT" led breathing --color ff00ff
sleep 3
$CLI --port "$PORT" led cycle
sleep 4
$CLI --port "$PORT" led off
$CLI --port "$PORT" led get
```

Ask the user to confirm all 16 LEDs, correct colors, and no dead or dim LEDs.

## Touch Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

python3 tools/firmware/monitor_serial.py "$CONSOLE1" 6 2>&1 | rg -i "TouchService|touch"

for pad in 0 1 2 3; do
  $CLI --port "$PORT" touch simulate --pad "$pad"
  sleep 0.5
done
```

Expected pad mapping:

| Pad | GPIO | Touch channel |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | 2 | 2 |
| 2 | 4 | 4 |
| 3 | 6 | 6 |

Warn if baseline values are below 10000 or above 40000. Normal NFF board baselines are often
around 20000 to 28000.

## Baseline Hardware Smoke Test

This is a fast programming and command-path smoke test, not a full readiness result. Run it on each
attached board before the feature-specific workflows listed under Full Readiness Evaluation.

For each port:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

$CLI --port "$PORT" system self-test
$CLI --port "$PORT" system info
$CLI --port "$PORT" feature list
$CLI --port "$PORT" led solid --color ff0000
sleep 1
$CLI --port "$PORT" led solid --color 00ff00
sleep 1
$CLI --port "$PORT" led solid --color 0000ff
sleep 1
$CLI --port "$PORT" led solid --color ffffff
sleep 1
$CLI --port "$PORT" led off
for pad in 0 1 2 3; do $CLI --port "$PORT" touch simulate --pad "$pad"; done
$CLI --scan-ble
```

For two ports, also run the ESP-NOW integration test above.

Report a pass/fail table for self-test, system info, feature list, LED command acceptance, touch
simulation, and BLE scan. For two ports, add ESP-NOW discovery and drill results from the dedicated
integration test.

Command acceptance and self-test initialization do not prove LED color, physical touch sensing,
haptic output, or audio output. Record visual/tactile/audible confirmation separately.

## Full Readiness Evaluation

Use [`hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../../../../hardware/nff-devboard/BRING_UP_CHECKLIST.md)
as the per-board evidence record. The checks below select the runbooks; they do not replace the
checklist's physical observations, programming/OTA acceptance sequence, or explicit unverified rows.

After the baseline passes on each board, run every applicable dedicated workflow in this reference:

- BLE Transport Test, including a real config response over BLE rather than scan-only evidence.
- IMU Test with live stationary readings and a physical tap. Confirm the tap's LED, haptic, and
  audio feedback separately.
- LED Test and Touch Test with physical visual/input confirmation.
- ESP-NOW Integration Test on two boards, including peer packet evidence and drill execution.
- Serial and BLE OTA from the root [`AGENTS.md`](../../../../AGENTS.md), followed by reconnect,
  version, health, self-test, a second reboot, and repeated confirmation. Exercise
  invalid/interrupted recovery and forced failed-self-test rollback separately. The declared
  version must be the parser-valid, at-most-31-byte value embedded in the exact image. Raw TCP OTA
  is not a supported path.
- ESP-IDF panic-dump retrieval from the
  [debug skill](../../domes-debug-esp32/SKILL.md) when a controlled crash test is scheduled; a CLI
  clean-restart snapshot is not equivalent.
- Final `system health`, `system memory`, and `feature list` captures on each board after the test
  sequence.

Mark a row unverified rather than passed when the workflow lacks its required physical observation,
transport, matching firmware image, or second device.

## New Driver Scaffold

Use when the user asks to add a new hardware driver.

Follow existing repo naming rather than the old snake-case templates:

- Interface: `firmware/domes/main/interfaces/i<Name>Driver.hpp`
- Implementation: `firmware/domes/main/drivers/<name>Driver.hpp` plus `.cpp` if needed
- Test fake: follow the nearest pattern under `firmware/test_app/`; there is no
  `firmware/domes/test/mocks/` tree.

Steps:

1. Read nearby interfaces and drivers before creating files.
2. Add an injectable interface only when service isolation provides meaningful test value.
3. Keep deterministic and ISR paths allocation-free; bounded startup ownership may follow nearby
   code.
4. Use ESP-IDF APIs directly and the existing `esp_err_t` or project-result conventions.
5. Add a focused fake and host test under `firmware/test_app/` when the boundary is testable there.
6. Update `firmware/domes/main/CMakeLists.txt` only if adding `.cpp` sources.
7. Add unit tests in the appropriate firmware test area.

Do not invent a generic driver framework; match the local style.
