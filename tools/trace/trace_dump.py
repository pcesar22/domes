#!/usr/bin/env python3
"""
ESP32 Trace Dump Tool

Dumps trace buffer from ESP32 and converts to Chrome Trace Format (Perfetto).

Usage:
    python trace_dump.py --port /dev/ttyUSB0 --output trace.json

Output can be opened in:
    - Perfetto: https://ui.perfetto.dev
    - Chrome: chrome://tracing
"""

import argparse
import json
import struct
import sys
import time
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import serial


# Frame protocol constants
FRAME_START = b'\xAA\x55'
MAX_PAYLOAD_SIZE = 1024
FRAME_OVERHEAD = 9  # 2 start + 2 length + 1 type + 4 CRC


# Trace message types
class TraceMsgType(IntEnum):
    START = 0x10
    STOP = 0x11
    DUMP = 0x12
    DATA = 0x13
    END = 0x14
    CLEAR = 0x15
    STATUS = 0x16
    ACK = 0x17


# Trace event types
class TraceEventType(IntEnum):
    TASK_SWITCH_IN = 0x01
    TASK_SWITCH_OUT = 0x02
    TASK_CREATE = 0x03
    TASK_DELETE = 0x04
    ISR_ENTER = 0x05
    ISR_EXIT = 0x06
    QUEUE_SEND = 0x07
    QUEUE_RECEIVE = 0x08
    MUTEX_LOCK = 0x09
    MUTEX_UNLOCK = 0x0A
    SPAN_BEGIN = 0x20
    SPAN_END = 0x21
    INSTANT = 0x22
    COUNTER = 0x23
    COMPLETE = 0x24


# Trace status codes
class TraceStatus(IntEnum):
    OK = 0x00
    NOT_INIT = 0x01
    ALREADY_ON = 0x02
    ALREADY_OFF = 0x03
    BUFFER_EMPTY = 0x04
    ERROR = 0xFF


# Category names
CATEGORY_NAMES = {
    0: "kernel",
    1: "transport",
    2: "ota",
    3: "wifi",
    4: "led",
    5: "audio",
    6: "touch",
    7: "game",
    8: "user",
    9: "haptic",
    10: "ble",
    11: "nvs",
}


@dataclass
class TraceEvent:
    """16-byte trace event structure."""
    timestamp: int      # Microseconds since boot
    task_id: int        # FreeRTOS task number
    event_type: int     # TraceEventType
    flags: int          # Category in upper nibble
    arg1: int           # Primary argument
    arg2: int           # Secondary argument

    @classmethod
    def from_bytes(cls, data: bytes) -> 'TraceEvent':
        """Parse event from 16 bytes."""
        if len(data) != 16:
            raise ValueError(f"Expected 16 bytes, got {len(data)}")
        ts, tid, etype, flags, arg1, arg2 = struct.unpack('<IHBBII', data)
        return cls(ts, tid, etype, flags, arg1, arg2)

    @property
    def category(self) -> int:
        """Extract category from flags."""
        return (self.flags >> 4) & 0x0F


@dataclass
class TraceMetadata:
    """Trace dump metadata."""
    event_count: int
    dropped_count: int
    start_timestamp: int
    end_timestamp: int
    task_names: Dict[int, str] = field(default_factory=dict)


def crc32(data: bytes) -> int:
    """Calculate CRC32 matching ESP-IDF implementation."""
    import binascii
    return binascii.crc32(data) & 0xFFFFFFFF


def encode_frame(msg_type: int, payload: bytes = b'') -> bytes:
    """Encode a frame with start bytes, length, type, payload, and CRC."""
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError(f"Payload too large: {len(payload)} > {MAX_PAYLOAD_SIZE}")

    length = 1 + len(payload)  # type byte + payload
    frame = bytearray()
    frame.extend(FRAME_START)
    frame.extend(struct.pack('<H', length))
    frame.append(msg_type)
    frame.extend(payload)

    # CRC over type + payload
    crc_data = bytes([msg_type]) + payload
    frame.extend(struct.pack('<I', crc32(crc_data)))

    return bytes(frame)


