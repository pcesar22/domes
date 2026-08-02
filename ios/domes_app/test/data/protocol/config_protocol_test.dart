import 'dart:typed_data';

import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('TouchEventNotification', () {
    test('parses protobuf-only touch fields', () {
      final event = parseTouchEventNotification(
        Uint8List.fromList([0x08, 0x02, 0x10, 0x03, 0x18, 0x2a]),
      );

      expect(event.podId, 2);
      expect(event.padIndex, 3);
      expect(event.timestampUs, 42);
    });

    test('rejects an out-of-range pad index', () {
      expect(
        () => parseTouchEventNotification(
          Uint8List.fromList([0x08, 0x02, 0x10, 0x04]),
        ),
        throwsA(isA<DecodeFailure>()),
      );
    });

    test('wraps malformed protobuf as a decode failure', () {
      expect(
        () => parseTouchEventNotification(Uint8List.fromList([0x80])),
        throwsA(isA<DecodeFailure>()),
      );
    });
  });
}
