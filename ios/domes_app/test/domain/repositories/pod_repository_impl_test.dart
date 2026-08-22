import 'dart:async';
import 'dart:typed_data';

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';
import 'package:domes_app/domain/repositories/pod_repository_impl.dart';
import 'package:flutter_test/flutter_test.dart';

final class _FakeTransport extends Transport {
  _FakeTransport(this.commandResponse);

  Frame commandResponse;
  int? expectedResponseType;
  int? requestType;
  Uint8List? requestPayload;
  final StreamController<Frame> events = StreamController<Frame>.broadcast();

  @override
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload, {
    required int expectedResponseType,
  }) async {
    requestType = msgType;
    requestPayload = payload;
    this.expectedResponseType = expectedResponseType;
    return commandResponse;
  }

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {}

  @override
  Future<Frame> receiveFrame(Duration timeout) =>
      throw UnsupportedError('not used');

  @override
  Future<Frame> transactFrame(
    int msgType,
    Uint8List payload,
    Duration timeout, {
    void Function()? onFrameSent,
  }) => throw UnsupportedError('not used');

  @override
  Stream<Frame> get unsolicitedFrames => events.stream;

  @override
  Future<void> disconnect() async {}

  @override
  bool get isConnected => true;
}

void main() {
  test(
    'rejects a response type mismatch even if the transport returns it',
    () async {
      final transport = _FakeTransport(
        Frame(msgType: 0x23, payload: Uint8List(0)),
      );
      final repository = PodRepositoryImpl(transport);

      await expectLater(repository.listFeatures(), throwsA(isA<StateError>()));
      expect(
        transport.expectedResponseType,
        MsgType.MSG_TYPE_LIST_FEATURES_RSP.value,
      );
      await transport.events.close();
    },
  );

  test('decodes device-originated touch events', () async {
    final transport = _FakeTransport(
      Frame(msgType: 0x21, payload: Uint8List(0)),
    );
    final repository = PodRepositoryImpl(transport);
    final eventFuture = repository.touchEvents.first;

    transport.events.add(
      Frame(
        msgType: MsgType.MSG_TYPE_TOUCH_EVENT_NTF.value,
        payload: Uint8List.fromList([0x08, 0x02, 0x10, 0x01, 0x18, 0x2a]),
      ),
    );

    final event = await eventFuture;
    expect(event.podId, 2);
    expect(event.padIndex, 1);
    expect(event.timestampUs, 42);
    await transport.events.close();
  });

  test('sets device-owned volume through generated status envelope', () async {
    final response = (SetAudioVolumeResponse()..volume = 55).writeToBuffer();
    final transport = _FakeTransport(
      Frame(
        msgType: MsgType.MSG_TYPE_SET_AUDIO_VOLUME_RSP.value,
        payload: Uint8List.fromList([Status.STATUS_OK.value, ...response]),
      ),
    );
    final repository = PodRepositoryImpl(transport);

    expect(await repository.setAudioVolume(55), 55);
    expect(transport.requestType, MsgType.MSG_TYPE_SET_AUDIO_VOLUME_REQ.value);
    expect(
      SetAudioVolumeRequest.fromBuffer(transport.requestPayload!).volume,
      55,
    );
    await transport.events.close();
  });
}