def decode_frame(data: bytes) -> Tuple[int, bytes]:
    """
    Decode a frame, returning (msg_type, payload).
    Raises ValueError on invalid frame.
    """
    if len(data) < FRAME_OVERHEAD:
        raise ValueError(f"Frame too short: {len(data)}")

    if data[:2] != FRAME_START:
        raise ValueError(f"Invalid start bytes: {data[:2].hex()}")

    length = struct.unpack('<H', data[2:4])[0]
    if length == 0 or length > MAX_PAYLOAD_SIZE + 1:
        raise ValueError(f"Invalid length: {length}")

    expected_size = 4 + length + 4  # header + type+payload + CRC
    if len(data) < expected_size:
        raise ValueError(f"Incomplete frame: {len(data)} < {expected_size}")

    msg_type = data[4]
    payload = data[5:4 + length]
    received_crc = struct.unpack('<I', data[4 + length:4 + length + 4])[0]

    # Verify CRC
    crc_data = data[4:4 + length]
    calculated_crc = crc32(crc_data)
    if calculated_crc != received_crc:
        raise ValueError(f"CRC mismatch: {calculated_crc:#x} != {received_crc:#x}")

    return msg_type, payload


class TraceDumper:
    """Communicate with ESP32 to dump trace buffer."""

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 5.0):
        self.serial = serial.Serial(port, baudrate, timeout=timeout)
        self.verbose = False

    def close(self):
        self.serial.close()

    def send_command(self, msg_type: TraceMsgType, payload: bytes = b''):
        """Send a trace command."""
        frame = encode_frame(msg_type, payload)
        if self.verbose:
            print(f"TX: {frame.hex()}")
        self.serial.write(frame)
        self.serial.flush()

    def receive_frame(self, timeout: float = 5.0) -> Tuple[int, bytes]:
        """Receive a complete frame."""
        self.serial.timeout = timeout
        buffer = bytearray()

        # Wait for start bytes
        start_time = time.time()
        while time.time() - start_time < timeout:
            byte = self.serial.read(1)
            if not byte:
                continue

            if len(buffer) == 0:
                if byte == b'\xAA':
                    buffer.append(0xAA)
            elif len(buffer) == 1:
                if byte == b'\x55':
                    buffer.append(0x55)
                elif byte == b'\xAA':
                    pass  # Could be AA AA 55
                else:
                    buffer.clear()
            else:
                buffer.extend(byte)

                # Check if we have enough for length
                if len(buffer) >= 4:
                    length = struct.unpack('<H', buffer[2:4])[0]
                    expected_size = 4 + length + 4

                    if len(buffer) >= expected_size:
                        if self.verbose:
                            print(f"RX: {buffer[:expected_size].hex()}")
                        return decode_frame(bytes(buffer[:expected_size]))

        raise TimeoutError("Timeout waiting for frame")

    def get_status(self) -> dict:
        """Query trace status."""
        self.send_command(TraceMsgType.STATUS)
        msg_type, payload = self.receive_frame()

        if msg_type == TraceMsgType.ACK:
            status = payload[0] if payload else 0xFF
            raise RuntimeError(f"Status failed: {TraceStatus(status).name}")

        if msg_type != TraceMsgType.STATUS:
            raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        if len(payload) < 14:
            raise RuntimeError(f"Invalid status payload: {len(payload)} bytes")

        init, enabled, event_count, dropped, buf_size = struct.unpack(
            '<BBIII', payload[:14]
        )

        return {
            'initialized': bool(init),
            'enabled': bool(enabled),
            'event_count': event_count,
            'dropped_count': dropped,
            'buffer_size': buf_size,
        }

    def start_trace(self):
        """Enable trace recording."""
        self.send_command(TraceMsgType.START)
        msg_type, payload = self.receive_frame()

        if msg_type != TraceMsgType.ACK:
            raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        status = TraceStatus(payload[0]) if payload else TraceStatus.ERROR
        if status not in (TraceStatus.OK, TraceStatus.ALREADY_ON):
            raise RuntimeError(f"Start failed: {status.name}")

    def stop_trace(self):
        """Disable trace recording."""
        self.send_command(TraceMsgType.STOP)
        msg_type, payload = self.receive_frame()

        if msg_type != TraceMsgType.ACK:
            raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        status = TraceStatus(payload[0]) if payload else TraceStatus.ERROR
        if status not in (TraceStatus.OK, TraceStatus.ALREADY_OFF):
            raise RuntimeError(f"Stop failed: {status.name}")

    def clear_trace(self):
        """Clear trace buffer."""
        self.send_command(TraceMsgType.CLEAR)
        msg_type, payload = self.receive_frame()

        if msg_type != TraceMsgType.ACK:
            raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        status = TraceStatus(payload[0]) if payload else TraceStatus.ERROR
        if status != TraceStatus.OK:
            raise RuntimeError(f"Clear failed: {status.name}")

    def dump_trace(self) -> Tuple[TraceMetadata, List[TraceEvent]]:
        """Request and receive trace dump."""
        self.send_command(TraceMsgType.DUMP)

        # First response: metadata or ACK (for empty buffer)
        msg_type, payload = self.receive_frame()

        if msg_type == TraceMsgType.ACK:
            status = TraceStatus(payload[0]) if payload else TraceStatus.ERROR
            if status == TraceStatus.BUFFER_EMPTY:
                return TraceMetadata(0, 0, 0, 0), []
            raise RuntimeError(f"Dump failed: {status.name}")

        if msg_type != TraceMsgType.DATA:
            raise RuntimeError(f"Expected DATA, got {msg_type:#x}")

        # Parse metadata
        if len(payload) < 17:
            raise RuntimeError(f"Invalid metadata: {len(payload)} bytes")

        event_count, dropped, start_ts, end_ts, task_count = struct.unpack(
            '<IIIIB', payload[:17]
        )

        # Parse task names
        task_names = {}
        offset = 17
        for _ in range(task_count):
            if offset + 18 > len(payload):
                break
            task_id = struct.unpack('<H', payload[offset:offset + 2])[0]
            name_bytes = payload[offset + 2:offset + 18]
            name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='replace')
            task_names[task_id] = name
            offset += 18

        metadata = TraceMetadata(
            event_count=event_count,
            dropped_count=dropped,
            start_timestamp=start_ts,
            end_timestamp=end_ts,
            task_names=task_names,
        )

        print(f"Metadata: {event_count} events, {dropped} dropped, "
              f"{len(task_names)} tasks")

        # Receive event chunks
        events = []
        while len(events) < event_count:
            msg_type, payload = self.receive_frame(timeout=10.0)

            if msg_type == TraceMsgType.END:
                # End of dump
                if len(payload) >= 8:
                    total, checksum = struct.unpack('<II', payload[:8])
                    print(f"End: {total} events, checksum {checksum:#x}")
                break

            if msg_type != TraceMsgType.DATA:
                raise RuntimeError(f"Expected DATA, got {msg_type:#x}")

            if len(payload) < 6:
                raise RuntimeError(f"Invalid data chunk: {len(payload)} bytes")

            chunk_offset, chunk_count = struct.unpack('<IH', payload[:6])
            print(f"  Chunk: offset={chunk_offset}, count={chunk_count}")

            # Parse events
            event_data = payload[6:]
            for i in range(chunk_count):
                start = i * 16
                if start + 16 > len(event_data):
                    break
                event = TraceEvent.from_bytes(event_data[start:start + 16])
                events.append(event)

        return metadata, events


