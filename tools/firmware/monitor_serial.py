#!/usr/bin/env python3
"""
Serial monitor for ESP32 devices.

Usage (single device):
    python monitor_serial.py /dev/serial/by-id/usb-Espressif_... 10

Usage (multi-device):
    python monitor_serial.py /dev/serial/by-id/device1,/dev/serial/by-id/device2 10
"""

import argparse
import os
import stat
import sys
import threading
import time

import serial

# ANSI color codes for device labeling
COLORS = [
    "\033[36m",  # Cyan
    "\033[33m",  # Yellow
    "\033[35m",  # Magenta
    "\033[32m",  # Green
    "\033[34m",  # Blue
    "\033[31m",  # Red
]
RESET = "\033[0m"


def monitor_single(
    port: str, duration: int, baudrate: int = 115200, label: str = "", color: str = ""
):
    """Read serial output from one ESP32 device."""
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            prefix = f"{color}[{label}]{RESET} " if label else ""

            start = time.time()
            line_buffer = ""
            while time.time() - start < duration:
                if ser.in_waiting:
                    data = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                    # Print with device prefix on each line
                    for ch in data:
                        if ch == "\n":
                            print(f"{prefix}{line_buffer}")
                            line_buffer = ""
                        else:
                            line_buffer += ch
                time.sleep(0.05)

            # Flush remaining buffer
            if line_buffer:
                print(f"{prefix}{line_buffer}")

        return 0
    except serial.SerialException as e:
        print(f"Error on {port}: {e}", file=sys.stderr)
        return 1


def monitor(ports_str: str, duration: int = 10, baudrate: int = 115200):
    """Monitor one or more serial ports."""
    ports = [p.strip() for p in ports_str.split(",") if p.strip()]
    if not ports:
        print("No serial ports were provided.", file=sys.stderr)
        return 2
    if len(set(ports)) != len(ports):
        print("Duplicate serial ports were provided.", file=sys.stderr)
        return 2
    for port in ports:
        try:
            mode = os.stat(port).st_mode
        except OSError as error:
            print(f"Cannot access serial port {port}: {error}", file=sys.stderr)
            return 2
        if not stat.S_ISCHR(mode):
            print(f"Serial port is not a character device: {port}", file=sys.stderr)
            return 2

    if len(ports) == 1:
        # Single device - no labels needed (backward compatible)
        port = ports[0]
        print(f"Monitoring {port} at {baudrate} baud for {duration}s...")
        print("-" * 50)
        result = monitor_single(port, duration, baudrate)
        print("\n" + "-" * 50)
        print("Monitoring complete." if result == 0 else "Monitoring failed.")
        return result
    else:
        # Multi-device - threaded monitoring with colored labels
        print(f"Monitoring {len(ports)} devices at {baudrate} baud for {duration}s...")
        for i, port in enumerate(ports):
            color = COLORS[i % len(COLORS)]
            print(f"  {color}[dev{i}]{RESET} {port}")
        print("-" * 50)

        results = [1] * len(ports)

        def record_result(index: int, port: str, label: str, color: str):
            results[index] = monitor_single(port, duration, baudrate, label, color)

        threads = []
        for i, port in enumerate(ports):
            label = f"dev{i}"
            color = COLORS[i % len(COLORS)]
            t = threading.Thread(
                target=record_result,
                args=(i, port, label, color),
                daemon=True,
            )
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=duration + 5)

        print("-" * 50)
        failed = sum(result != 0 for result in results)
        if failed:
            print(
                f"Monitoring failed on {failed} of {len(ports)} devices.",
                file=sys.stderr,
            )
            return 1

        print("Monitoring complete.")
        return 0


def parse_args():
    parser = argparse.ArgumentParser(
        description="Monitor one or more explicit serial devices"
    )
    parser.add_argument("ports", help="Comma-separated serial device paths")
    parser.add_argument(
        "duration", nargs="?", type=int, default=10, help="Duration in seconds"
    )
    parser.add_argument("--baudrate", type=int, default=115200)
    args = parser.parse_args()
    if args.duration <= 0 or args.baudrate <= 0:
        parser.error("duration and baudrate must be positive")
    return args


if __name__ == "__main__":
    arguments = parse_args()
    sys.exit(monitor(arguments.ports, arguments.duration, arguments.baudrate))
