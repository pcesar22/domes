---
description: Erase flash and reflash one or both devices (clean slate)
argument-hint: [port(s)]
allowed-tools: Bash, Read, Glob
---

# Erase Flash and Reflash

Erase all flash (including NVS) and reflash firmware for a clean start. Use when devices have stale state causing issues (ESP-NOW stuck, bad calibration, etc.).

## Arguments

- `$1` - Port(s) (optional, default: auto-detect)
  - Single: `/dev/ttyACM0`
  - Multiple: `/dev/ttyACM0,/dev/ttyACM1` or `all`

## Instructions

1. **Determine ports**: auto-detect if not specified.

2. **Source ESP-IDF**:
   ```bash
   . ~/esp/esp-idf/export.sh
   ```

3. **Find firmware project** (default: `firmware/domes/`):
   - Look for `firmware/domes/` in repo root

4. **Build firmware** (once):
   ```bash
   cd $PROJECT_PATH && idf.py build
   ```

5. **For each port — erase then flash**:
   ```bash
   for PORT in $PORTS; do
     echo "=== Erasing $PORT ==="
     idf.py -p $PORT erase-flash

     echo "=== Flashing $PORT ==="
     idf.py -p $PORT flash
   done
   ```

6. **Wait for boot** (10 seconds):
   ```bash
   sleep 10
   ```

7. **Verify each device**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   for PORT in $PORTS; do
     echo "=== Verify $PORT ==="
     $CLI --port $PORT feature list
   done
   ```

8. **Report**:
   - Erase success/failure per device
   - Flash success/failure per device
   - Feature list response (confirms device booted and protocol works)

## When to Use

- ESP-NOW discovery stuck (RX=0, peers=0)
- Touch calibration seems wrong
- Boot counter or NVS data corrupted
- After firmware struct layout changes (NVS format mismatch)
- "Clean slate" before M7 validation

## Warnings

- **Erases ALL NVS data**: pod_id, boot counter, feature defaults — all reset
- **Erases OTA partitions**: device reverts to factory app
- After erase+flash, you may need to re-set pod_id: `domes-cli system set-pod-id N`
