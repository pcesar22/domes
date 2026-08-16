import 'package:domes_app/data/proto/generated/peer_drill.pb.dart';
import 'package:domes_app/data/protocol/peer_drill_validator.dart';
import 'package:flutter_test/flutter_test.dart';

const _mac = <int>[0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0];
const _armFixture = <int>[
  0x8A,
  0x01,
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
  0x80,
  0x10,
  0x01,
  0x8A,
  0x10,
  0x06,
  0x94,
  0xA9,
  0x90,
  0x0A,
  0xEB,
  0xC0,
  0x95,
  0x10,
  0x44,
  0x33,
  0x22,
  0x11,
];

PeerMessage _message({
  Beacon? beacon,
  Ping? ping,
  Pong? pong,
  JoinGame? joinGame,
  ArmTouch? armTouch,
  SetColor? setColor,
  StopAll? stopAll,
  SimulateTouch? simulateTouch,
  TouchEvent? touchEvent,
  TimeoutEvent? timeoutEvent,
}) => PeerMessage(
  protocolVersion: 1,
  senderMac: _mac,
  timestampUs: 0x11223344,
  beacon: beacon,
  ping: ping,
  pong: pong,
  joinGame: joinGame,
  armTouch: armTouch,
  setColor: setColor,
  stopAll: stopAll,
  simulateTouch: simulateTouch,
  touchEvent: touchEvent,
  timeoutEvent: timeoutEvent,
);

int _firstFieldNumber(PeerMessage message) {
  final bytes = message.writeToBuffer();
  var key = 0;
  var shift = 0;
  for (final byte in bytes) {
    key |= (byte & 0x7F) << shift;
    if (byte & 0x80 == 0) {
      return key >> 3;
    }
    shift += 7;
  }
  throw StateError('oneof field key must be a complete varint');
}

