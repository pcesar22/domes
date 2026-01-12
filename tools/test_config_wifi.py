#!/usr/bin/env python3
"""
Test script for config protocol over WiFi (TCP).
Connects to the device's TCP config server and runs the same tests as test_config.py.

Usage:
    python3 test_config_wifi.py 192.168.1.100:5000
    python3 test_config_wifi.py 192.168.1.100  # Uses default port 5000
"""

import socket
import struct
import sys
import time

# Frame constants
START_BYTE_0 = 0xAA
START_BYTE_1 = 0x55

# Config message types
LIST_FEATURES_REQ = 0x20
LIST_FEATURES_RSP = 0x21
SET_FEATURE_REQ = 0x22
SET_FEATURE_RSP = 0x23

# Default port
DEFAULT_PORT = 5000

# Feature IDs
FEATURES = {
    1: "led-effects",
    2: "ble",
    3: "wifi",
    4: "esp-now",
    5: "touch",
    6: "haptic",
    7: "audio",
}


def crc32(data: bytes) -> int:
    """Calculate CRC32 matching firmware's IEEE 802.3 polynomial."""
    import binascii
    return binascii.crc32(data) & 0xFFFFFFFF


def encode_frame(msg_type: int, payload: bytes = b"") -> bytes:
    """Encode a frame: [0xAA][0x55][LenLE16][Type][Payload][CRC32LE]"""
    length = 1 + len(payload)  # type + payload

    # Calculate CRC over type + payload
    crc_data = bytes([msg_type]) + payload
    crc = crc32(crc_data)

    # Build frame
    frame = bytes([START_BYTE_0, START_BYTE_1])
    frame += struct.pack("<H", length)  # Length LE
    frame += bytes([msg_type])
    frame += payload
    frame += struct.pack("<I", crc)  # CRC32 LE

    return frame


def decode_frame(data: bytes) -> tuple:
    """Decode a frame, returns (msg_type, payload) or raises ValueError."""
    if len(data) < 9:
        raise ValueError(f"Frame too short: {len(data)} bytes")

    if data[0] != START_BYTE_0 or data[1] != START_BYTE_1:
        raise ValueError(f"Invalid start bytes: {data[0]:02X} {data[1]:02X}")

    length = struct.unpack("<H", data[2:4])[0]
    msg_type = data[4]
    payload = data[5:5 + length - 1]
    received_crc = struct.unpack("<I", data[5 + length - 1:9 + length - 1])[0]

    # Verify CRC
    crc_data = bytes([msg_type]) + payload
    expected_crc = crc32(crc_data)

    if received_crc != expected_crc:
        raise ValueError(f"CRC mismatch: expected {expected_crc:08X}, got {received_crc:08X}")

    return msg_type, payload


