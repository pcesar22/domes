#!/bin/bash
# Flash ESP32 firmware and verify serial output
# Usage: ./flash_and_verify.sh [project_dir] [port(s)] [verify_string]
#
# Multi-device: ./flash_and_verify.sh . /dev/ttyACM0,/dev/ttyACM1 "Hello"

set -e

PROJECT_DIR="${1:-.}"
PORTS="${2:-/dev/ttyACM0}"
VERIFY_STRING="${3:-Hello}"
MONITOR_DURATION=10

echo "=== ESP32 Flash and Verify ==="
echo "Project: $PROJECT_DIR"
echo "Port(s): $PORTS"
echo "Verify string: $VERIFY_STRING"
echo ""

# Source ESP-IDF environment
echo "Sourcing ESP-IDF environment..."
. ~/esp/esp-idf/export.sh

# Navigate to project
cd "$PROJECT_DIR"

# Build (once)
echo ""
echo "=== Building firmware ==="
idf.py build

# Split ports and flash each
IFS=',' read -ra PORT_ARRAY <<< "$PORTS"
TOTAL=${#PORT_ARRAY[@]}
PASSED=0
FAILED=0

for PORT in "${PORT_ARRAY[@]}"; do
    PORT=$(echo "$PORT" | xargs)  # trim whitespace
    echo ""
    echo "=== [$PORT] Flashing firmware ==="

    if ! idf.py -p "$PORT" flash; then
        echo "=== [$PORT] FLASH FAILED ==="
        FAILED=$((FAILED + 1))
        continue
    fi

    # Wait for device to reset
    sleep 2

    # Monitor and capture output
    echo ""
    echo "=== [$PORT] Monitoring serial output (${MONITOR_DURATION}s) ==="
    RESULT=$(python3 -c "
import serial
import time
import sys

ser = serial.Serial('$PORT', 115200, timeout=1)
output = []
start = time.time()
while time.time() - start < $MONITOR_DURATION:
    if ser.in_waiting:
        data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        output.append(data)
        print(data, end='', flush=True)
    time.sleep(0.05)
ser.close()

full_output = ''.join(output)
if '$VERIFY_STRING' in full_output:
    print('VERIFICATION_PASSED', file=sys.stderr)
    sys.exit(0)
else:
    print('VERIFICATION_FAILED', file=sys.stderr)
    sys.exit(1)
" 2>&1) || true

    echo "$RESULT"

    if echo "$RESULT" | grep -q "VERIFICATION_PASSED"; then
        echo ""
        echo "=== [$PORT] SUCCESS: Found '$VERIFY_STRING' in output ==="
        PASSED=$((PASSED + 1))
    else
        echo ""
        echo "=== [$PORT] FAILED: Did not find '$VERIFY_STRING' in output ==="
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Total: $TOTAL | Passed: $PASSED | Failed: $FAILED"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
