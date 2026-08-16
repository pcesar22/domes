import 'dart:typed_data';

import 'package:domes_app/data/proto/generated/peer_drill.pb.dart';

const int _maxPeerMessageSize = 64;

PeerMessage decodePeerMessage(
  Uint8List bytes, {
  required PeerRole receiverRole,
  required PeerLifecycleState state,
}) {
  if (bytes.isEmpty || bytes.length > _maxPeerMessageSize) {
    throw const FormatException('peer message is empty or oversized');
  }
  final PeerMessage message;
  try {
    message = PeerMessage.fromBuffer(bytes);
  } on Object {
    throw const FormatException('malformed peer protobuf');
  }
  if (_hasUnknownFields(message)) {
    throw const FormatException('unknown peer protobuf field');
  }
  if (!_sameBytes(message.writeToBuffer(), bytes)) {
    throw const FormatException('noncanonical peer protobuf');
  }
  validatePeerMessage(message, receiverRole: receiverRole, state: state);
  return message;
}

void validatePeerMessage(
  PeerMessage message, {
  required PeerRole receiverRole,
  required PeerLifecycleState state,
}) {
  if (receiverRole != PeerRole.PEER_ROLE_MASTER &&
      receiverRole != PeerRole.PEER_ROLE_SLAVE) {
    throw const FormatException('role-invalid peer receiver');
  }
  if (!message.hasHeader() ||
      message.header.version != ContractVersion.CONTRACT_VERSION_1 ||
      message.header.srcPodId > 0xffff ||
      message.header.dstPodId > 0xffff ||
      (message.header.senderMac.isNotEmpty &&
          message.header.senderMac.length != 6) ||
      message.whichPayload() == PeerMessage_Payload.notSet) {
    throw const FormatException('invalid peer fields');
  }

  final bool discovery =
      message.hasBeacon() || message.hasPing() || message.hasPong();
  final bool event = message.hasTouchEvent() || message.hasTimeoutEvent();
  final PeerRole expectedSender = discovery
      ? PeerRole.PEER_ROLE_UNSPECIFIED
      : event
      ? PeerRole.PEER_ROLE_SLAVE
      : PeerRole.PEER_ROLE_MASTER;
  if (message.header.senderRole != expectedSender) {
    throw const FormatException('role-invalid peer message');
  }

  final bool fieldsValid = switch (message.whichPayload()) {
    PeerMessage_Payload.beacon ||
    PeerMessage_Payload.ping ||
    PeerMessage_Payload.pong ||
    PeerMessage_Payload.stopAll => true,
    PeerMessage_Payload.joinGame =>
      message.joinGame.assignedRole == PeerRole.PEER_ROLE_SLAVE,
    PeerMessage_Payload.armTouch =>
      message.armTouch.roundToken != 0 &&
          message.armTouch.timeoutMs > 0 &&
          message.armTouch.timeoutMs <= 60000 &&
          message.armTouch.feedbackMode <= 3,
    PeerMessage_Payload.setColor =>
      message.setColor.r <= 255 &&
          message.setColor.g <= 255 &&
          message.setColor.b <= 255,
    PeerMessage_Payload.simulateTouch =>
      message.simulateTouch.roundToken != 0 &&
          message.simulateTouch.padIndex <= 3,
    PeerMessage_Payload.touchEvent =>
      message.touchEvent.roundToken != 0 && message.touchEvent.padIndex <= 3,
    PeerMessage_Payload.timeoutEvent => message.timeoutEvent.roundToken != 0,
    PeerMessage_Payload.notSet => false,
  };
  if (!fieldsValid) throw const FormatException('invalid peer fields');

  final bool allowed =
      (discovery &&
          state != PeerLifecycleState.PEER_LIFECYCLE_STATE_STOPPED &&
          state != PeerLifecycleState.PEER_LIFECYCLE_STATE_UNSPECIFIED) ||
      switch (state) {
        PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY =>
          receiverRole == PeerRole.PEER_ROLE_SLAVE && message.hasJoinGame(),
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY =>
          message.hasStopAll() ||
              (receiverRole == PeerRole.PEER_ROLE_SLAVE &&
                  (message.hasArmTouch() || message.hasSetColor())) ||
              (receiverRole == PeerRole.PEER_ROLE_MASTER && event),
        PeerLifecycleState.PEER_LIFECYCLE_STATE_ARMED =>
          receiverRole == PeerRole.PEER_ROLE_SLAVE &&
              (message.hasSimulateTouch() || message.hasStopAll()),
        PeerLifecycleState.PEER_LIFECYCLE_STATE_UNSPECIFIED ||
        PeerLifecycleState.PEER_LIFECYCLE_STATE_STOPPED => false,
        _ => false,
      };
  if (!allowed) throw const FormatException('state-invalid peer message');
}

bool _hasUnknownFields(PeerMessage message) {
  if (message.unknownFields.isNotEmpty ||
      (message.hasHeader() && message.header.unknownFields.isNotEmpty)) {
    return true;
  }
  return switch (message.whichPayload()) {
    PeerMessage_Payload.beacon => message.beacon.unknownFields.isNotEmpty,
    PeerMessage_Payload.ping => message.ping.unknownFields.isNotEmpty,
    PeerMessage_Payload.pong => message.pong.unknownFields.isNotEmpty,
    PeerMessage_Payload.joinGame => message.joinGame.unknownFields.isNotEmpty,
    PeerMessage_Payload.armTouch => message.armTouch.unknownFields.isNotEmpty,
    PeerMessage_Payload.setColor => message.setColor.unknownFields.isNotEmpty,
    PeerMessage_Payload.stopAll => message.stopAll.unknownFields.isNotEmpty,
    PeerMessage_Payload.simulateTouch =>
      message.simulateTouch.unknownFields.isNotEmpty,
    PeerMessage_Payload.touchEvent =>
      message.touchEvent.unknownFields.isNotEmpty,
    PeerMessage_Payload.timeoutEvent =>
      message.timeoutEvent.unknownFields.isNotEmpty,
    PeerMessage_Payload.notSet => false,
  };
}

bool _sameBytes(List<int> left, List<int> right) {
  if (left.length != right.length) return false;
  for (var index = 0; index < left.length; index++) {
    if (left[index] != right[index]) return false;
  }
  return true;
}