void main() {
  group('generated peer/drill contract', () {
    test('generated discriminators equal every Legacy-V1 type', () {
      final cases = <(PeerMessage, int)>[
        (PeerMessage(beacon: Beacon()), 0x01),
        (PeerMessage(ping: Ping()), 0x02),
        (PeerMessage(pong: Pong()), 0x03),
        (PeerMessage(joinGame: JoinGame()), 0x10),
        (PeerMessage(armTouch: ArmTouch()), 0x11),
        (PeerMessage(setColor: SetColor()), 0x12),
        (PeerMessage(stopAll: StopAll()), 0x13),
        (PeerMessage(simulateTouch: SimulateTouch()), 0x14),
        (PeerMessage(touchEvent: TouchEvent()), 0x20),
        (PeerMessage(timeoutEvent: TimeoutEvent()), 0x21),
      ];

      for (final (message, expected) in cases) {
        expect(_firstFieldNumber(message), expected);
      }
    });

    test('portable arm fixture round-trips exactly', () {
      final message = _message(
        armTouch: ArmTouch(
          roundToken: 0xA1B2C3D4,
          timeoutMs: 3000,
          feedbackMode: FeedbackMode.FEEDBACK_MODE_LED_AND_AUDIO,
        ),
      );

      expect(message.writeToBuffer(), _armFixture);
      final decoded = PeerMessage.fromBuffer(_armFixture);
      expect(decoded.whichPayload(), PeerMessage_Payload.armTouch);
      expect(decoded.protocolVersion, 1);
      expect(decoded.senderMac, hasLength(6));
      expect(decoded.timestampUs, 0x11223344);
      expect(decoded.armTouch.roundToken, 0xA1B2C3D4);
      expect(decoded.armTouch.timeoutMs, 3000);
      expect(
        decoded.armTouch.feedbackMode,
        FeedbackMode.FEEDBACK_MODE_LED_AND_AUDIO,
      );
    });

    test('validator enforces bounds and malformed semantic inputs', () {
      final validArm = _message(
        armTouch: ArmTouch(
          roundToken: 0xFFFFFFFF,
          timeoutMs: 0xFFFFFFFF,
          feedbackMode: FeedbackMode.FEEDBACK_MODE_LED_AND_AUDIO,
        ),
      );
      expect(
        validatePeerMessage(validArm, senderRole: PeerRole.PEER_ROLE_MASTER),
        isNull,
      );

      for (final version in <int>[0, 2]) {
        final invalid = validArm.deepCopy()..protocolVersion = version;
        expect(
          validatePeerMessage(invalid, senderRole: PeerRole.PEER_ROLE_MASTER),
          PeerDrillValidationError.unsupportedVersion,
        );
      }
      for (final length in <int>[0, 5, 7]) {
        final invalid = validArm.deepCopy()..senderMac = List.filled(length, 0);
        expect(
          validatePeerMessage(invalid, senderRole: PeerRole.PEER_ROLE_MASTER),
          PeerDrillValidationError.badMacLength,
        );
      }

      final missing = PeerMessage(protocolVersion: 1, senderMac: _mac);
      expect(
        validatePeerMessage(missing, senderRole: PeerRole.PEER_ROLE_MASTER),
        PeerDrillValidationError.missingPayload,
      );

      final zeroToken = validArm.deepCopy()..armTouch.roundToken = 0;
      expect(
        validatePeerMessage(zeroToken, senderRole: PeerRole.PEER_ROLE_MASTER),
        PeerDrillValidationError.zeroToken,
      );

      final invalidEnumFixture = List<int>.from(_armFixture);
      final enumKey = invalidEnumFixture.indexOf(0x18);
      invalidEnumFixture[enumKey + 1] = 0x04;
      final invalidEnum = PeerMessage.fromBuffer(invalidEnumFixture);
      expect(invalidEnum.armTouch.unknownFields.hasField(3), isTrue);
      expect(
        validatePeerMessage(invalidEnum, senderRole: PeerRole.PEER_ROLE_MASTER),
        PeerDrillValidationError.badEnum,
      );

      final validColor = _message(
        setColor: SetColor(red: 255, green: 255, blue: 255),
      );
      expect(
        validatePeerMessage(validColor, senderRole: PeerRole.PEER_ROLE_MASTER),
        isNull,
      );
      final badColor = validColor.deepCopy()..setColor.red = 256;
      expect(
        validatePeerMessage(badColor, senderRole: PeerRole.PEER_ROLE_MASTER),
        PeerDrillValidationError.badChannel,
      );

      final validTouch = _message(
        touchEvent: TouchEvent(
          roundToken: 1,
          reactionTimeUs: 0xFFFFFFFF,
          padIndex: 3,
        ),
      );
      expect(
        validatePeerMessage(validTouch, senderRole: PeerRole.PEER_ROLE_SLAVE),
        isNull,
      );
      final badPad = validTouch.deepCopy()..touchEvent.padIndex = 4;
      expect(
        validatePeerMessage(badPad, senderRole: PeerRole.PEER_ROLE_SLAVE),
        PeerDrillValidationError.badPad,
      );
    });

    test('validator enforces every sender-role and message combination', () {
      final neutral = <PeerMessage>[
        _message(beacon: Beacon()),
        _message(ping: Ping()),
        _message(pong: Pong()),
      ];
      for (final message in neutral) {
        for (final role in PeerRole.values) {
          expect(validatePeerMessage(message, senderRole: role), isNull);
        }
      }

      final masterMessages = <PeerMessage>[
        _message(joinGame: JoinGame()),
        _message(
          armTouch: ArmTouch(
            roundToken: 1,
            feedbackMode: FeedbackMode.FEEDBACK_MODE_NONE,
          ),
        ),
        _message(setColor: SetColor()),
        _message(stopAll: StopAll()),
        _message(simulateTouch: SimulateTouch(roundToken: 1)),
      ];
      for (final message in masterMessages) {
        expect(
          validatePeerMessage(message, senderRole: PeerRole.PEER_ROLE_MASTER),
          isNull,
        );
        for (final wrongRole in <PeerRole>[
          PeerRole.PEER_ROLE_UNSPECIFIED,
          PeerRole.PEER_ROLE_SLAVE,
        ]) {
          expect(
            validatePeerMessage(message, senderRole: wrongRole),
            PeerDrillValidationError.badRole,
          );
        }
      }

      final slaveMessages = <PeerMessage>[
        _message(touchEvent: TouchEvent(roundToken: 1)),
        _message(timeoutEvent: TimeoutEvent(roundToken: 1)),
      ];
      for (final message in slaveMessages) {
        expect(
          validatePeerMessage(message, senderRole: PeerRole.PEER_ROLE_SLAVE),
          isNull,
        );
        for (final wrongRole in <PeerRole>[
          PeerRole.PEER_ROLE_UNSPECIFIED,
          PeerRole.PEER_ROLE_MASTER,
        ]) {
          expect(
            validatePeerMessage(message, senderRole: wrongRole),
            PeerDrillValidationError.badRole,
          );
        }
      }
    });
  });
}
