---
description: Test LIS2DW12 IMU accelerometer and tap detection
argument-hint: [port]
allowed-tools: Bash, Read, Glob
---

# IMU (LIS2DW12) Test

Test the LIS2DW12 accelerometer and tap detection on NFF devboards.

## Arguments

- `$1` - Serial port (optional, default: `/dev/ttyACM0`)

## Instructions

1. **Set CLI and port**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   PORT="${1:-/dev/ttyACM0}"
   ```

2. **Verify IMU initialization** from serial logs:
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py $PORT 5 2>&1 | grep -i -E "imu|lis2dw12|accel|tap|mag"
   ```
   - Should show periodic readings: `mag=X.XXg X=X.XX Y=X.XX Z=X.XX`
   - Stationary board should read ~1.00g magnitude (gravity)
   - X≈0, Y≈0, Z≈1.0 for a board flat on a table
   - **PASS**: IMU data streaming at ~5Hz (every 200ms)
   - **FAIL**: No IMU data → check I2C bus (SDA=GPIO8, SCL=GPIO9)

3. **Verify accelerometer accuracy**:
   - Magnitude should be 0.95-1.05g when stationary
   - Z axis should dominate (~0.97-1.00) when board is flat
   - **WARN** if magnitude < 0.8g or > 1.2g (sensor miscalibrated or hardware issue)

4. **Test tap detection** (requires physical interaction):
   - Tell the user: "Tap the board firmly and watch serial output"
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py $PORT 10 2>&1 | grep -i -E "tap|triage|mode"
   ```
   - **PASS**: "tap detected" or mode transition IDLE→TRIAGE on tap
   - **NOTE**: Tap detection may not trigger if board is in TRIAGE mode already

5. **Report**:
   ```
   IMU TEST: $PORT
   ===============
   LIS2DW12 detected:    YES (address 0x19)
   Data streaming:       PASS (5Hz readings)
   Magnitude accuracy:   X.XXg (expected ~1.00g)
   Orientation:          X=X.XX Y=X.XX Z=X.XX
   Tap detection:        PASS/UNTESTED (requires physical tap)
   ```

## IMU Hardware Details

| Parameter | Value |
|-----------|-------|
| Chip | LIS2DW12 |
| I2C Address | 0x19 |
| I2C Bus | SDA=GPIO8, SCL=GPIO9 |
| INT1 Pin | GPIO5 (active low) |
| Sample Rate | 100Hz |
| Full Scale | ±2g |
| Mode | High-performance |
| Tap Detection | Single tap enabled |

## Notes

- IMU service runs as a FreeRTOS task, logs every 200ms
- Tap detection uses LIS2DW12 hardware interrupt (INT1)
- When tap detected in IDLE mode → triggers TRIAGE mode (LED feedback)
- IMU service can connect to haptic driver (buzz on tap) and audio service (beep on tap)
