// Cross-verify CRC32 compatibility with the Rust CLI.
//
// The Rust CLI uses crc32fast which implements CRC32 (ISO 3309).
// We verify our Dart implementation produces the same values
// as Python's binascii.crc32 (same standard polynomial).
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:domes_app/data/transport/frame_codec.dart';

void main() {
  test('CRC32 matches Python/Rust for LIST_FEATURES_REQ frame', () {
    final frame = encodeFrame(0x20, Uint8List(0));

    expect(frame.length, 9);
    expect(frame[0], 0xAA);
    expect(frame[1], 0x55);
    expect(frame[2], 0x01);
    expect(frame[3], 0x00);
    expect(frame[4], 0x20);

    // CRC32 of [0x20] = 0xE96CCF45 (verified with Python binascii.crc32)
    final crc =
        frame[5] | (frame[6] << 8) | (frame[7] << 16) | (frame[8] << 24);
    expect(crc, 0xE96CCF45,
        reason: 'CRC32 of [0x20] must match standard ISO 3309');
  });

  test('encode/decode roundtrip verifies CRC integrity', () {
    final frame =
        encodeFrame(0x00, Uint8List.fromList([0x01, 0x02, 0x03]));

    final decoder = FrameDecoder();
    DecodeResult? result;
    for (final byte in frame) {
      final r = decoder.feedByte(byte);
      if (r is! DecodeNone) result = r;
    }
    expect(result, isA<DecodeSuccess>());
    final decoded = (result as DecodeSuccess).frame;
    expect(decoded.msgType, 0x00);
    expect(decoded.payload, [0x01, 0x02, 0x03]);
  });
}
