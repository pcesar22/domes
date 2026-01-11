#!/bin/bash
# Serial OTA end-to-end test
# Usage: ./test-serial-ota.sh [port]

set -e

PORT="${1:-/dev/ttyACM0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Serial OTA Test ==="
echo "Port: $PORT"
echo ""

# Check device
if [ ! -e "$PORT" ]; then
    echo "ERROR: Device not found at $PORT"
    exit 1
fi

# Build firmware
echo ">>> Building firmware..."
cd "$SCRIPT_DIR/domes"
. ~/esp/esp-idf/export.sh > /dev/null 2>&1
idf.py build > /dev/null 2>&1
echo "    Firmware built: $(stat -c%s build/domes.bin) bytes"

# Flash initial firmware
echo ">>> Flashing firmware..."
idf.py -p "$PORT" flash > /dev/null 2>&1
sleep 3

# Build host tool
echo ">>> Building OTA sender..."
cd "$SCRIPT_DIR/host"
cmake -B build > /dev/null 2>&1
cmake --build build --target simple_ota_sender > /dev/null 2>&1

# Run OTA
echo ">>> Running Serial OTA transfer..."
./build/simple_ota_sender "$PORT" "$SCRIPT_DIR/domes/build/domes.bin" v0.0.0-test 2>&1 | tail -5

# Verify boot - wait for USB to re-enumerate after OTA reboot
echo ">>> Verifying device boot..."
sleep 2

# Wait for port to reappear
for i in {1..10}; do
    if [ -e "$PORT" ]; then
        break
    fi
    sleep 1
done

if [ ! -e "$PORT" ]; then
    echo "    ERROR: Port $PORT did not reappear after reboot"
    exit 1
fi

sleep 1

python3 -c "
import serial
import time
import sys

# Reset device to capture full boot
port = serial.Serial('$PORT', 115200, timeout=0.5)
port.dtr = False
port.rts = True
time.sleep(0.1)
port.rts = False
time.sleep(0.5)
port.reset_input_buffer()

output = ''
start = time.time()
while time.time() - start < 12:
    data = port.read(1024)
    if data:
        output += data.decode('utf-8', errors='replace')
port.close()

if 'domes' in output.lower() and ('Firmware version' in output or 'Initialization' in output):
    print('    Boot verified OK')
    sys.exit(0)
else:
    print('    Boot verification FAILED')
    print('    Looking for: DOMES Firmware, Initialization complete')
    print('    Output lines:')
    for line in output.split('\n')[:25]:
        print('    ' + line[:100])
    sys.exit(1)
"

echo ""
echo "=== PASS: Serial OTA test completed ==="
