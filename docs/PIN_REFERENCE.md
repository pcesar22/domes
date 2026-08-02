# GPIO Pin Reference

This document describes GPIO values used by the current firmware and separates them from carrier
header positions and planned production assignments.

## Ownership And Validation

- `firmware/domes/main/config.hpp` is authoritative for the sole supported NFF board profile.
- `hardware/nff-devboard/source/*.epro` and `hardware/nff-devboard/docs/schematic.pdf` are
  authoritative for physical NFF carrier nets.
- This document reconciles those sources for humans and must change with either source.

The original NFF schematic labels connections by DevKit header position. Those numbers are not
ESP32 GPIO numbers. For example, `LED_DATA_3V3` is on H1 pin 9, which maps to ESP32 GPIO16 on the
plugged-in DevKit.

Last reconciled with `config.hpp`: 2026-08-02.

## Active Firmware Target

The NFF carrier with an ESP32-S3 N8R8 module is the only supported firmware target. Its pin map is
compiled directly in `firmware/domes/main/config.hpp`.

| Function | DevKit header | ESP32 GPIO | Firmware symbol | Notes |
| --- | --- | --- | --- | --- |
| CP2102N UART TX | DevKit onboard bridge | 43 | `pins::kUartTx` | UART0 protocol output; console remains on native USB |
| CP2102N UART RX | DevKit onboard bridge | 44 | `pins::kUartRx` | UART0 protocol input; must be explicitly routed by firmware |
| LED ring data | H1 pin 9 | 16 | `pins::kLedData` | 16 SK6812MINI-E devices through level shifter; firmware uses RGB mode |
| I2C SDA | - | 8 | `pins::kI2cSda` | LIS2DW12 and DRV2605L bus |
| I2C SCL | - | 9 | `pins::kI2cScl` | LIS2DW12 and DRV2605L bus |
| IMU INT1 | H1 pin 5 | 5 | `pins::kImuInt1` | LIS2DW12 interrupt |
| I2S BCLK | H1 pin 18 | 12 | `pins::kI2sBclk` | MAX98357A bit clock |
| I2S LRCLK | H1 pin 17 | 11 | `pins::kI2sLrclk` | MAX98357A word select |
| I2S data out | H1 pin 19 | 13 | `pins::kI2sDout` | ESP32 output to MAX98357A DIN |
| Audio shutdown | H1 pin 7 | 7 | `pins::kAudioSd` | High or floating enables amplifier |
| Touch pad 0 / K1 | - | 1 | `pins::kTouch1` | Capacitive input |
| Touch pad 1 / K2 | - | 2 | `pins::kTouch2` | Capacitive input |
| Touch pad 2 / K3 | - | 4 | `pins::kTouch3` | Capacitive input |
| Touch pad 3 / K4 | - | 6 | `pins::kTouch4` | Capacitive input |

Current I2C addresses:

| Device | Address | Source |
| --- | --- | --- |
| LIS2DW12 | `0x19` | SA0 tied high on current NFF carrier and `pins::kLis2dw12Addr` |
| DRV2605L | `0x5A` | Fixed address and `pins::kDrv2605lAddr` |

## Platform Comparison

| Function | DevKitC-1 v1.0 | DevKitC-1 v1.1 | NFF carrier + DevKit | Production PCB (planned) |
| --- | --- | --- | --- | --- |
| LED data | GPIO48 | GPIO38 | GPIO16 | GPIO14 |
| LED count/mode | 1 RGB | 1 RGB | 16, firmware RGB mode | 16 RGBW planned |
| I2C SDA / SCL | GPIO8 / 9 test pins | GPIO8 / 9 test pins | GPIO8 / 9 | GPIO8 / 9 |
| IMU INT1 | Not fitted | Not fitted | GPIO5 | Not yet finalized in hardware |
| I2S BCLK / LRCLK / data | GPIO12 / 11 / 13 test pins | GPIO12 / 11 / 13 test pins | GPIO12 / 11 / 13 | GPIO12 / 11 / 13 planned |
| Audio shutdown | Not fitted | Not fitted | GPIO7 | GPIO7 planned |
| Touch | GPIO1, 2, 3, 4 test inputs | GPIO1, 2, 3, 4 test inputs | GPIO1, 2, 4, 6 | GPIO1, 2, 3, 4 planned |

The standalone DevKit and production columns are historical or planned values, not profiles in the
current firmware. Production values are targets only and are not verified PCB routing; the active
8 MB partition/config profile also does not support the planned 16 MB production target.

## Board Profile In Code

There is no board-selection define or `CONFIG_DOMES_PLATFORM_*` Kconfig selector. Adding another
target requires a complete profile, an explicit selection mechanism, and build and hardware tests.
Do not change isolated pin constants and treat the result as a supported board.

## LED Mode Note

The NFF BOM identifies SK6812MINI-E parts as RGBW-capable, but current bring-up found that this board
must be driven in RGB/WS2812 mode. `pins::kLedIsRgbw` is therefore `false` for
the active NFF profile. Do not describe the firmware as using the white channel unless a
hardware test changes that setting and validates all 16 devices.

## Troubleshooting

### LED Ring Does Not Respond

1. Confirm the firmware was built from the NFF profile in `config.hpp`.
2. Confirm the boot log reports GPIO16, 16 devices, and RGBW disabled.
3. Check the `LED_DATA_3V3` to `LED_DATA_5V` level-shifter path on the carrier.
4. Verify the LED supply and the first device in the chain.

### I2C Device Does Not Respond

1. Check GPIO8/GPIO9 and carrier pull-ups.
2. Expect LIS2DW12 at `0x19` and DRV2605L at `0x5A` on the current board.
3. Treat an address or interrupt mismatch as a schematic/config defect, not a documentation-only
   workaround.

### Audio Does Not Respond

1. Confirm BCLK GPIO12, LRCLK GPIO11, and data GPIO13.
2. Confirm audio shutdown GPIO7 is high or floating.
3. Verify the connected speaker is appropriate for the MAX98357A output.

## References

- [NFF board README](../hardware/nff-devboard/README.md)
- [NFF bring-up checklist](../hardware/nff-devboard/BRING_UP_CHECKLIST.md)
- [NFF schematic](../hardware/nff-devboard/docs/schematic.pdf)
- [ESP32-S3-DevKitC-1 guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html)
