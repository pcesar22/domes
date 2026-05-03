# DOMES Firmware Runbooks

Use these runbooks when a user asks to flash, monitor, validate, lint, size-check, or run hardware
tests. They replace the old Claude slash-command markdown with direct Codex shell workflows.

## Shared Setup

Build the CLI if a runbook uses `domes-cli` and `target/debug/domes-cli` does not exist:

```bash
cd tools/domes-cli
cargo build
```

Use this CLI variable from the repo root:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
```

Auto-detect serial ports with:

```bash
ls /dev/ttyACM* 2>/dev/null
```

## Flash Firmware

Use when the user asks to flash, build-and-flash, or verify firmware on a pod.

```bash
.codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh firmware/domes /dev/ttyACM0 "DOMES"
```

Multiple ports:

```bash
.codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh firmware/domes /dev/ttyACM0,/dev/ttyACM1 "DOMES"
```

After flashing, run the feature list over the relevant transport:

```bash
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 feature list
```

If flashing fails, check USB cable, BOOT button, serial permissions, and whether ESP-IDF was
sourced.

## Monitor Serial

Use this instead of `idf.py monitor` in non-TTY Codex sessions:

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0 15
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 30
```

Do not use `cat`, `dd`, `head`, or `tail` on `/dev/ttyACM*` or `/dev/ttyUSB*`. Do not use `stty`
against serial devices unless the user explicitly asks for low-level port configuration.

Filtered examples:

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py "$PORTS" 20 2>&1 | rg -i "espnow|esp-now|beacon|discover"
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py "$PORTS" 30 2>&1 | rg -i "game|arm|hit|miss|drill|round"
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py "$PORTS" 10 2>&1 | rg -i "touch|pad"
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py "$PORTS" 10 2>&1 | rg -i "heap|mem|diag"
```

## Erase Flash And Reflash

Use when stale NVS or corrupted state may be causing ESP-NOW, calibration, boot counter, or feature
state problems.

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 erase-flash
idf.py -p /dev/ttyACM0 flash
```

Verify:

```bash
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 feature list
```

Warn the user that erase-flash clears NVS, pod ID, feature defaults, boot counters, and OTA
partitions.

## Size Analysis

Use after adding large features or when the app may approach partition limits.

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py size
idf.py size-components
idf.py size-files
```

Report total binary size, IRAM, DRAM, top components by size, and whether the binary fits the app
partition in `partitions.csv`.

## Firmware Lint Pass

Use when the user asks to lint firmware or review against embedded rules.

Read `firmware/AGENTS.md`, then check the requested scope, defaulting to `firmware/domes/main`.

Critical searches:

```bash
rg -n "\b(new|malloc|calloc|realloc)\b" firmware/domes/main
rg -n "std::(vector|string|map|list|set|unordered_map)" firmware/domes/main
rg -n "#include\s*<(iostream|fstream)>" firmware/domes/main
rg -n "\b(throw|try|catch|typeid|dynamic_cast)\b" firmware/domes/main
rg -n "ESP_LOG[EWIDV]?\s*\(" firmware/domes/main
```

For ISR findings, inspect only functions marked `IRAM_ATTR`; logging and non-`FromISR` FreeRTOS
APIs are blocking issues.

Report findings as critical violations, warnings, and style issues with file and line references.

## BLE Transport Test

Prerequisites: native Linux, firmware flashed and booted for at least 8 seconds, Bluetooth powered
on with `bluetoothctl power on`.

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --scan-ble
$CLI --ble "DOMES-Pod-XX" feature list
$CLI --ble "DOMES-Pod-XX" system info
$CLI --ble "DOMES-Pod-XX" led solid --color ff0000
sleep 2
$CLI --ble "DOMES-Pod-XX" led off
$CLI --ble "DOMES-Pod-XX" feature disable haptic
$CLI --ble "DOMES-Pod-XX" feature list
$CLI --ble "DOMES-Pod-XX" feature enable haptic
```

Also test connect-by-MAC if scan output provides the address.

Report devices found, feature list, system info, LED command acceptance, feature toggle, and MAC
connect status. Ask for visual confirmation for LEDs.

## ESP-NOW Integration Test

