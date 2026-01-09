---
description: Build, flash, and monitor ESP32 firmware in one command
argument-hint: [project-path] [port]
allowed-tools: Bash, Read, Glob
---

# Flash ESP32 Firmware

Build, flash, and monitor the ESP32 firmware project.

## Arguments

- `$1` - Project path (optional, defaults to current firmware project)
- `$2` - Serial port (optional, defaults to /dev/ttyACM0)

## Instructions

1. Determine the project path:
   - If `$1` is provided, use it
   - Otherwise, look for `firmware/domes/` or `firmware/hello_world/` in the repo root
   - Verify the path contains a valid ESP-IDF project (CMakeLists.txt with `project()`)

2. Determine the serial port:
   - If `$2` is provided, use it
   - Otherwise, default to `/dev/ttyACM0`
   - Verify the port exists with `ls -la $PORT`

3. Source the ESP-IDF environment:
   ```bash
   . ~/esp/esp-idf/export.sh
   ```

4. Build the firmware:
   ```bash
   cd $PROJECT_PATH && idf.py build
   ```

5. If build succeeds, flash and monitor:
   ```bash
   idf.py -p $PORT flash
   ```

6. After flashing, read serial output for 15 seconds using Python (since idf.py monitor requires TTY):
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

7. Report results:
   - Build success/failure with any errors
   - Flash success/failure
   - Key output from serial monitor (boot messages, app output)

## Error Handling

- If build fails, show the error and suggest fixes based on common issues
- If flash fails, suggest: check USB cable, hold BOOT button, verify port permissions
- If no serial output, check `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in sdkconfig
