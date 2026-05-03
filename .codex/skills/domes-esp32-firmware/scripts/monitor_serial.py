#!/usr/bin/env python3
"""
Serial monitor for ESP32 devices.

Usage (single device):
    python monitor_serial.py /dev/ttyACM0 10

Usage (multi-device):
    python monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 10
"""

import serial
import sys
import time
import threading


# ANSI color codes for device labeling
COLORS = [
    '\033[36m',   # Cyan
    '\033[33m',   # Yellow
    '\033[35m',   # Magenta
    '\033[32m',   # Green
    '\033[34m',   # Blue
    '\033[31m',   # Red
]
RESET = '\033[0m'


def monitor_single(port: str, duration: int, baudrate: int = 115200,
                    label: str = "", color: str = ""):
    """Read serial output from one ESP32 device."""
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        prefix = f"{color}[{label}]{RESET} " if label else ""

        start = time.time()
        line_buffer = ""
        while time.time() - start < duration:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                # Print with device prefix on each line
                for ch in data:
                    if ch == '\n':
                        print(f"{prefix}{line_buffer}")
                        line_buffer = ""
                    else:
                        line_buffer += ch
            time.sleep(0.05)

        # Flush remaining buffer
        if line_buffer:
            print(f"{prefix}{line_buffer}")

        ser.close()
        return 0
    except serial.SerialException as e:
        print(f"Error on {port}: {e}", file=sys.stderr)
        return 1


def monitor(ports_str: str = "/dev/ttyACM0", duration: int = 10, baudrate: int = 115200):
    """Monitor one or more serial ports."""
    ports = [p.strip() for p in ports_str.split(',')]

    if len(ports) == 1:
        # Single device - no labels needed (backward compatible)
        port = ports[0]
        print(f"Monitoring {port} at {baudrate} baud for {duration}s...")
        print("-" * 50)
        try:
            ser = serial.Serial(port, baudrate, timeout=1)
            start = time.time()
            while time.time() - start < duration:
                if ser.in_waiting:
                    data = ser.read(ser.in_waiting)
                    print(data.decode('utf-8', errors='ignore'), end='', flush=True)
                time.sleep(0.05)
            print("\n" + "-" * 50)
            print("Monitoring complete.")
            ser.close()
            return 0
        except serial.SerialException as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    else:
        # Multi-device - threaded monitoring with colored labels
        print(f"Monitoring {len(ports)} devices at {baudrate} baud for {duration}s...")
        for i, port in enumerate(ports):
            color = COLORS[i % len(COLORS)]
            print(f"  {color}[dev{i}]{RESET} {port}")
        print("-" * 50)

        threads = []
        for i, port in enumerate(ports):
            label = f"dev{i}"
            color = COLORS[i % len(COLORS)]
            t = threading.Thread(
                target=monitor_single,
                args=(port, duration, baudrate, label, color),
                daemon=True,
            )
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=duration + 5)

        print("-" * 50)
        print("Monitoring complete.")
        return 0


if __name__ == "__main__":
    ports = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    sys.exit(monitor(ports, duration))
