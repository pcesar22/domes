import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:domes_app/data/transport/frame_codec.dart';

void main() {
  group('encodeFrame', () {
    test('encodes empty payload correctly', () {
      final frame = encodeFrame(0x20, Uint8List(0));

      expect(frame[0], kStartByte0);
      expect(frame[1], kStartByte1);
      expect(frame[2], 1); // Length low byte (just type)
      expect(frame[3], 0); // Length high byte
      expect(frame[4], 0x20); // Type
      expect(frame.length, kFrameOverhead); // 9 bytes total
    });

    test('encodes payload correctly', () {
      final payload = Uint8List.fromList([0x01, 0x02, 0x03, 0x04]);
      final frame = encodeFrame(0x21, payload);

      expect(frame[0], kStartByte0);
      expect(frame[1], kStartByte1);
      // Length = 1 (type) + 4 (payload) = 5
      expect(frame[2], 5);
      expect(frame[3], 0);
      expect(frame[4], 0x21);
      expect(frame.sublist(5, 9), payload);
      expect(frame.length, kFrameOverhead + 4);
    });

    test('throws on payload too large', () {
      expect(
        () => encodeFrame(0x20, Uint8List(kMaxPayloadSize + 1)),
        throwsA(isA<PayloadTooLarge>()),
      );
    });

    test('accepts max payload size', () {
      final frame = encodeFrame(0x20, Uint8List(kMaxPayloadSize));
      expect(frame.length, kFrameOverhead + kMaxPayloadSize);
    });
  });

  group('FrameDecoder', () {
    test('decode roundtrip with payload', () {
      final payload = Uint8List.fromList([0x01, 0x02, 0x03, 0x04]);
      final frame = encodeFrame(0x21, payload);

      final decoder = FrameDecoder();
      DecodeResult? result;

      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) {
          result = r;
        }
      }

      expect(result, isA<DecodeSuccess>());
      final decoded = (result as DecodeSuccess).frame;
      expect(decoded.msgType, 0x21);
      expect(decoded.payload, payload);
    });

    test('decode roundtrip with empty payload', () {
      final frame = encodeFrame(0x20, Uint8List(0));

      final decoder = FrameDecoder();
      DecodeResult? result;

      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) {
          result = r;
        }
      }

      expect(result, isA<DecodeSuccess>());
      final decoded = (result as DecodeSuccess).frame;
      expect(decoded.msgType, 0x20);
      expect(decoded.payload, isEmpty);
    });

    test('detects CRC mismatch', () {
      final frame = encodeFrame(0x20, Uint8List.fromList([0x01]));
      // Corrupt the last byte (CRC)
      frame[frame.length - 1] ^= 0xFF;

      final decoder = FrameDecoder();
      DecodeResult? result;

      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) {
          result = r;
        }
      }

      expect(result, isA<DecodeError>());
      expect((result as DecodeError).error, isA<CrcMismatch>());
    });

    test('noise resilience - ignores garbage before frame', () {
      final frame = encodeFrame(0x20, Uint8List(0));

      final decoder = FrameDecoder();

      // Feed garbage before frame
      decoder.feedByte(0x00);
      decoder.feedByte(0xFF);
      decoder.feedByte(0x12);

      DecodeResult? result;
      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) {
          result = r;
        }
      }

      expect(result, isA<DecodeSuccess>());
    });

    test('handles repeated start bytes', () {
      final frame = encodeFrame(0x20, Uint8List(0));

      final decoder = FrameDecoder();

      // Feed extra 0xAA bytes
      decoder.feedByte(kStartByte0);
      decoder.feedByte(kStartByte0);

      DecodeResult? result;
      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) {
          result = r;
        }
      }

      expect(result, isA<DecodeSuccess>());
    });

    test('reset allows decoding another frame', () {
      final frame1 = encodeFrame(0x20, Uint8List(0));
      final frame2 =
          encodeFrame(0x21, Uint8List.fromList([0xAB, 0xCD]));

      final decoder = FrameDecoder();

      // Decode first frame
      DecodeResult? result1;
      for (final byte in frame1) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) result1 = r;
      }
      expect(result1, isA<DecodeSuccess>());

      // Reset and decode second frame
      decoder.reset();
      DecodeResult? result2;
      for (final byte in frame2) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) result2 = r;
      }
      expect(result2, isA<DecodeSuccess>());
      final decoded = (result2 as DecodeSuccess).frame;
      expect(decoded.msgType, 0x21);
      expect(decoded.payload, [0xAB, 0xCD]);
    });

    test('rejects invalid length 0', () {
      final decoder = FrameDecoder();
      decoder.feedByte(kStartByte0);
      decoder.feedByte(kStartByte1);
      decoder.feedByte(0); // len low = 0
      final result = decoder.feedByte(0); // len high = 0

      expect(result, isA<DecodeError>());
      expect((result as DecodeError).error, isA<InvalidLength>());
    });

    test('rejects length exceeding max', () {
      final decoder = FrameDecoder();
      decoder.feedByte(kStartByte0);
      decoder.feedByte(kStartByte1);
      decoder.feedByte(0xFF);
      final result = decoder.feedByte(0xFF);

      expect(result, isA<DecodeError>());
      expect((result as DecodeError).error, isA<InvalidLength>());
    });

    test('max payload roundtrip', () {
      final payload = Uint8List(kMaxPayloadSize);
      for (var i = 0; i < payload.length; i++) {
        payload[i] = i & 0xFF;
      }
      final frame = encodeFrame(0x30, payload);

      final decoder = FrameDecoder();
      DecodeResult? result;
      for (final byte in frame) {
        final r = decoder.feedByte(byte);
        if (r is! DecodeNone) result = r;
      }

      expect(result, isA<DecodeSuccess>());
      final decoded = (result as DecodeSuccess).frame;
      expect(decoded.msgType, 0x30);
      expect(decoded.payload.length, kMaxPayloadSize);
      expect(decoded.payload, payload);
    });
  });
}
