---
description: Test touch pad sensing and simulated touch injection
argument-hint: [port]
allowed-tools: Bash, Read, Glob
---

# Touch Pad Test

Test capacitive touch pads and simulated touch injection on NFF devboards.

## Arguments

- `$1` - Serial port (optional, default: `/dev/ttyACM0`)

## Instructions

1. **Set CLI and port**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   PORT="${1:-/dev/ttyACM0}"
   ```

2. **Read touch pad baselines from serial**:
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py $PORT 6 2>&1 | grep -i "TouchService\|touch"
   ```
   - Should show 4 pads with baseline readings and thresholds
   - Example: `Pad 0: raw=21707, threshold=22792, touched=0`
   - `touched=0` is normal when nothing is touching the pad
   - **FAIL** if no touch data appears (touch driver not initialized)

3. **Verify touch threshold calibration**:
   - Each pad should have: `threshold ≈ baseline * 1.05` (5% above baseline)
   - Baseline values vary by pad and environment (20000-28000 typical for NFF board)
   - **WARN** if baseline < 10000 or > 40000 (possible hardware issue)

4. **Test simulated touch injection** (all 4 pads):
   ```bash
   for pad in 0 1 2 3; do
     echo "Injecting touch on pad $pad..."
     $CLI --port $PORT touch simulate --pad $pad
     sleep 0.5
   done
   ```
   - **PASS**: Each pad reports "Injected touch on pad N"
   - **FAIL**: Timeout or error response

5. **Verify injection triggers game engine** — monitor serial during injection:
   ```bash
   # Start monitoring in background-ish, then inject
   $CLI --port $PORT touch simulate --pad 0
   sleep 0.5
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py $PORT 3 2>&1 | grep -i -E "game|hit|touch|triage|mode"
   ```
   - In IDLE/TRIAGE mode: touch should trigger mode transition to TRIAGE
   - In GAME mode (ESP-NOW armed): touch triggers HIT event

6. **Report**:
   ```
   TOUCH TEST: $PORT
   =================
   Pad 0 (GPIO1):  baseline=XXXXX  threshold=XXXXX  inject=PASS
   Pad 1 (GPIO2):  baseline=XXXXX  threshold=XXXXX  inject=PASS
   Pad 2 (GPIO4):  baseline=XXXXX  threshold=XXXXX  inject=PASS
   Pad 3 (GPIO6):  baseline=XXXXX  threshold=XXXXX  inject=PASS

   Simulated injection: PASS (all 4 pads)
   Game engine trigger:  PASS/UNTESTED
   ```

## Touch Pad GPIO Mapping (NFF Devboard)

| Pad | GPIO | Touch Channel |
|-----|------|---------------|
| 0   | 1    | 1             |
| 1   | 2    | 2             |
| 2   | 4    | 4             |
| 3   | 6    | 6             |

## Notes

- Touch calibration runs at boot (captures 10 samples per pad)
- Threshold = baseline + 5% (configurable in touchDriver.hpp)
- Physical touch increases capacitance → raw value goes UP past threshold
- InjectableTouchDriver wraps the hardware driver — injection lasts 10 ticks (~100ms at 100Hz)
- Injection via CLI uses config protocol message `SIMULATE_TOUCH` (0x4C)