Requires two pods.

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT1=/dev/ttyACM0
PORT2=/dev/ttyACM1

$CLI --port $PORT1 feature disable wifi
$CLI --port $PORT2 feature disable wifi
sleep 1
$CLI --port $PORT1 feature disable esp-now
$CLI --port $PORT2 feature disable esp-now
sleep 2
$CLI --port $PORT1 feature enable esp-now
$CLI --port $PORT2 feature enable esp-now
sleep 25
$CLI --port $PORT1 espnow status
$CLI --port $PORT2 espnow status
```

Both devices should show one peer, one master/slave pairing, and RX packets greater than zero.

Monitor drill execution:

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py "$PORT1,$PORT2" 20 2>&1 | rg -i "round|arm|hit|miss|drill|game|espnow"
```

Cleanup:

```bash
$CLI --port $PORT1 feature disable esp-now
$CLI --port $PORT2 feature disable esp-now
$CLI --port $PORT1 feature enable wifi
$CLI --port $PORT2 feature enable wifi
```

## IMU Test

```bash
PORT=/dev/ttyACM0
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py $PORT 5 2>&1 | rg -i "imu|lis2dw12|accel|tap|mag"
```

Expected:

- LIS2DW12 data streaming at about 5 Hz in logs.
- Stationary magnitude around 0.95 g to 1.05 g.
- Flat board Z axis dominates near 1 g.

For tap detection, ask the user to tap the board:

```bash
python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py $PORT 10 2>&1 | rg -i "tap|triage|mode"
```

Hardware details: LIS2DW12 at I2C address 0x19, SDA GPIO8, SCL GPIO9, INT1 GPIO5.

## LED Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT=/dev/ttyACM0

for color in ff0000 00ff00 0000ff ffffff; do
  $CLI --port $PORT led solid --color $color
  sleep 1
done

$CLI --port $PORT led breathing --color ff00ff
sleep 3
$CLI --port $PORT led cycle
sleep 4
$CLI --port $PORT led off
$CLI --port $PORT led get
```

Ask the user to confirm all 16 LEDs, correct colors, and no dead or dim LEDs.

## Touch Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT=/dev/ttyACM0

python3 .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py $PORT 6 2>&1 | rg -i "TouchService|touch"

for pad in 0 1 2 3; do
  $CLI --port $PORT touch simulate --pad $pad
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

## Full Hardware Validation

For each port:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT=/dev/ttyACM0

$CLI --port $PORT system self-test
$CLI --port $PORT system info
$CLI --port $PORT feature list
$CLI --port $PORT led solid --color ff0000
sleep 1
$CLI --port $PORT led solid --color 00ff00
sleep 1
$CLI --port $PORT led solid --color 0000ff
sleep 1
$CLI --port $PORT led solid --color ffffff
sleep 1
$CLI --port $PORT led off
for pad in 0 1 2 3; do $CLI --port $PORT touch simulate --pad $pad; done
$CLI --scan-ble
```

For two ports, also run the ESP-NOW integration test above.

Report a pass/fail table for self-test, system info, feature list, LED command acceptance, touch
simulation, BLE scan, BLE transport, ESP-NOW discovery, and ESP-NOW drill.

## New Driver Scaffold

Use when the user asks to add a new hardware driver.

Follow existing repo naming rather than the old snake-case templates:

- Interface: `firmware/domes/main/interfaces/i<Name>Driver.hpp`
- Implementation: `firmware/domes/main/drivers/<name>Driver.hpp` plus `.cpp` if needed
- Mock: `firmware/domes/test/mocks/mock<Name>Driver.hpp`

Steps:

1. Read nearby interfaces and drivers before creating files.
2. Define an abstract `I<Name>Driver` with a virtual destructor and no copy operations.
3. Keep implementation statically allocated and non-copyable/non-movable.
4. Use ESP-IDF APIs directly, ETL/static buffers, and `esp_err_t` or `tl::expected`.
5. Add a mock with call counters and configurable return values.
6. Update `firmware/domes/main/CMakeLists.txt` only if adding `.cpp` sources.
7. Add unit tests in the appropriate firmware test area.

Do not invent a generic driver framework; match the local style.
