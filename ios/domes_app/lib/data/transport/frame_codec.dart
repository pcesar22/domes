/// Frame codec for DOMES protocol.
///
/// Frame format: [0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
/// - Start bytes: 0xAA 0x55
/// - Length: 2 bytes little-endian, contains (Type + Payload) length
/// - Type: 1 byte message type
/// - Payload: 0-1024 bytes
/// - CRC32: 4 bytes little-endian, calculated over (Type + Payload)
///
/// Direct port of tools/domes-cli/src/transport/frame.rs
library;

import 'dart:typed_data';

const int kStartByte0 = 0xAA;
const int kStartByte1 = 0x55;
const int kMaxPayloadSize = 1024;
const int kBleMaxPayload = 400;
const int kFrameOverhead = 9; // 2 start + 2 len + 1 type + 4 crc

/// A decoded frame.
class Frame {
  final int msgType;
  final Uint8List payload;

  Frame({required this.msgType, required this.payload});
}

/// Frame codec errors.
sealed class FrameError {
  const FrameError();
}

class PayloadTooLarge extends FrameError {
  final int size;
  const PayloadTooLarge(this.size);

  @override
  String toString() => 'PayloadTooLarge($size > $kMaxPayloadSize)';
}

class InvalidLength extends FrameError {
  final int length;
  const InvalidLength(this.length);

  @override
  String toString() => 'InvalidLength($length)';
}

class CrcMismatch extends FrameError {
  final int expected;
  final int actual;
  const CrcMismatch({required this.expected, required this.actual});

  @override
  String toString() =>
      'CrcMismatch(expected: 0x${expected.toRadixString(16).padLeft(8, '0')}, '
      'actual: 0x${actual.toRadixString(16).padLeft(8, '0')})';
}

/// CRC32 lookup table (ISO 3309 / standard polynomial 0xEDB88320).
final Uint32List _crc32Table = _buildCrc32Table();

Uint32List _buildCrc32Table() {
  final table = Uint32List(256);
  for (var i = 0; i < 256; i++) {
    var crc = i;
    for (var j = 0; j < 8; j++) {
      if ((crc & 1) != 0) {
        crc = (crc >>> 1) ^ 0xEDB88320;
      } else {
        crc = crc >>> 1;
      }
    }
    table[i] = crc;
  }
  return table;
}

/// Compute CRC32 over the given bytes (ISO 3309 / standard polynomial).
int _crc32(Uint8List data) {
  var crc = 0xFFFFFFFF;
  for (final byte in data) {
    crc = _crc32Table[(crc ^ byte) & 0xFF] ^ (crc >>> 8);
  }
  return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF;
}

/// Encode a frame with the given type and payload.
///
/// Returns the encoded frame bytes or throws on error.
Uint8List encodeFrame(int msgType, Uint8List payload) {
  if (payload.length > kMaxPayloadSize) {
    throw PayloadTooLarge(payload.length);
  }

  // Length field = type (1 byte) + payload length
  final length = 1 + payload.length;

  // Calculate CRC over type + payload
  final crcInput = Uint8List(1 + payload.length);
  crcInput[0] = msgType;
  crcInput.setRange(1, 1 + payload.length, payload);
  final crc = _crc32(crcInput);

  // Build frame
  final frameSize = kFrameOverhead + payload.length;
  final frame = Uint8List(frameSize);
  var offset = 0;

  // Start bytes
  frame[offset++] = kStartByte0;
  frame[offset++] = kStartByte1;

  // Length (little-endian)
  frame[offset++] = length & 0xFF;
  frame[offset++] = (length >> 8) & 0xFF;

  // Type
  frame[offset++] = msgType;

  // Payload
  frame.setRange(offset, offset + payload.length, payload);
  offset += payload.length;

  // CRC32 (little-endian)
  frame[offset++] = crc & 0xFF;
  frame[offset++] = (crc >> 8) & 0xFF;
  frame[offset++] = (crc >> 16) & 0xFF;
  frame[offset++] = (crc >> 24) & 0xFF;

  return frame;
}

/// Decoder state machine states.
enum _DecoderState {
  waitStart0,
  waitStart1,
  waitLenLow,
  waitLenHigh,
  waitType,
  waitPayload,
  waitCrc,
  complete,
  error,
}