def convert_to_chrome_json(
    metadata: TraceMetadata,
    events: List[TraceEvent],
    span_names: Dict[int, str] = None
) -> dict:
    """Convert trace events to Chrome Trace Format."""
    if span_names is None:
        span_names = {}

    chrome_events = []

    # Add process metadata
    chrome_events.append({
        "name": "process_name",
        "ph": "M",
        "pid": 1,
        "args": {"name": "ESP32-S3"},
    })

    # Add thread names
    for tid, name in metadata.task_names.items():
        chrome_events.append({
            "name": "thread_name",
            "ph": "M",
            "pid": 1,
            "tid": tid,
            "args": {"name": name},
        })

    # Add ISR thread name
    chrome_events.append({
        "name": "thread_name",
        "ph": "M",
        "pid": 1,
        "tid": 0,
        "args": {"name": "ISR"},
    })

    # Convert events
    for evt in events:
        cat = CATEGORY_NAMES.get(evt.category, f"cat{evt.category}")
        tid = evt.task_id
        ts = evt.timestamp  # Already in microseconds

        task_name = metadata.task_names.get(tid, f"Task-{tid}")

        if evt.event_type == TraceEventType.TASK_SWITCH_IN:
            chrome_events.append({
                "name": "Running",
                "cat": cat,
                "ph": "B",
                "pid": 1,
                "tid": tid,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.TASK_SWITCH_OUT:
            chrome_events.append({
                "name": "Running",
                "cat": cat,
                "ph": "E",
                "pid": 1,
                "tid": tid,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.SPAN_BEGIN:
            span_name = span_names.get(evt.arg1, f"Span-{evt.arg1:08x}")
            chrome_events.append({
                "name": span_name,
                "cat": cat,
                "ph": "B",
                "pid": 1,
                "tid": tid,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.SPAN_END:
            span_name = span_names.get(evt.arg1, f"Span-{evt.arg1:08x}")
            chrome_events.append({
                "name": span_name,
                "cat": cat,
                "ph": "E",
                "pid": 1,
                "tid": tid,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.INSTANT:
            event_name = span_names.get(evt.arg1, f"Event-{evt.arg1:08x}")
            chrome_events.append({
                "name": event_name,
                "cat": cat,
                "ph": "i",
                "pid": 1,
                "tid": tid,
                "ts": ts,
                "s": "t",  # Thread scope
            })

        elif evt.event_type == TraceEventType.COUNTER:
            counter_name = span_names.get(evt.arg1, f"Counter-{evt.arg1:08x}")
            chrome_events.append({
                "name": counter_name,
                "cat": cat,
                "ph": "C",
                "pid": 1,
                "tid": tid,
                "ts": ts,
                "args": {counter_name: evt.arg2},
            })

        elif evt.event_type == TraceEventType.ISR_ENTER:
            chrome_events.append({
                "name": f"ISR-{evt.arg1}",
                "cat": cat,
                "ph": "B",
                "pid": 1,
                "tid": 0,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.ISR_EXIT:
            chrome_events.append({
                "name": f"ISR-{evt.arg1}",
                "cat": cat,
                "ph": "E",
                "pid": 1,
                "tid": 0,
                "ts": ts,
            })

        elif evt.event_type == TraceEventType.TASK_CREATE:
            chrome_events.append({
                "name": f"TaskCreate-{evt.arg1}",
                "cat": cat,
                "ph": "i",
                "pid": 1,
                "tid": tid,
                "ts": ts,
                "s": "g",  # Global scope
            })

        elif evt.event_type == TraceEventType.TASK_DELETE:
            chrome_events.append({
                "name": f"TaskDelete-{evt.arg1}",
                "cat": cat,
                "ph": "i",
                "pid": 1,
                "tid": tid,
                "ts": ts,
                "s": "g",
            })

    return {"traceEvents": chrome_events}


def main():
    parser = argparse.ArgumentParser(
        description='Dump ESP32 trace buffer to Chrome JSON format'
    )
    parser.add_argument(
        '--port', '-p',
        required=True,
        help='Serial port (e.g., /dev/ttyUSB0, COM3)'
    )
    parser.add_argument(
        '--output', '-o',
        default='trace.json',
        help='Output file (default: trace.json)'
    )
    parser.add_argument(
        '--names', '-n',
        help='JSON file with span name mappings (spanId -> name)'
    )
    parser.add_argument(
        '--baudrate', '-b',
        type=int,
        default=115200,
        help='Serial baud rate (default: 115200)'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    parser.add_argument(
        '--status',
        action='store_true',
        help='Query trace status and exit'
    )
    parser.add_argument(
        '--start',
        action='store_true',
        help='Start trace recording'
    )
    parser.add_argument(
        '--stop',
        action='store_true',
        help='Stop trace recording'
    )
    parser.add_argument(
        '--clear',
        action='store_true',
        help='Clear trace buffer'
    )

    args = parser.parse_args()

    # Load span names if provided
    span_names = {}
    if args.names:
        with open(args.names) as f:
            # Support both int keys and string keys
            raw = json.load(f)
            for k, v in raw.items():
                span_names[int(k)] = v

    try:
        dumper = TraceDumper(args.port, args.baudrate)
        dumper.verbose = args.verbose

        if args.status:
            status = dumper.get_status()
            print(f"Initialized: {status['initialized']}")
            print(f"Enabled: {status['enabled']}")
            print(f"Events: {status['event_count']}")
            print(f"Dropped: {status['dropped_count']}")
            print(f"Buffer size: {status['buffer_size']} bytes")
            return

        if args.start:
            dumper.start_trace()
            print("Trace recording started")
            return

        if args.stop:
            dumper.stop_trace()
            print("Trace recording stopped")
            return

        if args.clear:
            dumper.clear_trace()
            print("Trace buffer cleared")
            return

        # Default: dump trace
        print(f"Requesting trace dump from {args.port}...")
        metadata, events = dumper.dump_trace()

        if not events:
            print("No events to dump")
            return

        print(f"Received {len(events)} events from "
              f"{len(metadata.task_names)} tasks")

        # Convert to Chrome format
        chrome_json = convert_to_chrome_json(metadata, events, span_names)

        # Write output
        with open(args.output, 'w') as f:
            json.dump(chrome_json, f, indent=2)

        print(f"Written to {args.output}")
        print(f"Open in: https://ui.perfetto.dev or chrome://tracing")

    except serial.SerialException as e:
        print(f"Serial error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)
    finally:
        if 'dumper' in locals():
            dumper.close()


if __name__ == '__main__':
    main()
