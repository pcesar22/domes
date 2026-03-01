---
description: Test BLE advertising, scanning, and GATT transport
argument-hint: [device-name-or-mac]
allowed-tools: Bash, Read, Glob
---

# BLE Transport Test

Test BLE advertising, device scanning, and GATT-based config transport.

## Arguments

- `$1` - Device name or MAC (optional, default: scan and test all found DOMES devices)

## Prerequisites

- **Native Linux only** — BLE does NOT work in WSL2
- Firmware flashed and booted (wait 8+ seconds)
- Bluetooth adapter enabled: `bluetoothctl power on`

## Instructions

1. **Set CLI path**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   ```

2. **BLE Scan** (discover all advertising DOMES pods):
   ```bash
   $CLI --scan-ble
   ```
   - **PASS**: At least one DOMES-Pod-XXXX device found
   - **FAIL**: No devices found — check BLE is enabled on pod (`feature list` should show `ble: enabled`)
   - Note the device name(s) and MAC address(es)

3. **For each discovered device**, run through the test suite:

   ### Test A: Feature list via BLE
   ```bash
   $CLI --ble "DOMES-Pod-XXXX" feature list
   ```
   - **PASS**: 7 features returned
   - **FAIL**: Connection timeout or empty response

   ### Test B: System info via BLE
   ```bash
   $CLI --ble "DOMES-Pod-XXXX" system info
   ```
   - **PASS**: Firmware version, uptime, heap info returned
   - **FAIL**: Timeout

   ### Test C: LED control via BLE
   ```bash
   $CLI --ble "DOMES-Pod-XXXX" led solid --color ff0000
   sleep 2
   $CLI --ble "DOMES-Pod-XXXX" led off
   ```
   - **PASS**: LED commands accepted, tell user to visually confirm
   - **FAIL**: Command timeout

   ### Test D: Feature toggle via BLE
   ```bash
   $CLI --ble "DOMES-Pod-XXXX" feature disable haptic
   sleep 1
   $CLI --ble "DOMES-Pod-XXXX" feature list   # haptic should show disabled
   sleep 1
   $CLI --ble "DOMES-Pod-XXXX" feature enable haptic
   ```
   - **PASS**: Feature toggled and confirmed
   - **FAIL**: State didn't change

   ### Test E: Connect by MAC address
   Use the MAC from the scan:
   ```bash
   $CLI --ble "94:A9:90:0A:XX:XX" feature list
   ```
   - **PASS**: Same features returned
   - **FAIL**: Connection error

4. **Report**:
   ```
   BLE TEST RESULTS
   ================
   Devices found: 2
     - DOMES-Pod-EA52 (94:A9:90:0A:EA:52)
     - DOMES-Pod-EBC2 (94:A9:90:0A:EB:C2)

   DOMES-Pod-EA52:
     Feature list:    PASS
     System info:     PASS
     LED control:     PASS
     Feature toggle:  PASS
     MAC connect:     PASS

   DOMES-Pod-EBC2:
     Feature list:    PASS
     System info:     PASS
     LED control:     PASS
     Feature toggle:  PASS
     MAC connect:     PASS
   ```

## Troubleshooting

| Issue | Fix |
|-------|-----|
| "No adapter found" | BLE requires native Linux, NOT WSL2. Check `bluetoothctl power on` |
| Scan finds 0 devices | Ensure BLE feature is enabled: `domes-cli --port /dev/ttyACMx feature list` |
| Connection timeout | Try again — BLE has 100-200ms advertising interval. Also try by MAC instead of name |
| "Timeout waiting for response" | Device may be busy. Wait 2s and retry. If persistent, reflash firmware |

## Known BLE Device Names

| Name | MAC | Notes |
|------|-----|-------|
| DOMES-Pod-EA52 | 94:A9:90:0A:EA:52 | NFF devboard 1 |
| DOMES-Pod-EBC2 | 94:A9:90:0A:EB:C2 | NFF devboard 2 |

Names use the last 2 bytes of BT MAC. If `pod_id` is set, name becomes `DOMES-Pod-01` etc.
