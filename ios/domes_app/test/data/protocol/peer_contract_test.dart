import 'dart:typed_data';

import 'package:domes_app/data/proto/generated/peer_drill.pb.dart';
import 'package:domes_app/data/protocol/peer_contract.dart';
import 'package:flutter_test/flutter_test.dart';

PeerMessage peerMessage(Object payload, PeerRole senderRole) {
  final message = PeerMessage(
    header: PeerHeader(
      version: ContractVersion.CONTRACT_VERSION_1,
      srcPodId: 1,
      dstPodId: 2,
      senderRole: senderRole,
      senderMac: [1, 2, 3, 4, 5, 6],
    ),
  );
  switch (payload) {
    case Beacon value:
      message.beacon = value;
    case Ping value:
      message.ping = value;
    case Pong value:
      message.pong = value;
    case JoinGame value:
      message.joinGame = value;
    case ArmTouch value:
      message.armTouch = value;
    case SetColor value:
      message.setColor = value;
    case StopAll value:
      message.stopAll = value;
    case SimulateTouch value:
      message.simulateTouch = value;
    case TouchEvent value:
      message.touchEvent = value;
    case TimeoutEvent value:
      message.timeoutEvent = value;
  }
  return message;
}

void main() {
  test('every generated variant round trips through the strict consumer', () {
    final cases = <(PeerMessage, PeerRole, PeerLifecycleState)>[
      (
        peerMessage(Beacon(), PeerRole.PEER_ROLE_UNSPECIFIED),
        PeerRole.PEER_ROLE_MASTER,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      (
        peerMessage(Ping(), PeerRole.PEER_ROLE_UNSPECIFIED),
        PeerRole.PEER_ROLE_MASTER,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      (
        peerMessage(Pong(), PeerRole.PEER_ROLE_UNSPECIFIED),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      (
        peerMessage(
          JoinGame(assignedRole: PeerRole.PEER_ROLE_SLAVE),
          PeerRole.PEER_ROLE_MASTER,
        ),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      (
        peerMessage(
          ArmTouch(roundToken: 1, timeoutMs: 3000, feedbackMode: 3),
          PeerRole.PEER_ROLE_MASTER,
        ),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
      (
        peerMessage(SetColor(r: 1, g: 2, b: 3), PeerRole.PEER_ROLE_MASTER),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
      (
        peerMessage(StopAll(), PeerRole.PEER_ROLE_MASTER),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
      (
        peerMessage(
          SimulateTouch(roundToken: 1, padIndex: 0),
          PeerRole.PEER_ROLE_MASTER,
        ),
        PeerRole.PEER_ROLE_SLAVE,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_ARMED,
      ),
      (
        peerMessage(
          TouchEvent(roundToken: 1, reactionTimeUs: 100, padIndex: 0),
          PeerRole.PEER_ROLE_SLAVE,
        ),
        PeerRole.PEER_ROLE_MASTER,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
      (
        peerMessage(TimeoutEvent(roundToken: 1), PeerRole.PEER_ROLE_SLAVE),
        PeerRole.PEER_ROLE_MASTER,
        PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
    ];
    for (final (expected, receiver, state) in cases) {
      expect(
        decodePeerMessage(
          Uint8List.fromList(expected.writeToBuffer()),
          receiverRole: receiver,
          state: state,
        ),
        expected,
      );
    }
  });

  test('malformed unknown truncated and oversized inputs fail closed', () {
    final bytes = peerMessage(
      Ping(),
      PeerRole.PEER_ROLE_UNSPECIFIED,
    ).writeToBuffer();
    expect(
      () => decodePeerMessage(
        Uint8List.fromList(bytes.sublist(0, bytes.length - 1)),
        receiverRole: PeerRole.PEER_ROLE_MASTER,
        state: PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      throwsFormatException,
    );
    expect(
      () => decodePeerMessage(
        Uint8List.fromList([...bytes, 0xf8, 0x01, 0x01]),
        receiverRole: PeerRole.PEER_ROLE_MASTER,
        state: PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      throwsFormatException,
    );
    expect(
      () => decodePeerMessage(
        Uint8List(65),
        receiverRole: PeerRole.PEER_ROLE_MASTER,
        state: PeerLifecycleState.PEER_LIFECYCLE_STATE_DISCOVERY,
      ),
      throwsFormatException,
    );
  });

  test('role and state invalid messages fail closed', () {
    final roleInvalid = peerMessage(
      TimeoutEvent(roundToken: 1),
      PeerRole.PEER_ROLE_MASTER,
    );
    expect(
      () => validatePeerMessage(
        roleInvalid,
        receiverRole: PeerRole.PEER_ROLE_MASTER,
        state: PeerLifecycleState.PEER_LIFECYCLE_STATE_READY,
      ),
      throwsFormatException,
    );
    final stateInvalid = peerMessage(
      ArmTouch(roundToken: 1, timeoutMs: 3000, feedbackMode: 3),
      PeerRole.PEER_ROLE_MASTER,
    );
    expect(
      () => validatePeerMessage(
        stateInvalid,
        receiverRole: PeerRole.PEER_ROLE_SLAVE,
        state: PeerLifecycleState.PEER_LIFECYCLE_STATE_ARMED,
      ),
      throwsFormatException,
    );
  });
}
