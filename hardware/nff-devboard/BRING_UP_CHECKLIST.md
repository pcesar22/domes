# NFF Development Board Bring-Up Checklist

Run this checklist for each newly assembled carrier. Record the board identifier, firmware commit,
date, and tester so a checkmark is meaningful.

**Board ID:** __________  **Firmware commit:** __________  **Date/tester:** __________

## 1. Before Power

- [ ] Inspect orientation, missing parts, solder bridges, and damaged components.
- [ ] Confirm the DevKit header orientation before insertion.
- [ ] Check 3.3 V to ground and 5 V to ground for shorts.
- [ ] Confirm the speaker and haptic actuator match the design; U5 should be `LD0832AA-0099F`.

## 2. Power Rails

- [ ] Insert the ESP32-S3 DevKit and power it from the DevKit USB connector.
- [ ] Measure the 3.3 V rail within the board tolerance.
- [ ] Measure the 5 V LED/audio rail when populated.
- [ ] Stop immediately for unexpected heating or excessive current.

## 3. Build, Flash, And Identify

```bash
cargo build --locked --manifest-path tools/domes-cli/Cargo.toml
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
CONSOLE="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh \
  firmware/domes "$PORT"
```

The CP2102N port is the DevKit programming, framed UART, and serial OTA interface. The helper builds,
flashes, and verifies the exact embedded version, system health, and the complete on-device
self-test over that UART. Native USB Serial/JTAG is a separate optional console/JTAG connection.
Continue from the repository root:

```bash
tools/domes-cli/target/debug/domes-cli --port "$PORT" system info
tools/domes-cli/target/debug/domes-cli --port "$PORT" system self-test
tools/domes-cli/target/debug/domes-cli --port "$PORT" feature list
```

- [ ] If native USB is attached, its boot log reports NFF LED mapping: GPIO16, 16 devices, RGBW disabled.
- [ ] System info and feature list return over serial.
- [ ] All reported self-test checks pass. Peripheral initialization checks do not replace the
  physical LED, touch, IMU, haptic, and audio observations below.

## 4. LED Ring

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --port "$PORT" led solid --color ff0000
$CLI --port "$PORT" led solid --color 00ff00
$CLI --port "$PORT" led solid --color 0000ff
$CLI --port "$PORT" led solid --color ffffff
$CLI --port "$PORT" led cycle --period 1000
$CLI --port "$PORT" led off
```

- [ ] All 16 devices respond in order.
- [ ] Red, green, blue, and mixed white are correct in RGB mode.
- [ ] Brightness is uniform with no flicker or dead device.

If the ring is dark, inspect the GPIO16 `LED_DATA_3V3` path, level shifter, 5 V supply, and first LED.

## 5. I2C, IMU, And Haptic

Current expected addresses are LIS2DW12 `0x19` and DRV2605L `0x5A` on GPIO8/GPIO9.

Enable IMU triage before observing orientation or tap events, and use the native USB console if it
is attached:

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --port "$PORT" imu triage --enable
test -n "$CONSOLE"
python3 tools/firmware/monitor_serial.py "$CONSOLE" 15
$CLI --port "$PORT" imu triage --disable
```

- [ ] Both devices acknowledge.
- [ ] Accelerometer readings change with orientation.
- [ ] Tap detection asserts through IMU INT1 on GPIO5.
- [ ] The haptic driver identifies successfully and its short initialization click is felt after
  each boot.
- [ ] A physical IMU tap in triage mode produces the longer haptic effect.

The NFF firmware profile is bounded for the schematic's 1.8 Vrms, 235 Hz LD0832AA-0099F. Do not run
haptic effects if a different actuator is populated until its profile is reviewed. The current driver
uses fixed-frequency open-loop operation and does not expose auto-calibration. An I2C ACK alone is
not a haptic pass.

The CLI has no standalone haptic-effect or audio-playback command. The bounded physical trigger is
an IMU tap while triage mode and the audio and haptic features are enabled; it requests the built-in
beep and a long haptic effect. A feature toggle or self-test initialization result alone is not
physical output verification.

## 6. Touch

Touch pads K1-K4 map to GPIO1, GPIO2, GPIO4, and GPIO6.

With native USB attached, monitor while touching each physical pad:

```bash
test -n "$CONSOLE"
python3 tools/firmware/monitor_serial.py "$CONSOLE" 20
```

- [ ] Each untouched baseline is stable.
- [ ] Each physical pad triggers only its expected logical pad.
- [ ] Repeated touches do not cause stuck or cross-triggered input.

Use `domes-cli touch simulate --pad N` only to verify downstream logic; it does not validate the
physical pad or touch peripheral.

## 7. Audio

Current mapping is BCLK GPIO12, LRCLK GPIO11, data GPIO13, and shutdown GPIO7.

