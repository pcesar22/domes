---
description: Build, flash, and monitor ESP32 firmware in one command
argument-hint: [project-path] [port(s)]
allowed-tools: Bash, Read, Glob
---

# Flash ESP32 Firmware

Build, flash, and monitor the ESP32 firmware project.

## Arguments

- `$1` - Project path (optional, defaults to current firmware project)
- `$2` - Serial port(s) (optional, defaults to auto-detect)
  - Single: `/dev/ttyACM0`
  - Multiple: `/dev/ttyACM0,/dev/ttyACM1`
  - All: `all` (auto-detect all /dev/ttyACM* devices)

## Instructions

1. Determine the project path:
   - If `$1` is provided, use it
   - Otherwise, look for `firmware/domes/` or `firmware/hello_world/` in the repo root
   - Verify the path contains a valid ESP-IDF project (CMakeLists.txt with `project()`)

2. Determine the serial port(s):
   - If `$2` is "all", auto-detect: `ls /dev/ttyACM* 2>/dev/null`
   - If `$2` contains commas, split into list
   - If `$2` is a single port, use it
   - Otherwise, default to `/dev/ttyACM0`
   - Verify each port exists with `ls -la $PORT`

3. Source the ESP-IDF environment:
   ```bash
   . ~/esp/esp-idf/export.sh
   ```

4. Build the firmware (once):
   ```bash
   cd $PROJECT_PATH && idf.py build
   ```

5. Flash each device:
   ```bash
   for PORT in $PORTS; do
     echo "=== Flashing $PORT ==="
     idf.py -p $PORT flash
   done
   ```

6. After flashing all devices, monitor serial output for 15 seconds.
   For single device:
   ```bash
   python3 -c "
   import serial, time
   ser = serial.Serial('$PORT', 115200, timeout=1)
   start = time.time()
   while time.time() - start < 15:
       if ser.in_waiting:
           print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), end='', flush=True)
       time.sleep(0.05)
   ser.close()
   "
   ```

   For multiple devices, use the multi-device monitor script:
   ```bash
   python3 .claude/skills/esp32-firmware/scripts/monitor_serial.py "$PORTS" 15
   ```

7. Report results:
   - Build success/failure with any errors
   - Flash success/failure per device
   - Key output from serial monitor (boot messages, app output)

## Error Handling

- If build fails, show the error and suggest fixes based on common issues
- If flash fails on any device, report which device(s) failed and suggest:
  check USB cable, hold BOOT button, verify port permissions
- If no serial output, check `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in sdkconfig

## Multi-Device Examples

```bash
# Flash single device (default)
/flash

# Flash specific port
/flash firmware/domes /dev/ttyACM1

# Flash two devices
/flash firmware/domes /dev/ttyACM0,/dev/ttyACM1

# Flash all connected devices
/flash firmware/domes all
```
