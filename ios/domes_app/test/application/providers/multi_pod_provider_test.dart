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

final class _RetainedCallbackStream extends Stream<AppTouchEvent> {
  _RetainedCallbackStream({this.onCancel});

  final void Function()? onCancel;
  void Function(AppTouchEvent)? _onData;
  Function? _onError;

  void emit(AppTouchEvent event) => _onData?.call(event);

  void emitError(Object error, StackTrace stackTrace) {
    final handler = _onError;
    if (handler != null) Function.apply(handler, [error, stackTrace]);
  }

  @override
  StreamSubscription<AppTouchEvent> listen(
    void Function(AppTouchEvent event)? onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    _onData = onData;
    _onError = onError;
    return _RetainedCallbackSubscription(onCancel);
  }
}

final class _RetainedCallbackSubscription
    implements StreamSubscription<AppTouchEvent> {
  _RetainedCallbackSubscription(this._onCancel);

  final void Function()? _onCancel;
  bool _isPaused = false;

  @override
  Future<void> cancel() async => _onCancel?.call();

  @override
  void onData(void Function(AppTouchEvent data)? handleData) {}

  @override
  void onError(Function? handleError) {}

  @override
  void onDone(void Function()? handleDone) {}

  @override
  void pause([Future<void>? resumeSignal]) => _isPaused = true;

  @override
  void resume() => _isPaused = false;

  @override
  bool get isPaused => _isPaused;

  @override
  Future<E> asFuture<E>([E? futureValue]) => Completer<E>().future;
}

final class _FakeRepository implements PodRepository {
  _FakeRepository({
    void Function()? onListen,
    void Function()? onCancel,
    this.retainedCallbackStream,
  }) {
    eventController = StreamController<AppTouchEvent>.broadcast(
      onListen: onListen,
      onCancel: onCancel,
    );
  }

  late final StreamController<AppTouchEvent> eventController;
  final _RetainedCallbackStream? retainedCallbackStream;
  (SystemMode, bool) modeResult = (SystemMode.SYSTEM_MODE_GAME, true);
  AppLedPattern? ledResult;
  AppLedPattern? lastLedPattern;
  Object? modeError;
  Object? ledError;
  int modeCalls = 0;
  int ledCalls = 0;

