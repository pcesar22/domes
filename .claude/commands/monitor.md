---
description: Monitor serial output from one or both devices
argument-hint: [port(s)] [duration]
allowed-tools: Bash, Read, Glob
---

# Serial Monitor

Monitor serial output from one or both ESP32 devices with colored labels.

## Arguments

- `$1` - Port(s) (optional, default: auto-detect all `/dev/ttyACM*`)
  - Single: `/dev/ttyACM0`
  - Multiple: `/dev/ttyACM0,/dev/ttyACM1` or `all`
- `$2` - Duration in seconds (optional, default: 15)

## Instructions

1. **Determine ports**:
   - If `$1` is "all" or empty: `ls /dev/ttyACM* 2>/dev/null`
   - Join detected ports with commas

2. **Set duration**: default 15 seconds, max 120 seconds

3. **Run monitor**:
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS_CSV" $DURATION
   ```

4. **Report key observations**:
   - Boot messages (firmware version, init sequence)
   - Error/warning messages (ESP_LOGE, ESP_LOGW)
   - IMU readings (magnitude, orientation)
   - Touch pad data (raw values, thresholds)
   - ESP-NOW activity (beacons, discovery, drill rounds)
   - Mode transitions (IDLE, TRIAGE, GAME)
   - Memory/heap status (diagnostics output)

## Filtered Monitoring

For targeted monitoring, pipe through grep:

```bash
# ESP-NOW only
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 20 2>&1 | grep -i "espnow\|esp-now\|beacon\|discover"

# Errors and warnings only
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 15 2>&1 | grep -E "\[0;31m|\[0;33m"

# Game events
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 30 2>&1 | grep -i "game\|arm\|hit\|miss\|drill\|round"

# Mode transitions
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 20 2>&1 | grep -i "mode_mgr\|mode:"

# Touch events
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 10 2>&1 | grep -i "touch\|pad"

# Memory/heap
python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 10 2>&1 | grep -i "heap\|mem\|diag"
```

## Notes

- Multi-device output is color-coded: dev0=cyan, dev1=yellow
- Baud rate: 115200 (USB Serial/JTAG console)
- The monitor script reads from serial ports directly — no `idf.py monitor` needed (no TTY required)
- After `initSerialOta()`, USB-CDC is shared between logs and config protocol — some log lines may be interleaved with binary protocol data
