#!/usr/bin/env python3
"""
Quick test script to verify the config protocol works on the device.
Sends a LIST_FEATURES command and verifies the response.
"""

import serial
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


def receive_frame(ser: serial.Serial, timeout: float = 2.0) -> tuple:
    """Receive and decode a frame from serial port."""
    start_time = time.time()
    buffer = b""

    # Wait for start bytes
    while time.time() - start_time < timeout:
        if ser.in_waiting:
            byte = ser.read(1)
            if len(buffer) == 0 and byte[0] == START_BYTE_0:
                buffer = byte
            elif len(buffer) == 1 and byte[0] == START_BYTE_1:
                buffer += byte
                break
            elif len(buffer) == 1:
                buffer = b""  # Reset
        else:
            time.sleep(0.01)

    if len(buffer) != 2:
        raise TimeoutError("Timeout waiting for start bytes")

    # Read length
    while len(buffer) < 4 and time.time() - start_time < timeout:
        if ser.in_waiting:
            buffer += ser.read(min(4 - len(buffer), ser.in_waiting))
        else:
            time.sleep(0.01)

    if len(buffer) < 4:
        raise TimeoutError("Timeout waiting for length")

    length = struct.unpack("<H", buffer[2:4])[0]
    total_len = 4 + length + 4  # start + len + (type + payload) + crc

    # Read rest of frame
    while len(buffer) < total_len and time.time() - start_time < timeout:
        if ser.in_waiting:
            buffer += ser.read(min(total_len - len(buffer), ser.in_waiting))
        else:
            time.sleep(0.01)

    if len(buffer) < total_len:
        raise TimeoutError(f"Timeout waiting for frame data (got {len(buffer)}/{total_len})")

    return decode_frame(buffer)


def test_list_features(port: str):
    """Send LIST_FEATURES command and print response."""
    print(f"Opening {port}...")
    ser = serial.Serial(port, 115200, timeout=0.1)
    time.sleep(0.5)  # Wait for device to be ready

    # Clear any pending data
    ser.reset_input_buffer()

    print("\n=== Testing LIST_FEATURES ===")
    frame = encode_frame(LIST_FEATURES_REQ)
    print(f"Sending: {frame.hex()}")
    ser.write(frame)

    try:
        msg_type, payload = receive_frame(ser)
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
    finally:
        ser.close()


def test_set_feature(port: str, feature_id: int, enabled: bool):
    """Send SET_FEATURE command and print response."""
    print(f"\nOpening {port}...")
    ser = serial.Serial(port, 115200, timeout=0.1)
    time.sleep(0.2)
    ser.reset_input_buffer()

    feature_name = FEATURES.get(feature_id, f"unknown({feature_id})")
    action = "enable" if enabled else "disable"
    print(f"\n=== Testing SET_FEATURE ({action} {feature_name}) ===")

    payload = bytes([feature_id, 1 if enabled else 0])
    frame = encode_frame(SET_FEATURE_REQ, payload)
    print(f"Sending: {frame.hex()}")
    ser.write(frame)

    try:
        msg_type, resp_payload = receive_frame(ser)
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
    finally:
        ser.close()


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

    print("=" * 50)
    print("DOMES Config Protocol Test")
    print("=" * 50)

    # Test 1: List features
    if not test_list_features(port):
        print("\nFAILED: LIST_FEATURES test")
        sys.exit(1)

    time.sleep(0.5)

    # Test 2: Disable LED effects
    if not test_set_feature(port, 1, False):
        print("\nFAILED: SET_FEATURE (disable) test")
        sys.exit(1)

    time.sleep(0.5)

    # Test 3: List again to verify change
    if not test_list_features(port):
        print("\nFAILED: LIST_FEATURES after SET test")
        sys.exit(1)

    time.sleep(0.5)

    # Test 4: Re-enable LED effects
    if not test_set_feature(port, 1, True):
        print("\nFAILED: SET_FEATURE (enable) test")
        sys.exit(1)

    print("\n" + "=" * 50)
    print("ALL TESTS PASSED")
    print("=" * 50)
