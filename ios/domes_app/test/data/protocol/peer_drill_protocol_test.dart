import 'package:domes_app/data/proto/generated/peer_drill.pb.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('generated peer/drill contract', () {
    test('exposes exactly the ten production peer variants', () {
      final messages = <PeerMessage>[
        PeerMessage(beacon: Beacon()),
        PeerMessage(ping: Ping()),
        PeerMessage(pong: Pong()),
        PeerMessage(joinGame: JoinGame()),
        PeerMessage(armTouch: ArmTouch()),
        PeerMessage(setColor: SetColor()),
        PeerMessage(stopAll: StopAll()),
        PeerMessage(simulateTouch: SimulateTouch()),
        PeerMessage(touchEvent: TouchEvent()),
        PeerMessage(timeoutEvent: TimeoutEvent()),
      ];

      expect(
        messages.map((message) => message.whichPayload()).toSet(),
        <PeerMessage_Payload>{
          PeerMessage_Payload.beacon,
          PeerMessage_Payload.ping,
          PeerMessage_Payload.pong,
          PeerMessage_Payload.joinGame,
          PeerMessage_Payload.armTouch,
          PeerMessage_Payload.setColor,
          PeerMessage_Payload.stopAll,
          PeerMessage_Payload.simulateTouch,
          PeerMessage_Payload.touchEvent,
          PeerMessage_Payload.timeoutEvent,
        },
      );
    });

    test('fixed32 arm fixture round-trips exactly', () {
      const fixture = <int>[
        0x08,
        0x01,
        0x12,
        0x06,
        0x94,
        0xA9,
        0x90,
        0x0A,
        0xEB,
        0xC0,
        0x1D,
        0x44,
        0x33,
        0x22,
        0x11,
        0x72,
        0x0A,
        0x0D,
        0xD4,
        0xC3,
        0xB2,
        0xA1,
        0x10,
        0xB8,
        0x17,
        0x18,
        0x03,
      ];
      final message = PeerMessage(
        protocolVersion: 1,
        senderMac: <int>[0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0],
        senderTimestampUs: 0x11223344,
        armTouch: ArmTouch(
          roundToken: 0xA1B2C3D4,
          timeoutMs: 3000,
          feedbackMode: FeedbackMode.FEEDBACK_MODE_LED_AND_AUDIO,
        ),
      );

      expect(message.writeToBuffer(), fixture);
      final decoded = PeerMessage.fromBuffer(fixture);
      expect(decoded.whichPayload(), PeerMessage_Payload.armTouch);
      expect(decoded.protocolVersion, 1);
      expect(decoded.senderMac, hasLength(6));
      expect(decoded.armTouch.roundToken, 0xA1B2C3D4);
      expect(decoded.armTouch.timeoutMs, 3000);
      expect(
        decoded.armTouch.feedbackMode,
        FeedbackMode.FEEDBACK_MODE_LED_AND_AUDIO,
      );
    });
  });
}