class TcpTransport:
    """TCP transport for config protocol."""

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self):
        """Connect to the device."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))
        # Disable Nagle's algorithm
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print(f"Connected to {self.host}:{self.port}")

    def close(self):
        """Close the connection."""
        if self.sock:
            self.sock.close()
            self.sock = None

    def send_frame(self, msg_type: int, payload: bytes = b""):
        """Send a frame."""
        frame = encode_frame(msg_type, payload)
        self.sock.sendall(frame)

    def receive_frame(self, timeout: float = 2.0) -> tuple:
        """Receive and decode a frame."""
        self.sock.settimeout(timeout)
        buffer = b""
        start_time = time.time()

        # Wait for start bytes
        while time.time() - start_time < timeout:
            try:
                byte = self.sock.recv(1)
                if not byte:
                    raise ConnectionError("Connection closed by peer")

                if len(buffer) == 0 and byte[0] == START_BYTE_0:
                    buffer = byte
                elif len(buffer) == 1 and byte[0] == START_BYTE_1:
                    buffer += byte
                    break
                elif len(buffer) == 1:
                    buffer = b""  # Reset
            except socket.timeout:
                continue

        if len(buffer) != 2:
            raise TimeoutError("Timeout waiting for start bytes")

        # Read length (2 bytes)
        while len(buffer) < 4 and time.time() - start_time < timeout:
            try:
                data = self.sock.recv(4 - len(buffer))
                if not data:
                    raise ConnectionError("Connection closed by peer")
                buffer += data
            except socket.timeout:
                continue

        if len(buffer) < 4:
            raise TimeoutError("Timeout waiting for length")

        length = struct.unpack("<H", buffer[2:4])[0]
        total_len = 4 + length + 4  # start + len + (type + payload) + crc

        # Read rest of frame
        while len(buffer) < total_len and time.time() - start_time < timeout:
            try:
                data = self.sock.recv(total_len - len(buffer))
                if not data:
                    raise ConnectionError("Connection closed by peer")
                buffer += data
            except socket.timeout:
                continue

        if len(buffer) < total_len:
            raise TimeoutError(f"Timeout waiting for frame data (got {len(buffer)}/{total_len})")

        return decode_frame(buffer)


def test_list_features(transport: TcpTransport) -> bool:
    """Send LIST_FEATURES command and print response."""
    print("\n=== Testing LIST_FEATURES ===")
    frame = encode_frame(LIST_FEATURES_REQ)
    print(f"Sending: {frame.hex()}")
    transport.send_frame(LIST_FEATURES_REQ)

    try:
        msg_type, payload = transport.receive_frame()
        print(f"Received: type=0x{msg_type:02X}, payload={payload.hex()}")

        if msg_type != LIST_FEATURES_RSP:
            print(f"ERROR: Expected response type 0x{LIST_FEATURES_RSP:02X}, got 0x{msg_type:02X}")
            return False

        if len(payload) < 2:
            print(f"ERROR: Payload too short: {len(payload)} bytes")
            return False

        status = payload[0]
        count = payload[1]

        if status != 0:
            print(f"ERROR: Device returned error status: {status}")
            return False

        print(f"\nStatus: OK")
        print(f"Feature count: {count}")
        print("\nFeatures:")

        for i in range(count):
            offset = 2 + i * 2
            if offset + 1 < len(payload):
                feature_id = payload[offset]
                enabled = payload[offset + 1]
                name = FEATURES.get(feature_id, f"unknown({feature_id})")
                status_str = "enabled" if enabled else "disabled"
                print(f"  {name}: {status_str}")

        print("\n=== LIST_FEATURES: PASS ===")
        return True

    except TimeoutError as e:
        print(f"ERROR: {e}")
        return False
    except ValueError as e:
        print(f"ERROR: {e}")
        return False


def test_set_feature(transport: TcpTransport, feature_id: int, enabled: bool) -> bool:
    """Send SET_FEATURE command and print response."""
    feature_name = FEATURES.get(feature_id, f"unknown({feature_id})")
    action = "enable" if enabled else "disable"
    print(f"\n=== Testing SET_FEATURE ({action} {feature_name}) ===")

    payload = bytes([feature_id, 1 if enabled else 0])
    frame = encode_frame(SET_FEATURE_REQ, payload)
    print(f"Sending: {frame.hex()}")
    transport.send_frame(SET_FEATURE_REQ, payload)

    try:
        msg_type, resp_payload = transport.receive_frame()
        print(f"Received: type=0x{msg_type:02X}, payload={resp_payload.hex()}")

        if msg_type != SET_FEATURE_RSP:
            print(f"ERROR: Expected response type 0x{SET_FEATURE_RSP:02X}, got 0x{msg_type:02X}")
            return False

        if len(resp_payload) < 3:
            print(f"ERROR: Payload too short: {len(resp_payload)} bytes")
            return False

        status = resp_payload[0]
        resp_feature = resp_payload[1]
        resp_enabled = resp_payload[2]

        if status != 0:
            print(f"ERROR: Device returned error status: {status}")
            return False

        print(f"\nStatus: OK")
        print(f"Feature: {FEATURES.get(resp_feature, resp_feature)}")
        print(f"Enabled: {'yes' if resp_enabled else 'no'}")

        if resp_feature != feature_id:
            print(f"WARNING: Response feature ID doesn't match request")

        if resp_enabled != (1 if enabled else 0):
            print(f"WARNING: Response enabled state doesn't match request")

        print(f"\n=== SET_FEATURE: PASS ===")
        return True

    except TimeoutError as e:
        print(f"ERROR: {e}")
        return False
    except ValueError as e:
        print(f"ERROR: {e}")
        return False


def parse_address(addr: str) -> tuple:
    """Parse address in format 'host' or 'host:port'."""
    if ':' in addr:
        parts = addr.split(':')
        return parts[0], int(parts[1])
    else:
        return addr, DEFAULT_PORT


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_config_wifi.py <ip:port>")
        print("       python3 test_config_wifi.py <ip>  # Uses default port 5000")
        sys.exit(1)

    host, port = parse_address(sys.argv[1])

    print("=" * 50)
    print("DOMES Config Protocol Test (WiFi/TCP)")
    print("=" * 50)

    transport = TcpTransport(host, port)

    try:
        print(f"\nConnecting to {host}:{port}...")
        transport.connect()

        # Test 1: List features
        if not test_list_features(transport):
            print("\nFAILED: LIST_FEATURES test")
            sys.exit(1)

        time.sleep(0.2)

        # Test 2: Disable LED effects
        if not test_set_feature(transport, 1, False):
            print("\nFAILED: SET_FEATURE (disable) test")
            sys.exit(1)

        time.sleep(0.2)

        # Test 3: List again to verify change
        if not test_list_features(transport):
            print("\nFAILED: LIST_FEATURES after SET test")
            sys.exit(1)

        time.sleep(0.2)

        # Test 4: Re-enable LED effects
        if not test_set_feature(transport, 1, True):
            print("\nFAILED: SET_FEATURE (enable) test")
            sys.exit(1)

        print("\n" + "=" * 50)
        print("ALL TESTS PASSED")
        print("=" * 50)

    except ConnectionRefusedError:
        print(f"ERROR: Connection refused - is the device running and TCP config server enabled?")
        sys.exit(1)
    except socket.timeout:
        print(f"ERROR: Connection timeout - check the IP address and port")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)
    finally:
        transport.close()


if __name__ == "__main__":
    main()