Enable IMU triage and physically tap the board. The tap requests the built-in `beep` asset when the
audio feature is enabled; use this as the current bounded playback trigger.

- [ ] Amplifier shutdown control works.
- [ ] The built-in sample is audible.
- [ ] Output is free of obvious clipping at moderate level.
- [ ] Speaker and amplifier remain within a safe temperature.

## 8. Connectivity And Stability

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --scan-ble
$CLI --ble "DOMES-Pod-01" system info
```

- [ ] BLE advertisement is discoverable and the BLE `system info` command succeeds.
- [ ] The Flutter app connects to each intended pod and retains distinct connection identities.
- [ ] In an app-driven drill, a physical touch on the active pod completes its round; a touch on an
  inactive pod is ignored, and stop/disconnect does not advance a stale round.
- [ ] WiFi/TCP feature listing succeeds when credentials are configured.
- [ ] With a second pod, the canonical ESP-NOW runbook completes repeated fresh lifecycles with
  complementary roles, one peer each, slave-first and master-second zero-loss benchmarks, exact
  `disabled` teardown, and a separate trace-backed simulated drill.
- [ ] All enabled peripherals run together for at least 10 minutes without reset or watchdog event.

The clean-board CLI does not provision WiFi credentials. Record WiFi/TCP as blocked unless the
board was provisioned through a supported firmware workflow and the credential source is recorded.

## 9. Programming, OTA, And Diagnostics

Initial programming must install the bootloader, partition table, OTA metadata, and application.
The `idf.py flash` path in section 3 does this. A tagged release's `domes-<tag>.bin` is only an OTA
application image; use the matching `domes-<tag>-factory.bin` at address `0x0` (or Software CI's
unversioned `domes-factory.bin`) when validating the merged factory artifact described in
[`../../firmware/README.md`](../../firmware/README.md).

- [ ] A blank or erased board boots after `idf.py flash` or the matching merged factory image.
- [ ] `system info`, health, and self-test pass after initial programming.

Build a retained OTA application with a fresh configuration, then extract the version embedded in
that exact image. Retain the exact ELF for the image already running before each OTA and record its
version and boot count. The declared value must be parser-valid, at most 31 ASCII bytes, and
byte-for-byte identical to the embedded value. Exercise serial and BLE on separate test targets, or
restore the baseline between flows, so each transport actually changes the selected app partition:

```bash
CLI=tools/domes-cli/target/debug/domes-cli
BASELINE_ELF='<exact domes.elf for the image currently running on this board>'
BASELINE_INFO=$($CLI --port "$PORT" system info)
BASELINE_VERSION=$(awk '/Firmware:/ {print $2; exit}' <<< "$BASELINE_INFO")
BASELINE_BOOT_COUNT=$(awk '/Boot count:/ {print $3; exit}' <<< "$BASELINE_INFO")
test -f "$BASELINE_ELF"
OTA_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$OTA_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$OTA_ROOT/sdkconfig" build)
OTA_BIN="$OTA_ROOT/build/domes.bin"
EXPECTED_VERSION=$(
  . ~/esp/esp-idf/export.sh >/dev/null 2>&1
  python -m esptool image_info --version 2 "$OTA_BIN" |
    sed -n 's/^App version: //p'
)
test -n "$EXPECTED_VERSION"

$CLI --port "$PORT" ota flash "$OTA_BIN" \
  --version "$EXPECTED_VERSION"
sleep 15
$CLI --port "$PORT" system info
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test
tools/firmware/verify_restart_snapshot.sh \
  "$PORT" "$BASELINE_BOOT_COUNT" "$BASELINE_VERSION" "$BASELINE_ELF" "$CLI"
$CLI --port "$PORT" system crash-dump --clear

# Reset once more and repeat the checks to confirm the image was accepted.
. ~/esp/esp-idf/export.sh
python -m esptool --chip esp32s3 --port "$PORT" run
sleep 10
$CLI --port "$PORT" system info
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test

# Repeat the complete acceptance sequence on the BLE test target.
BLE_TARGET='<BLE address from devices scan>'
BLE_PORT='<matching CP2102N serial path>'
BLE_BASELINE_ELF='<exact domes.elf currently running on the BLE test target>'
BLE_BASELINE_INFO=$($CLI --port "$BLE_PORT" system info)
BLE_BASELINE_VERSION=$(awk '/Firmware:/ {print $2; exit}' <<< "$BLE_BASELINE_INFO")
BLE_BASELINE_BOOT_COUNT=$(awk '/Boot count:/ {print $3; exit}' <<< "$BLE_BASELINE_INFO")
test -f "$BLE_BASELINE_ELF"
$CLI --ble "$BLE_TARGET" ota flash "$OTA_BIN" \
  --version "$EXPECTED_VERSION"
