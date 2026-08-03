import 'dart:async';
import 'dart:typed_data';

import 'package:domes_app/application/providers/pod_connection_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';
import 'package:domes_app/domain/models/pod_device.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_test/flutter_test.dart';

final class _FakeTransport extends Transport {
  bool connected = true;

  @override
  Future<void> disconnect() async => connected = false;

  @override
  bool get isConnected => connected;

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
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload, {
    required int expectedResponseType,
  }) => throw UnsupportedError('not used');

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {}

  @override
  Stream<Frame> get unsolicitedFrames => const Stream.empty();
}

final class _FakeRepository implements PodRepository {
  final StreamController<AppTouchEvent> events =
      StreamController<AppTouchEvent>.broadcast();

  @override
  Stream<AppTouchEvent> get touchEvents => events.stream;

  @override
  Future<AppModeInfo> getSystemMode() => throw UnsupportedError('not used');

  @override
  Future<AppSystemInfo> getSystemInfo() => throw UnsupportedError('not used');

  @override
  Future<AppLedPattern> getLedPattern() => throw UnsupportedError('not used');

  @override
  Future<List<AppFeatureState>> listFeatures() =>
      throw UnsupportedError('not used');

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) =>
      throw UnsupportedError('not used');

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      throw UnsupportedError('not used');

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) =>
      throw UnsupportedError('not used');
}

void main() {
  const pod = PodDevice(name: 'Pod 1', address: 'pod-1');

  test(
    'disconnects a transport whose connect completion became stale',
    () async {
      final transport = _FakeTransport();
      final repository = _FakeRepository();
      final connectorCalled = Completer<void>();
      final connected =
          Completer<({Transport transport, PodRepository repository})>();
      final notifier = PodConnectionNotifier(
        connector: (_) {
          connectorCalled.complete();
          return connected.future;
        },
      );

      final connectFuture = notifier.connect(pod);
      await connectorCalled.future;
      await notifier.disconnect();
      connected.complete((transport: transport, repository: repository));
      await connectFuture;

      expect(notifier.state.isConnected, isFalse);
      expect(transport.connected, isFalse);

      notifier.dispose();
      await repository.events.close();
    },
  );

  test('stream failure disconnects and removes the active transport', () async {
    final transport = _FakeTransport();
    final repository = _FakeRepository();
    final notifier = PodConnectionNotifier(
      connector: (_) async => (transport: transport, repository: repository),
    );
    await notifier.connect(pod);

    repository.events.addError(StateError('BLE disconnected'));
    await Future<void>.delayed(Duration.zero);

    expect(notifier.state.isConnected, isFalse);
    expect(notifier.state.error, contains('BLE disconnected'));
    expect(transport.connected, isFalse);

    notifier.dispose();
    await repository.events.close();
  });
}
