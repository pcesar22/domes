import 'dart:async';
import 'dart:typed_data';

import 'package:domes_app/application/providers/multi_pod_provider.dart';
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
  final StreamController<AppTouchEvent> eventController =
      StreamController<AppTouchEvent>.broadcast();
  (SystemMode, bool) modeResult = (SystemMode.SYSTEM_MODE_GAME, true);
  AppLedPattern? ledResult;
  AppLedPattern? lastLedPattern;

  @override
  Stream<AppTouchEvent> get touchEvents => eventController.stream;

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    lastLedPattern = pattern;
    return ledResult ?? pattern;
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async => modeResult;

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
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      throw UnsupportedError('not used');
}

void main() {
  late _FakeTransport transport;
  late _FakeRepository repository;
  late MultiPodNotifier notifier;

  setUp(() async {
    transport = _FakeTransport();
    repository = _FakeRepository();
    notifier = MultiPodNotifier(
      connector: (_) async => (transport: transport, repository: repository),
    );
    await notifier.connectPod(const PodDevice(name: 'Pod 1', address: 'pod-1'));
  });

  tearDown(() async {
    notifier.dispose();
    await repository.eventController.close();
  });

  test('delegates LED updates through the typed repository', () async {
    final pattern = AppLedPattern.solid(1, 2, 3);

    await notifier.setLedPattern('pod-1', pattern);

    expect(repository.lastLedPattern, same(pattern));
  });

  test('requires a successful mode transition response', () async {
    repository.modeResult = (SystemMode.SYSTEM_MODE_IDLE, false);

    await expectLater(
      notifier.setMode('pod-1', SystemMode.SYSTEM_MODE_GAME),
      throwsA(isA<StateError>()),
    );

    repository.modeResult = (SystemMode.SYSTEM_MODE_IDLE, true);
    await expectLater(
      notifier.setMode('pod-1', SystemMode.SYSTEM_MODE_GAME),
      throwsA(isA<StateError>()),
    );
  });

  test('requires the reported LED state to match the request', () async {
    repository.ledResult = AppLedPattern.solid(9, 9, 9);

    await expectLater(
      notifier.setLedPattern('pod-1', AppLedPattern.solid(1, 2, 3)),
      throwsA(isA<StateError>()),
    );
  });

  test('associates touch events with their connection address', () async {
    final received = notifier.touchEvents.first;
    repository.eventController.add(
      const AppTouchEvent(podId: 1, padIndex: 2, timestampUs: 42),
    );

    final event = await received;
    expect(event.address, 'pod-1');
    expect(event.event.padIndex, 2);
  });

  test('marks a pod disconnected when its event stream fails', () async {
    repository.eventController.addError(StateError('BLE disconnected'));
    await Future<void>.delayed(Duration.zero);

    expect(notifier.state['pod-1']!.isConnected, isFalse);
    expect(notifier.state['pod-1']!.error, contains('BLE disconnected'));
    expect(transport.connected, isFalse);
  });
}