sleep 15
$CLI --port "$BLE_PORT" system info
$CLI --port "$BLE_PORT" system health
$CLI --port "$BLE_PORT" system self-test
$CLI --ble "$BLE_TARGET" system info
tools/firmware/verify_restart_snapshot.sh \
  "$BLE_PORT" "$BLE_BASELINE_BOOT_COUNT" "$BLE_BASELINE_VERSION" \
  "$BLE_BASELINE_ELF" "$CLI"
$CLI --port "$BLE_PORT" system crash-dump --clear

python -m esptool --chip esp32s3 --port "$BLE_PORT" run
sleep 10
$CLI --port "$BLE_PORT" system info
$CLI --port "$BLE_PORT" system health
$CLI --port "$BLE_PORT" system self-test
$CLI --ble "$BLE_TARGET" system info
```

- [ ] Serial OTA reports success and the expected version survives the second boot.
- [ ] BLE OTA reports success, reconnects over BLE and UART, and survives the second boot.
- [ ] A truncated serial image is rejected without changing the running image.
- [ ] An interrupted serial transfer times out cleanly and a subsequent command/update succeeds.
- [ ] BLE abort/interruption recovery is exercised separately; serial recovery is not evidence for
  the BLE session path.
- [ ] Forced failed-self-test rollback was tested with a purpose-built image, or is explicitly
  recorded as unverified. A successful normal OTA does not exercise this path.
- [ ] The first post-OTA boot exposes a format-2 clean-restart snapshot whose CRC-protected boot
  count, firmware version, internal heap, processed PCs, and ELF SHA match the exact pre-OTA image.
- [ ] An unreadable or unsupported snapshot fails closed, and an explicit
  `system crash-dump --clear` restores the no-record state. Use a controlled malformed-record fixture;
  do not corrupt NVS on a board with calibration or identity data that has not been backed up.

Bounded serial rejection and interruption checks mirror the hardware workflow:

```bash
head -c 4096 "$OTA_BIN" > "$OTA_ROOT/truncated.bin"
if $CLI --port "$PORT" ota flash "$OTA_ROOT/truncated.bin" \
  --version "$EXPECTED_VERSION"; then
  echo 'ERROR: truncated image was accepted' >&2
  false
fi
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test

set +e
timeout 2s $CLI --port "$PORT" ota flash "$OTA_BIN" \
  --version "$EXPECTED_VERSION"
INTERRUPT_STATUS=$?
set -e
if [[ "$INTERRUPT_STATUS" -ne 124 && "$INTERRUPT_STATUS" -ne 143 ]]; then
  echo "ERROR: interruption returned status $INTERRUPT_STATUS" >&2
  false
fi
sleep 20
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test
```

Use the purpose-built rollback image and assertions in
[`../../.github/workflows/firmware-hw-test.yml`](../../.github/workflows/firmware-hw-test.yml) for
the destructive rollback gate. Do not improvise a failing release image.

Use the complete
[`$domes-esp32-firmware` ESP-NOW integration runbook](../../.codex/skills/domes-esp32-firmware/references/runbooks.md#esp-now-integration-test)
for the two-pod gate. `stopping` is not a ready state; wait for exact `disabled` before every new
lifecycle.

Capture a bounded trace and confirm that the exported JSON opens in Perfetto:

```bash
$CLI --port "$PORT" trace clear
$CLI --port "$PORT" trace start
$CLI --port "$PORT" trace status
$CLI --port "$PORT" system health
$CLI --port "$PORT" trace stop
$CLI --port "$PORT" trace dump \
  --output /tmp/domes-trace.json --names tools/trace/trace_names.json
```

- [ ] Trace start/status/stop/dump succeeds and the output contains recorded events.
- [ ] Two-pod traces can be grouped with `trace_merge.py --align zero`; record them as separate local
  timelines, not cross-clock timing evidence.
- [ ] If panic diagnostics are in scope, an intentional panic dump is retrieved and decoded with
  the exact matching `domes.elf`. The clean-restart CLI snapshot is not equivalent.

## Results

| Area | Pass/Fail | Evidence or notes |
| --- | --- | --- |
| Power | | |
| Serial/system | | |
| LED ring | | |
| I2C/IMU | | |
| Haptic | | |
| Touch | | |
| Audio | | |
| BLE/WiFi | | |
| ESP-NOW | | |
| Stability | | |
| Initial programming | | |
| Serial/BLE OTA | | |
| Invalid/interrupted OTA | | |
| Forced rollback | | |
| Trace/coredump diagnostics | | |

Update [`../../firmware/MILESTONES.md`](../../firmware/MILESTONES.md) only when recorded evidence
changes the project-wide verification status.
