import 'dart:typed_data';

import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
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

  group('feedback interface', () {
    test('round trips bounded volume and status envelope', () {
      final request = SetAudioVolumeRequest.fromBuffer(
        serializeSetAudioVolume(67),
      );
      expect(request.volume, 67);

      final body = (SetAudioVolumeResponse()..volume = 67).writeToBuffer();
      expect(
        parseSetAudioVolumeResponse(
          Uint8List.fromList([Status.STATUS_OK.value, ...body]),
        ),
        67,
      );
    });

    test('does not turn a rejected command into acceptance', () {
      final body =
          (TriggerFeedbackResponse()
                ..probe = FeedbackProbe.FEEDBACK_PROBE_EMBEDDED_BEEP
                ..accepted = false)
              .writeToBuffer();
      expect(
        () => parseTriggerFeedbackResponse(
          Uint8List.fromList([Status.STATUS_REJECTED.value, ...body]),
        ),
        throwsA(isA<DeviceError>()),
      );
    });
  });
}
