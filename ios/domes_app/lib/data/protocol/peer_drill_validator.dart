import '../proto/generated/peer_drill.pb.dart';

/// Stable semantic validation failures shared with nanopb and prost consumers.
enum PeerDrillValidationError {
  unsupportedVersion,
  badMacLength,
  missingPayload,
  badEnum,
  badChannel,
  badPad,
  zeroToken,
  badRole,
}

/// Validates bounded peer semantics and authenticated sender direction.
PeerDrillValidationError? validatePeerMessage(
  PeerMessage message, {
  required PeerRole senderRole,
}) {
  if (message.protocolVersion != 1) {
    return PeerDrillValidationError.unsupportedVersion;
  }
  if (message.senderMac.length != 6) {
    return PeerDrillValidationError.badMacLength;
  }

  switch (message.whichPayload()) {
    case PeerMessage_Payload.armTouch:
      if (message.armTouch.roundToken == 0) {
        return PeerDrillValidationError.zeroToken;
      }
      // protobuf.dart preserves an unrecognized enum value as unknown field 3.
      if (message.armTouch.unknownFields.hasField(3)) {
        return PeerDrillValidationError.badEnum;
      }
    case PeerMessage_Payload.setColor:
      if (message.setColor.red > 255 ||
          message.setColor.green > 255 ||
          message.setColor.blue > 255) {
        return PeerDrillValidationError.badChannel;
      }
    case PeerMessage_Payload.simulateTouch:
      if (message.simulateTouch.roundToken == 0) {
        return PeerDrillValidationError.zeroToken;
      }
      if (message.simulateTouch.padIndex > 3) {
        return PeerDrillValidationError.badPad;
      }
    case PeerMessage_Payload.touchEvent:
      if (message.touchEvent.roundToken == 0) {
        return PeerDrillValidationError.zeroToken;
      }
      if (message.touchEvent.padIndex > 3) {
        return PeerDrillValidationError.badPad;
      }
    case PeerMessage_Payload.timeoutEvent:
      if (message.timeoutEvent.roundToken == 0) {
        return PeerDrillValidationError.zeroToken;
      }
    case PeerMessage_Payload.notSet:
      return PeerDrillValidationError.missingPayload;
    case PeerMessage_Payload.beacon:
    case PeerMessage_Payload.ping:
    case PeerMessage_Payload.pong:
    case PeerMessage_Payload.joinGame:
    case PeerMessage_Payload.stopAll:
      break;
  }

  final roleAllowed = switch (message.whichPayload()) {
    PeerMessage_Payload.beacon ||
    PeerMessage_Payload.ping ||
    PeerMessage_Payload.pong => true,
    PeerMessage_Payload.joinGame ||
    PeerMessage_Payload.armTouch ||
    PeerMessage_Payload.setColor ||
    PeerMessage_Payload.stopAll ||
    PeerMessage_Payload.simulateTouch =>
      senderRole == PeerRole.PEER_ROLE_MASTER,
    PeerMessage_Payload.touchEvent ||
    PeerMessage_Payload.timeoutEvent => senderRole == PeerRole.PEER_ROLE_SLAVE,
    PeerMessage_Payload.notSet => false,
  };
  return roleAllowed ? null : PeerDrillValidationError.badRole;
}
