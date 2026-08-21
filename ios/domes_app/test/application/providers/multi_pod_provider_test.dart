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
  _FakeTransport({this.onDisconnect});

  final void Function()? onDisconnect;
  bool connected = true;
  Completer<void>? disconnectGate;
  int disconnectCalls = 0;

  @override
  Future<void> disconnect() async {
    disconnectCalls++;
    onDisconnect?.call();
    await disconnectGate?.future;
    connected = false;
  }

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
  _FakeRepository({void Function()? onListen, void Function()? onCancel}) {
    eventController = StreamController<AppTouchEvent>.broadcast(
      onListen: onListen,
      onCancel: onCancel,
    );
  }

  late final StreamController<AppTouchEvent> eventController;
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
    final failure = notifier.connectionFailures.first;
    repository.eventController.addError(StateError('BLE disconnected'));
    await Future<void>.delayed(Duration.zero);

    expect(notifier.state['pod-1']!.isConnected, isFalse);
    expect(notifier.state['pod-1']!.error, contains('BLE disconnected'));
    expect(transport.connected, isFalse);
    expect((await failure).address, 'pod-1');
  });

  test(
    'reconnect waits for failed generation cleanup and replaces stream',
    () async {
      final oldTransport = _FakeTransport()..disconnectGate = Completer<void>();
      final oldRepository = _FakeRepository();
      final newTransport = _FakeTransport();
      final newRepository = _FakeRepository();
      var connectorCalls = 0;
      final localNotifier = MultiPodNotifier(
        connector: (_) async {
          connectorCalls++;
          return connectorCalls == 1
              ? (transport: oldTransport, repository: oldRepository)
              : (transport: newTransport, repository: newRepository);
        },
      );
      const pod = PodDevice(name: 'Pod 2', address: 'pod-2');
      await localNotifier.connectPod(pod);
      final failures = <PodConnectionFailure>[];
      final failureSubscription = localNotifier.connectionFailures.listen(
        failures.add,
      );

      oldRepository.eventController.addError(StateError('link lost'));
      await Future<void>.delayed(Duration.zero);
      final reconnect = localNotifier.connectPod(pod);
      await Future<void>.delayed(Duration.zero);

      expect(connectorCalls, 1);
      expect(oldTransport.disconnectCalls, 1);
      oldTransport.disconnectGate!.complete();
      await reconnect;
      expect(connectorCalls, 2);
      expect(localNotifier.state['pod-2']!.transport, same(newTransport));

      oldRepository.eventController.addError(StateError('stale failure'));
      await Future<void>.delayed(Duration.zero);
      expect(failures, hasLength(1));

      final touch = localNotifier.touchEvents.first;
      newRepository.eventController.add(
        const AppTouchEvent(podId: 2, padIndex: 3, timestampUs: 99),
      );
      expect((await touch).address, 'pod-2');

      await failureSubscription.cancel();
      localNotifier.dispose();
      await oldRepository.eventController.close();
      await newRepository.eventController.close();
    },
  );

  test('six-pod reconnect stress leaves no fake subscriptions', () async {
    const addresses = [
      'sim-pod-1',
      'sim-pod-2',
      'sim-pod-3',
      'sim-pod-4',
      'sim-pod-5',
      'sim-pod-6',
    ];
    const lifecycleCycles = 100;
    final repositories = <String, _FakeRepository>{};
    final allRepositories = <_FakeRepository>[];
    final connectionCounts = <String, int>{};
    final lifecycleLog = <String>[];
    var activeSubscriptions = 0;
    var cleanupOrderViolations = 0;
    var reconnects = 0;

    final stressNotifier = MultiPodNotifier(
      connector: (pod) async {
        final connection = (connectionCounts[pod.address] ?? 0) + 1;
        connectionCounts[pod.address] = connection;
        late final _FakeRepository fakeRepository;
        fakeRepository = _FakeRepository(
          onListen: () {
            activeSubscriptions++;
            lifecycleLog.add('listen:${pod.address}:$connection');
          },
          onCancel: () {
            activeSubscriptions--;
            lifecycleLog.add('cancel:${pod.address}:$connection');
          },
        );
        final fakeTransport = _FakeTransport(
          onDisconnect: () {
            lifecycleLog.add('disconnect:${pod.address}:$connection');
          },
        );
        repositories[pod.address] = fakeRepository;
        allRepositories.add(fakeRepository);
        lifecycleLog.add('connect:${pod.address}:$connection');
        return (transport: fakeTransport, repository: fakeRepository);
      },
    );

    for (final address in addresses) {
      await stressNotifier.connectPod(
        PodDevice(name: address, address: address),
      );
    }
    expect(activeSubscriptions, addresses.length);

    for (var cycle = 0; cycle < lifecycleCycles; cycle++) {
      final address = addresses[cycle % addresses.length];
      final before = lifecycleLog.length;
      final failure = stressNotifier.connectionFailures.firstWhere(
        (event) => event.address == address,
      );

      repositories[address]!.eventController.addError(
        StateError('deterministic failure cycle $cycle'),
      );
      await failure;
      await stressNotifier.connectPod(
        PodDevice(name: address, address: address),
      );
      reconnects++;

      final cycleLog = lifecycleLog.sublist(before);
      final cancelIndex = cycleLog.indexWhere(
        (entry) => entry.startsWith('cancel:$address:'),
      );
      final disconnectIndex = cycleLog.indexWhere(
        (entry) => entry.startsWith('disconnect:$address:'),
      );
      final connectIndex = cycleLog.lastIndexWhere(
        (entry) => entry.startsWith('connect:$address:'),
      );
      if (cancelIndex < 0 ||
          disconnectIndex <= cancelIndex ||
          connectIndex <= disconnectIndex) {
        cleanupOrderViolations++;
      }
      expect(
        activeSubscriptions,
        addresses.length,
        reason: 'cycle=$cycle address=$address log=$cycleLog',
      );
    }

    await stressNotifier.disconnectAll();
    expect(activeSubscriptions, 0);
    expect(cleanupOrderViolations, 0);
    expect(reconnects, lifecycleCycles);
    expect(stressNotifier.state, isEmpty);
    // Retained CI evidence: values are deterministic and intentionally omit time.
    // ignore: avoid_print
    print(
      'FS4_MULTI_POD_STRESS identities=${addresses.length} '
      'lifecycle_cycles=$lifecycleCycles reconnects=$reconnects '
      'active_fake_subscriptions=$activeSubscriptions '
      'cleanup_order_violations=$cleanupOrderViolations terminal=disconnected',
    );

    stressNotifier.dispose();
    for (final fakeRepository in allRepositories) {
      await fakeRepository.eventController.close();
    }
  });
}
