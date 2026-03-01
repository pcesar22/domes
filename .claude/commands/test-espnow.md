---
description: Test ESP-NOW discovery and drill execution between two pods
argument-hint: [port1,port2]
allowed-tools: Bash, Read, Glob
---

# ESP-NOW Integration Test

Test ESP-NOW peer discovery and drill game execution between two pods.

## Arguments

- `$1` - Comma-separated ports (optional, default: auto-detect two `/dev/ttyACM*`)

## Prerequisites

- Two ESP32 devices connected via USB
- Firmware already flashed (use `/flash` first)
- Wait 8+ seconds after flash for full boot

## Instructions

1. **Determine ports**:
   - If `$1` provided, split on comma to get PORT1 and PORT2
   - Otherwise: `ls /dev/ttyACM* 2>/dev/null` — need exactly 2 ports
   - If only 1 port found, error: "ESP-NOW requires 2 devices"

2. **Set CLI path**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   ```
   Build if needed: `cd tools/domes-cli && cargo build`

3. **Prepare for ESP-NOW** (WiFi blocks discovery):
   ```bash
   $CLI --port $PORT1 feature disable wifi
   $CLI --port $PORT2 feature disable wifi
   sleep 1
   $CLI --port $PORT1 feature disable esp-now
   $CLI --port $PORT2 feature disable esp-now
   sleep 2
   ```

4. **Enable ESP-NOW on both**:
   ```bash
   $CLI --port $PORT1 feature enable esp-now
   $CLI --port $PORT2 feature enable esp-now
   ```

5. **Wait for discovery** (25 seconds):
   ```bash
   sleep 25
   ```

6. **Check discovery status**:
   ```bash
   $CLI --port $PORT1 espnow status
   $CLI --port $PORT2 espnow status
   ```
   - Both should show `peers: 1`
   - One should be `master`, the other `slave`
   - Both should have `RX packets > 0`
   - If `peers: 0` or `RX: 0`, report FAIL and suggest:
     - Erase flash and reflash both devices
     - Ensure BLE advertising isn't blocking (already widened to 100-200ms in firmware)

7. **Monitor drill execution** (20 seconds):
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORT1,$PORT2" 20 2>&1 | grep -i -E "round|arm|hit|miss|drill|game|espnow"
   ```
   - Expect: 10 rounds of "ARM" → "MISS (timeout)" (no physical touches)
   - Final: "DRILL COMPLETE: 0/10 hits"

8. **Test sim-touch drill** (if `touch simulate` command available):
   ```bash
   # Enable sim mode with 500ms delay (auto-touches during drill)
   $CLI --port $PORT1 espnow sim-mode on --delay-ms 500 --pad 0

   # Wait for drill cycle (discovery + 10 rounds ≈ 40s)
   sleep 45

   # Check status — should show hits
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORT1,$PORT2" 15 2>&1 | grep -i -E "round|hit|miss|drill"

   # Disable sim mode
   $CLI --port $PORT1 espnow sim-mode off
   ```

9. **Cleanup**:
   ```bash
   $CLI --port $PORT1 feature disable esp-now
   $CLI --port $PORT2 feature disable esp-now
   sleep 1
   $CLI --port $PORT1 feature enable wifi
   $CLI --port $PORT2 feature enable wifi
   ```

10. **Report**:
    ```
    ESP-NOW TEST RESULTS
    ====================
    Port 1: $PORT1
    Port 2: $PORT2

    Discovery:     PASS/FAIL (peers found in Xs)
    Roles:         master=$PORT1, slave=$PORT2
    Drill:         PASS/FAIL (X/10 rounds completed)
    Sim drill:     PASS/FAIL/SKIP (X/10 hits with sim mode)
    ```

## Troubleshooting

| Issue | Fix |
|-------|-----|
| RX=0 on both | Erase flash (`idf.py erase-flash`) then reflash both |
| Discovery timeout | Disable WiFi first, disable BLE if still failing |
| "Searching" forever | Feature toggle bug — reflash to reset |
| Drill stuck | Check serial logs for mode transition errors |
| Sim-mode no hits | Verify `touch simulate` command works on each pod first |