/// Result from feeding a byte into the decoder.
sealed class DecodeResult {
  const DecodeResult();
}

class DecodeNone extends DecodeResult {
  const DecodeNone();
}

class DecodeSuccess extends DecodeResult {
  final Frame frame;
  const DecodeSuccess(this.frame);
}

class DecodeError extends DecodeResult {
  final FrameError error;
  const DecodeError(this.error);
}

/// Streaming frame decoder.
///
/// Feed bytes one at a time via [feedByte]. Returns a [DecodeResult]
/// indicating whether a frame was decoded, an error occurred, or more
/// bytes are needed.
class FrameDecoder {
  _DecoderState _state = _DecoderState.waitStart0;
  int _length = 0;
  int _msgType = 0;
  List<int> _payload = [];
  final Uint8List _crcBytes = Uint8List(4);
  int _crcIndex = 0;
  int _payloadIndex = 0;

  /// Reset the decoder state.
  void reset() {
    _state = _DecoderState.waitStart0;
    _length = 0;
    _msgType = 0;
    _payload = [];
    _crcBytes.fillRange(0, 4, 0);
    _crcIndex = 0;
    _payloadIndex = 0;
  }

  /// Feed a single byte to the decoder.
  ///
  /// Returns [DecodeSuccess] when a complete frame is decoded,
  /// [DecodeError] on protocol errors, or [DecodeNone] when more bytes
  /// are needed.
  DecodeResult feedByte(int byte) {
    switch (_state) {
      case _DecoderState.waitStart0:
        if (byte == kStartByte0) {
          _state = _DecoderState.waitStart1;
        }
        return const DecodeNone();

      case _DecoderState.waitStart1:
        if (byte == kStartByte1) {
          _state = _DecoderState.waitLenLow;
        } else if (byte == kStartByte0) {
          // Stay in waitStart1 (might be repeated start byte)
        } else {
          _state = _DecoderState.waitStart0;
        }
        return const DecodeNone();

      case _DecoderState.waitLenLow:
        _length = byte;
        _state = _DecoderState.waitLenHigh;
        return const DecodeNone();

      case _DecoderState.waitLenHigh:
        _length |= (byte << 8);
        if (_length == 0 || _length > kMaxPayloadSize + 1) {
          _state = _DecoderState.error;
          return DecodeError(InvalidLength(_length));
        }
        _state = _DecoderState.waitType;
        return const DecodeNone();

      case _DecoderState.waitType:
        _msgType = byte;
        final payloadLen = _length - 1;
        _payload = [];
        _payloadIndex = 0;
        if (payloadLen == 0) {
          _state = _DecoderState.waitCrc;
          _crcIndex = 0;
        } else {
          _state = _DecoderState.waitPayload;
        }
        return const DecodeNone();

      case _DecoderState.waitPayload:
        _payload.add(byte);
        _payloadIndex++;
        final payloadLen = _length - 1;
        if (_payloadIndex >= payloadLen) {
          _state = _DecoderState.waitCrc;
          _crcIndex = 0;
        }
        return const DecodeNone();

      case _DecoderState.waitCrc:
        _crcBytes[_crcIndex] = byte;
        _crcIndex++;
        if (_crcIndex >= 4) {
          _state = _DecoderState.complete;

          // Verify CRC
          final receivedCrc = _crcBytes[0] |
              (_crcBytes[1] << 8) |
              (_crcBytes[2] << 16) |
              (_crcBytes[3] << 24);

          final payloadData = Uint8List.fromList(_payload);
          final crcInput = Uint8List(1 + payloadData.length);
          crcInput[0] = _msgType;
          crcInput.setRange(1, 1 + payloadData.length, payloadData);
          final calculatedCrc = _crc32(crcInput);

          if (receivedCrc != calculatedCrc) {
            return DecodeError(CrcMismatch(
              expected: calculatedCrc,
              actual: receivedCrc,
            ));
          }

          return DecodeSuccess(Frame(
            msgType: _msgType,
            payload: payloadData,
          ));
        }
        return const DecodeNone();

      case _DecoderState.complete:
      case _DecoderState.error:
        // Should call reset() before feeding more bytes
        return const DecodeNone();
    }
  }
}
