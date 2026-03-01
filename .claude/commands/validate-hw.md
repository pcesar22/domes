---
description: Run full NFF devboard hardware validation suite
argument-hint: [port(s)]
allowed-tools: Bash, Read, Glob
---

# NFF Devboard Hardware Validation

Run all hardware checks on one or both NFF devboards. Reports pass/fail per subsystem.

## Arguments

- `$1` - Serial port(s) (optional)
  - Single: `/dev/ttyACM0`
  - Multiple: `/dev/ttyACM0,/dev/ttyACM1` or `all`
  - Default: auto-detect all `/dev/ttyACM*`

## Instructions

1. **Determine ports**:
   - If `$1` is "all" or empty, auto-detect: `ls /dev/ttyACM* 2>/dev/null`
   - If `$1` contains commas, split into list
   - Verify each port exists

2. **Set CLI path**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   ```
   If binary doesn't exist, build it first: `cd tools/domes-cli && cargo build`

3. **Wait for device readiness** (8 seconds after flash, or check immediately if devices already booted).

4. **Run checks IN THIS ORDER for each device**:

   ### Check 1: Self-Test Suite
   ```bash
   $CLI --port $PORT system self-test
   ```
   - **PASS**: All 5 tests pass (NVS, Heap, Flash, WiFi, BLE)
   - **FAIL**: Any test fails — report which one and the error message

   ### Check 2: System Info
   ```bash
   $CLI --port $PORT system info
   ```
   - Report: firmware version, pod ID, mode, uptime, free heap, features bitmask
   - **WARN** if pod_id is "not set" — suggest `system set-pod-id`
   - **WARN** if free heap < 30KB

   ### Check 3: Feature List
   ```bash
   $CLI --port $PORT feature list
   ```
   - **PASS**: All 7 features respond (led-effects, ble, wifi, esp-now, touch, haptic, audio)
   - **FAIL**: Timeout or missing features

   ### Check 4: LED Ring (all 16 LEDs)
   ```bash
   $CLI --port $PORT led solid --color ff0000   # Red
   sleep 1
   $CLI --port $PORT led solid --color 00ff00   # Green
   sleep 1
   $CLI --port $PORT led solid --color 0000ff   # Blue
   sleep 1
   $CLI --port $PORT led solid --color ffffff   # White
   sleep 1
   $CLI --port $PORT led cycle                  # Color cycle
   sleep 2
   $CLI --port $PORT led off
   ```
   - **PASS**: All commands return "LED pattern set" without errors
   - **NOTE**: Cannot verify visual output — tell user to visually confirm all 16 LEDs lit for each color

   ### Check 5: Touch Pad Simulation
   ```bash
   for pad in 0 1 2 3; do
     $CLI --port $PORT touch simulate --pad $pad
   done
   ```
   - **PASS**: All 4 pads report "Injected touch on pad N"
   - **FAIL**: Timeout or error response

   ### Check 6: BLE Scan
   ```bash
   $CLI --scan-ble
   ```
   - **PASS**: Device appears in BLE scan results (DOMES-Pod-XXXX)
   - **FAIL**: Device not found after 10s scan

   ### Check 7: BLE Transport
   Pick one device from the BLE scan:
   ```bash
   $CLI --ble "DOMES-Pod-XXXX" feature list
   ```
   - **PASS**: Feature list returned via BLE
   - **FAIL**: Connection timeout or no response

5. **Multi-device checks** (only if 2+ ports):

   ### Check 8: ESP-NOW Discovery
   ```bash
   # Disable WiFi on both (WiFi blocks ESP-NOW discovery)
   for PORT in $PORTS; do $CLI --port $PORT feature disable wifi; done
   sleep 1

   # Enable ESP-NOW on both
   for PORT in $PORTS; do $CLI --port $PORT feature enable esp-now; done

   # Wait for discovery (25 seconds)
   sleep 25

   # Check status
   for PORT in $PORTS; do $CLI --port $PORT espnow status; done
   ```
   - **PASS**: Both show peers=1, one is master, one is slave, RX > 0
   - **FAIL**: peers=0 or RX=0 after 25 seconds

   ### Check 9: ESP-NOW Drill Execution
   Monitor serial for 20 seconds after discovery:
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS_CSV" 20
   ```
   Grep for: `Round`, `ARM`, `MISS`, `HIT`, `DRILL COMPLETE`
   - **PASS**: 10 rounds executed, "DRILL COMPLETE" message seen
   - **FAIL**: No rounds or stuck in discovery

6. **Cleanup**: Restore features to defaults:
   ```bash
   for PORT in $PORTS; do
     $CLI --port $PORT feature enable wifi
     $CLI --port $PORT feature disable esp-now
     $CLI --port $PORT led off
   done
   ```

7. **Report summary table**:
   ```
   NFF HARDWARE VALIDATION REPORT
   ==============================
   Device: /dev/ttyACM0 (DOMES-Pod-XXXX)

   CHECK                    STATUS
   ----------------------- --------
   Self-test (5 tests)     PASS
   System info             PASS
   Feature list            PASS
   LED ring                PASS (visual check needed)
   Touch simulation        PASS
   BLE scan                PASS
   BLE transport           PASS
   ESP-NOW discovery       PASS
   ESP-NOW drill           PASS
   ```

## Known Issues

- **ESP-NOW requires WiFi disabled** — always disable WiFi before enabling ESP-NOW
- **Stale NVS can block ESP-NOW** — if discovery fails, try `idf.py -p $PORT erase-flash` then reflash
- **Wait 8+ seconds after flash** before running CLI commands
- **BLE scan takes 10 seconds** — be patient
- **Touch raw values are NOT touch events** — they're capacitive sensor readings; "touched=0" in logs is normal when not physically touching
