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
cd /path/to/domes
. ~/esp/esp-idf/export.sh
cargo build --manifest-path tools/domes-cli/Cargo.toml
tools/firmware/flash_and_verify.sh \
  firmware/domes /dev/ttyACM0 "DOMES"
```

The helper builds and flashes the firmware, captures the boot log, and checks for the expected
marker. Continue from the repository root:

```bash
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 system info
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 system self-test
tools/domes-cli/target/debug/domes-cli --port /dev/ttyACM0 feature list
```

- [ ] Boot log reports NFF LED mapping: GPIO16, 16 devices, RGBW disabled.
- [ ] System info and feature list return over serial.
- [ ] On-device self-test returns without transport or storage failure.

## 4. LED Ring

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --port /dev/ttyACM0 led solid --color ff0000
$CLI --port /dev/ttyACM0 led solid --color 00ff00
$CLI --port /dev/ttyACM0 led solid --color 0000ff
$CLI --port /dev/ttyACM0 led solid --color ffffff
$CLI --port /dev/ttyACM0 led cycle --period 1000
$CLI --port /dev/ttyACM0 led off
```

- [ ] All 16 devices respond in order.
- [ ] Red, green, blue, and mixed white are correct in RGB mode.
- [ ] Brightness is uniform with no flicker or dead device.

If the ring is dark, inspect the GPIO16 `LED_DATA_3V3` path, level shifter, 5 V supply, and first LED.

## 5. I2C, IMU, And Haptic

Current expected addresses are LIS2DW12 `0x19` and DRV2605L `0x5A` on GPIO8/GPIO9.

- [ ] Both devices acknowledge.
- [ ] Accelerometer readings change with orientation.
- [ ] Tap detection asserts through IMU INT1 on GPIO5.
- [ ] The haptic driver identifies successfully.
- [ ] Fixed 235 Hz LRA drive and at least one effect are physically verified.

The NFF firmware profile is bounded for the schematic's 1.8 Vrms, 235 Hz LD0832AA-0099F. Do not run
haptic effects if a different actuator is populated until its profile is reviewed. The current driver
uses fixed-frequency open-loop operation and does not expose auto-calibration. An I2C ACK alone is
not a haptic pass.

## 6. Touch

Touch pads K1-K4 map to GPIO1, GPIO2, GPIO4, and GPIO6.

- [ ] Each untouched baseline is stable.
- [ ] Each physical pad triggers only its expected logical pad.
- [ ] Repeated touches do not cause stuck or cross-triggered input.

Use `domes-cli touch simulate --pad N` only to verify downstream logic; it does not validate the
physical pad or touch peripheral.

## 7. Audio

Current mapping is BCLK GPIO12, LRCLK GPIO11, data GPIO13, and shutdown GPIO7.

- [ ] Amplifier shutdown control works.
- [ ] The built-in sample is audible.
- [ ] Output is free of obvious clipping at moderate level.
- [ ] Speaker and amplifier remain within a safe temperature.

## 8. Connectivity And Stability

- [ ] BLE advertisement is discoverable and `domes-cli --ble ... system info` succeeds.
- [ ] WiFi/TCP feature listing succeeds when credentials are configured.
- [ ] With a second pod, ESP-NOW discovers one peer and exchanges packets.
- [ ] All enabled peripherals run together for at least 10 minutes without reset or watchdog event.

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

Update [`../../firmware/MILESTONES.md`](../../firmware/MILESTONES.md) only when recorded evidence
changes the project-wide verification status.