  @override
  Stream<AppTouchEvent> get touchEvents =>
      retainedCallbackStream ?? eventController.stream;

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    ledCalls++;
    lastLedPattern = pattern;
    if (ledError case final error?) throw error;
    return ledResult ?? pattern;
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    modeCalls++;
    if (modeError case final error?) throw error;
    return modeResult;
  }

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

  @override
  Future<int> getAudioVolume() => throw UnsupportedError('not used');

  @override
  Future<int> setAudioVolume(int volume) => throw UnsupportedError('not used');

  @override
  Future<bool> triggerFeedback(FeedbackProbe probe) =>
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

  test(
    'rejected mode transition quarantines its connected generation',
    () async {
      repository.modeResult = (SystemMode.SYSTEM_MODE_GAME, false);

      await expectLater(
        notifier.setMode('pod-1', SystemMode.SYSTEM_MODE_GAME),
        throwsA(isA<StateError>()),
      );

      expect(repository.modeCalls, 1);
      expect(notifier.state['pod-1']!.isConnected, isFalse);
    },
  );

  test('reported mode mismatch quarantines its connected generation', () async {
    repository.modeResult = (SystemMode.SYSTEM_MODE_IDLE, true);

    await expectLater(
      notifier.setMode('pod-1', SystemMode.SYSTEM_MODE_GAME),
      throwsA(isA<StateError>()),
    );

    expect(repository.modeCalls, 1);
    expect(notifier.state['pod-1']!.isConnected, isFalse);
  });

  test('requires the reported LED state to match the request', () async {
    repository.ledResult = AppLedPattern.solid(9, 9, 9);

    await expectLater(
      notifier.setLedPattern('pod-1', AppLedPattern.solid(1, 2, 3)),
      throwsA(isA<StateError>()),
    );
  });

  test(
    'set-mode failure quarantines once and reconnects after cleanup',
    () async {
      final lifecycle = <String>[];
      final oldTransport = _FakeTransport(
        onDisconnect: () => lifecycle.add('disconnect:pod-1:1'),
      )..disconnectGate = Completer<void>();
      final staleCallbacks = _RetainedCallbackStream(
        onCancel: () => lifecycle.add('cancel:pod-1:1'),
      );
      final oldRepository = _FakeRepository(
        retainedCallbackStream: staleCallbacks,
      );
      final newTransport = _FakeTransport();
      final newRepository = _FakeRepository();
      final healthyTransport = _FakeTransport();
      final healthyRepository = _FakeRepository();
      final commandError = StateError('ambiguous mode response');
      oldRepository.modeError = commandError;
      var affectedConnections = 0;
      final localNotifier = MultiPodNotifier(
        connector: (pod) async {
          if (pod.address == 'healthy') {
            return (transport: healthyTransport, repository: healthyRepository);
          }
          affectedConnections++;
          lifecycle.add('connect:pod-1:$affectedConnections');
          return affectedConnections == 1
              ? (transport: oldTransport, repository: oldRepository)
              : (transport: newTransport, repository: newRepository);
        },
      );
      const affected = PodDevice(name: 'Pod 1', address: 'pod-1');
      const healthy = PodDevice(name: 'Healthy', address: 'healthy');
      await localNotifier.connectPod(affected);
      await localNotifier.connectPod(healthy);
      final failures = <PodConnectionFailure>[];
      final touches = <PodTouchEvent>[];
      final failureSubscription = localNotifier.connectionFailures.listen(
        failures.add,
      );
      final touchSubscription = localNotifier.touchEvents.listen(touches.add);

      final command = localNotifier.setMode(
        affected.address,
        SystemMode.SYSTEM_MODE_GAME,
      );
      final commandExpectation = expectLater(
        command,
        throwsA(same(commandError)),
      );
      await Future<void>.delayed(Duration.zero);

      expect(localNotifier.state[affected.address]!.isConnected, isFalse);
      expect(
        localNotifier.state[affected.address]!.error,
        contains('$commandError'),
      );
      expect(localNotifier.state[healthy.address]!.isConnected, isTrue);
      expect(oldRepository.modeCalls, 1, reason: 'commands are never retried');
      expect(failures, isEmpty, reason: 'failure follows cleanup');

      final reconnect = localNotifier.connectPod(affected);
      await Future<void>.delayed(Duration.zero);
      expect(affectedConnections, 1, reason: 'reconnect awaits cleanup');

      staleCallbacks.emitError(
        StateError('stale generation'),
        StackTrace.current,
      );
      staleCallbacks.emit(
        const AppTouchEvent(podId: 1, padIndex: 7, timestampUs: 101),
      );
      await Future<void>.delayed(Duration.zero);
      expect(failures, isEmpty, reason: 'stale error emits no terminal event');
      expect(touches, isEmpty, reason: 'stale touch cannot escape generation');
      expect(
        localNotifier.state[affected.address]!.error,
        contains('$commandError'),
        reason: 'stale callbacks cannot mutate disconnected state',
      );

      oldTransport.disconnectGate!.complete();
      await commandExpectation;
      await reconnect;
      await Future<void>.delayed(Duration.zero);

      expect(lifecycle, [
        'connect:pod-1:1',
        'cancel:pod-1:1',
        'disconnect:pod-1:1',
        'connect:pod-1:2',
      ]);
      expect(failures, hasLength(1));
      expect(failures.single.address, affected.address);
      expect(failures.single.error, same(commandError));
      expect(localNotifier.state[affected.address]!.isConnected, isTrue);
      expect(localNotifier.state[healthy.address]!.isConnected, isTrue);

      await localNotifier.setMode(
        affected.address,
        SystemMode.SYSTEM_MODE_GAME,
      );
      expect(newRepository.modeCalls, 1);

      await touchSubscription.cancel();
      await failureSubscription.cancel();
      localNotifier.dispose();
      await oldRepository.eventController.close();
      await newRepository.eventController.close();
      await healthyRepository.eventController.close();
    },
  );

  test(
    'set-LED failure disconnects and emits one address-bound failure',
    () async {
      final commandError = TimeoutException('LED response timed out');
      repository.ledError = commandError;
      final failures = <PodConnectionFailure>[];
      final subscription = notifier.connectionFailures.listen(failures.add);

      await expectLater(
        notifier.setLedPattern('pod-1', AppLedPattern.solid(1, 2, 3)),
        throwsA(same(commandError)),
      );
      repository.eventController.addError(StateError('stale stream failure'));
      await Future<void>.delayed(Duration.zero);

      expect(
        repository.ledCalls,
        1,
        reason: 'ambiguous commands are not retried',
      );
      expect(transport.disconnectCalls, 1);
      expect(notifier.state['pod-1']!.isConnected, isFalse);
      expect(notifier.state['pod-1']!.error, contains('$commandError'));
      expect(failures, hasLength(1));
      expect(failures.single.address, 'pod-1');
      expect(failures.single.error, same(commandError));

      await subscription.cancel();
    },
  );

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

  group('operator lifecycle failures', () {
    test(
      'disconnectPod publishes once and leaves a healthy peer connected',
      () async {
        final affectedTransport = _FakeTransport();
        final affectedRepository = _FakeRepository();
        final healthyTransport = _FakeTransport();
        final healthyRepository = _FakeRepository();
        final localNotifier = MultiPodNotifier(
          connector: (pod) async => pod.address == 'affected'
              ? (transport: affectedTransport, repository: affectedRepository)
              : (transport: healthyTransport, repository: healthyRepository),
        );
        await localNotifier.connectPod(
          const PodDevice(name: 'Affected', address: 'affected'),
        );
        await localNotifier.connectPod(
          const PodDevice(name: 'Healthy', address: 'healthy'),
        );
        final failures = <PodConnectionFailure>[];
        final subscription = localNotifier.lifecycleFailures.listen(
          failures.add,
        );

        await localNotifier.disconnectPod('affected');
        affectedRepository.eventController.addError(StateError('stale error'));
        await Future<void>.delayed(Duration.zero);

        expect(failures, hasLength(1));
        expect(failures.single.address, 'affected');
        expect(failures.single.generation, 1);
        expect(failures.single.error, isA<PodLifecycleFailure>());
        expect('${failures.single.error}', contains('affected'));
        expect(localNotifier.state['affected']!.isConnected, isFalse);
        expect(localNotifier.state['healthy']!.isConnected, isTrue);
        expect(healthyTransport.disconnectCalls, 0);

        await subscription.cancel();
        localNotifier.dispose();
        await affectedRepository.eventController.close();
        await healthyRepository.eventController.close();
      },
    );

    test('replacement publishes the superseded generation once', () async {
      final transports = [_FakeTransport(), _FakeTransport()];
      final repositories = [_FakeRepository(), _FakeRepository()];
      final healthyTransport = _FakeTransport();
      final healthyRepository = _FakeRepository();
      var affectedConnections = 0;
      final localNotifier = MultiPodNotifier(
        connector: (pod) async {
          if (pod.address == 'healthy') {
            return (transport: healthyTransport, repository: healthyRepository);
          }
          final index = affectedConnections++;
          return (
            transport: transports[index],
            repository: repositories[index],
          );
        },
      );
      const affected = PodDevice(name: 'Affected', address: 'affected');
      await localNotifier.connectPod(affected);
      await localNotifier.connectPod(
        const PodDevice(name: 'Healthy', address: 'healthy'),
      );
      final failures = <PodConnectionFailure>[];
      final subscription = localNotifier.lifecycleFailures.listen(failures.add);

      await localNotifier.connectPod(affected);
      repositories.first.eventController.addError(StateError('stale error'));
      await Future<void>.delayed(Duration.zero);

      expect(failures, hasLength(1));
      expect(failures.single.address, affected.address);
      expect(failures.single.generation, 1);
      expect('${failures.single.error}', contains('replacement connect'));
      expect(transports.first.disconnectCalls, 1);
      expect(localNotifier.state[affected.address]!.isConnected, isTrue);
      expect(localNotifier.state['healthy']!.isConnected, isTrue);
      expect(healthyTransport.disconnectCalls, 0);

      await subscription.cancel();
      localNotifier.dispose();
      for (final fakeRepository in repositories) {
        await fakeRepository.eventController.close();
      }
      await healthyRepository.eventController.close();
    });

    test(
      'disconnectAll publishes one bound event per live generation',
      () async {
        final repositories = <_FakeRepository>[];
        final localNotifier = MultiPodNotifier(
          connector: (_) async {
            final fakeRepository = _FakeRepository();
            repositories.add(fakeRepository);
            return (transport: _FakeTransport(), repository: fakeRepository);
          },
        );
        for (final address in ['pod-1', 'pod-2']) {
          await localNotifier.connectPod(
            PodDevice(name: address, address: address),
          );
        }
        final failures = <PodConnectionFailure>[];
        final subscription = localNotifier.lifecycleFailures.listen(
          failures.add,
        );

        await localNotifier.disconnectAll();
        await Future<void>.delayed(Duration.zero);

        expect(failures, hasLength(2));
        expect(failures.map((event) => event.address).toSet(), {
          'pod-1',
          'pod-2',
        });
        expect(failures.map((event) => event.generation).toSet(), {1});
        expect(
          failures.every((event) => '${event.error}'.contains('disconnectAll')),
          isTrue,
        );
        expect(localNotifier.state, isEmpty);

        await subscription.cancel();
        localNotifier.dispose();
        for (final fakeRepository in repositories) {
          await fakeRepository.eventController.close();
        }
      },
    );
  });

  test('software-only six-identity operator lifecycle regression', () async {
    const addresses = [
      'sim-pod-1',
      'sim-pod-2',
      'sim-pod-3',
      'sim-pod-4',
      'sim-pod-5',
      'sim-pod-6',
    ];
    const lifecycleCycles = 60;
    final repositories = <String, _FakeRepository>{};
    final allRepositories = <_FakeRepository>[];
    final connectionCounts = <String, int>{};
    final lifecycleLog = <String>[];
    var activeSubscriptions = 0;
    var cleanupOrderViolations = 0;
    var reconnects = 0;
    var terminalEvents = 0;
    var duplicateTerminalEvents = 0;
    var peerMutations = 0;
    final failures = <PodConnectionFailure>[];

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
    final failureSubscription = stressNotifier.lifecycleFailures.listen(
      failures.add,
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
      final failureCountBefore = failures.length;
      final peersBefore = {
        for (final peer in addresses.where((peer) => peer != address))
          peer: stressNotifier.state[peer]!.transport,
      };

      if (cycle.isEven) {
        await stressNotifier.disconnectPod(address);
        await stressNotifier.connectPod(
          PodDevice(name: address, address: address),
        );
      } else {
        await stressNotifier.connectPod(
          PodDevice(name: address, address: address),
        );
      }
      await Future<void>.delayed(Duration.zero);
      reconnects++;
      final newFailures = failures.sublist(failureCountBefore);
      terminalEvents += newFailures.length;
      if (newFailures.length != 1) duplicateTerminalEvents++;
      for (final peer in peersBefore.entries) {
        if (!identical(stressNotifier.state[peer.key]!.transport, peer.value) ||
            !stressNotifier.state[peer.key]!.isConnected) {
          peerMutations++;
        }
      }

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
    await Future<void>.delayed(Duration.zero);
    expect(activeSubscriptions, 0);
    expect(cleanupOrderViolations, 0);
    expect(reconnects, lifecycleCycles);
    expect(terminalEvents, lifecycleCycles);
    expect(duplicateTerminalEvents, 0);
    expect(peerMutations, 0);
    expect(stressNotifier.state, isEmpty);
    // Retained CI evidence: values are deterministic and intentionally omit time.
    // ignore: avoid_print
    print(
      'FS4_LIFECYCLE_SOFTWARE_ONLY identities=${addresses.length} '
      'fault_types=2 lifecycle_cycles=$lifecycleCycles '
      'terminal_events=$terminalEvents reconnects=$reconnects '
      'duplicate_terminal_events=$duplicateTerminalEvents '
      'peer_mutations=$peerMutations '
      'active_fake_subscriptions=$activeSubscriptions '
      'cleanup_order_violations=$cleanupOrderViolations terminal=disconnected',
    );

    await failureSubscription.cancel();
    stressNotifier.dispose();
    for (final fakeRepository in allRepositories) {
      await fakeRepository.eventController.close();
    }
  });
}
