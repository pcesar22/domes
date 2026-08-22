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
  _FakeTransport({this.onDisconnect});

  final Future<void> Function()? onDisconnect;
  bool connected = true;
  int disconnectCalls = 0;

  @override
  Future<void> disconnect() async {
    disconnectCalls++;
    await onDisconnect?.call();
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
  _FakeRepository({
    void Function()? onCancel,
    this.infoResult,
    this.retainedEvents,
  }) : events = StreamController<AppTouchEvent>.broadcast(onCancel: onCancel);

  final StreamController<AppTouchEvent> events;
  final Completer<AppSystemInfo>? infoResult;
  final _RetainedCallbackStream? retainedEvents;
  final List<String> calls = [];

  Future<T> _fail<T>(String operation) {
    calls.add(operation);
    return Future<T>.error(StateError('$operation failed ambiguously'));
  }

  @override
  Stream<AppTouchEvent> get touchEvents => retainedEvents ?? events.stream;

  @override
  Future<AppModeInfo> getSystemMode() => _fail('getSystemMode');

  @override
  Future<AppSystemInfo> getSystemInfo() {
    if (infoResult == null) return _fail('getSystemInfo');
    calls.add('getSystemInfo');
    return infoResult!.future;
  }

  @override
  Future<AppLedPattern> getLedPattern() => _fail('getLedPattern');

  @override
  Future<List<AppFeatureState>> listFeatures() => _fail('listFeatures');

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) =>
      _fail('setLedPattern');

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      _fail('setFeature');

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) =>
      _fail('setSystemMode');

  @override
  Future<int> getAudioVolume() => _fail('getAudioVolume');

  @override
  Future<int> setAudioVolume(int volume) => _fail('setAudioVolume');

  @override
  Future<bool> triggerFeedback(FeedbackProbe probe) => _fail('triggerFeedback');
}

final class _RetainedCallbackStream extends Stream<AppTouchEvent> {
  void Function(AppTouchEvent)? _onData;
  Function? _onError;
  int retainedDataCalls = 0;
  int retainedErrorCalls = 0;

  void emit(AppTouchEvent event) {
    final handler = _onData;
    if (handler != null) {
      retainedDataCalls++;
      handler(event);
    }
  }

  void emitError(Object error) {
    final handler = _onError;
    if (handler != null) {
      retainedErrorCalls++;
      Function.apply(handler, [error, StackTrace.current]);
    }
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
    return _RetainedCallbackSubscription();
  }
}

final class _RetainedCallbackSubscription
    implements StreamSubscription<AppTouchEvent> {
  bool _isPaused = false;

  @override
  Future<void> cancel() async {}

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

typedef _Operation = Future<Object?> Function(PodRepository repository);

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

  final operations = <String, _Operation>{
    'listFeatures': (repo) => repo.listFeatures(),
    'setFeature': (repo) => repo.setFeature(Feature.FEATURE_LED_EFFECTS, true),
    'getLedPattern': (repo) => repo.getLedPattern(),
    'setLedPattern': (repo) => repo.setLedPattern(AppLedPattern.off()),
    'getSystemMode': (repo) => repo.getSystemMode(),
    'setSystemMode': (repo) => repo.setSystemMode(SystemMode.SYSTEM_MODE_IDLE),
    'getSystemInfo': (repo) => repo.getSystemInfo(),
    'getAudioVolume': (repo) => repo.getAudioVolume(),
    'setAudioVolume': (repo) => repo.setAudioVolume(50),
    'triggerFeedback': (repo) =>
        repo.triggerFeedback(FeedbackProbe.FEEDBACK_PROBE_EMBEDDED_BEEP),
  };

  for (final operation in operations.entries) {
    test(
      '${operation.key} failure quarantines the current generation once',
      () async {
        final transport = _FakeTransport();
        final repository = _FakeRepository();
        final notifier = PodConnectionNotifier(
          connector: (_) async =>
              (transport: transport, repository: repository),
        );
        var errorTransitions = 0;
        notifier.addListener((state) {
          if (state.error != null) errorTransitions++;
        });
        await notifier.connect(pod);
        final exposed = notifier.state.repository!;

        await expectLater(operation.value(exposed), throwsStateError);
        await Future<void>.delayed(Duration.zero);

        expect(notifier.state.repository, isNull);
        expect(notifier.state.isConnected, isFalse);
        expect(errorTransitions, 1);
        expect(repository.calls, [operation.key]);
        expect(transport.disconnectCalls, 1);

        await expectLater(operation.value(exposed), throwsStateError);
        await Future<void>.delayed(Duration.zero);
        expect(errorTransitions, 1);
        expect(transport.disconnectCalls, 1);
        expect(repository.calls, [operation.key]);

        notifier.dispose();
        await repository.events.close();
      },
    );
  }

  test('reconnect waits for cancel then disconnect cleanup ordering', () async {
    final lifecycle = <String>[];
    final disconnectGate = Completer<void>();
    final oldTransport = _FakeTransport(
      onDisconnect: () async {
        lifecycle.add('disconnect');
        await disconnectGate.future;
      },
    );
    final oldRepository = _FakeRepository(
      onCancel: () => lifecycle.add('cancel'),
    );
    final replacementTransport = _FakeTransport();
    final replacementRepository = _FakeRepository();
    var connects = 0;
    final notifier = PodConnectionNotifier(
      connector: (_) async {
        connects++;
        lifecycle.add('connect:$connects');
        return connects == 1
            ? (transport: oldTransport, repository: oldRepository)
            : (
                transport: replacementTransport,
                repository: replacementRepository,
              );
      },
    );
    await notifier.connect(pod);

    await expectLater(
      notifier.state.repository!.getSystemInfo(),
      throwsStateError,
    );
    final reconnect = notifier.connect(pod);
    await Future<void>.delayed(Duration.zero);
    expect(connects, 1);
    expect(lifecycle, ['connect:1', 'cancel', 'disconnect']);

    disconnectGate.complete();
    await reconnect;
    expect(lifecycle, ['connect:1', 'cancel', 'disconnect', 'connect:2']);
    expect(notifier.state.repository, isNotNull);
    expect(notifier.state.isConnected, isTrue);

    notifier.dispose();
    await oldRepository.events.close();
    await replacementRepository.events.close();
  });

  test(
    'superseded command and stream callbacks cannot alter recovery',
    () async {
      final delayedSuccess = Completer<AppSystemInfo>();
      final delayedError = Completer<AppSystemInfo>();
      final retainedSuccessEvents = _RetainedCallbackStream();
      final retainedErrorEvents = _RetainedCallbackStream();
      final repositories = [
        _FakeRepository(
          infoResult: delayedSuccess,
          retainedEvents: retainedSuccessEvents,
        ),
        _FakeRepository(
          infoResult: delayedError,
          retainedEvents: retainedErrorEvents,
        ),
        _FakeRepository(),
      ];
      final transports = [_FakeTransport(), _FakeTransport(), _FakeTransport()];
      var next = 0;
      final notifier = PodConnectionNotifier(
        connector: (_) async {
          final index = next++;
          return (
            transport: transports[index],
            repository: repositories[index],
          );
        },
      );

      await notifier.connect(pod);
      final staleSuccess = notifier.state.repository!.getSystemInfo();
      await notifier.connect(pod);
      delayedSuccess.complete(
        const AppSystemInfo(
          firmwareVersion: 'stale',
          uptimeS: 1,
          freeHeap: 2,
          bootCount: 3,
          mode: SystemMode.SYSTEM_MODE_IDLE,
          featureMask: 0,
        ),
      );
      await staleSuccess;
      retainedSuccessEvents.emit(
        const AppTouchEvent(podId: 1, padIndex: 0, timestampUs: 1),
      );
      retainedSuccessEvents.emitError(
        StateError('retained stale stream error'),
      );
      await Future<void>.delayed(Duration.zero);
      expect(notifier.state.repository, isNotNull);
      expect(notifier.state.error, isNull);
      expect(retainedSuccessEvents.retainedDataCalls, 1);
      expect(retainedSuccessEvents.retainedErrorCalls, 1);

      final staleError = notifier.state.repository!.getSystemInfo();
      await notifier.connect(pod);
      delayedError.completeError(StateError('late command failure'));
      await expectLater(staleError, throwsStateError);
      retainedErrorEvents.emit(
        const AppTouchEvent(podId: 1, padIndex: 1, timestampUs: 2),
      );
      retainedErrorEvents.emitError(StateError('late stream failure'));
      await Future<void>.delayed(Duration.zero);

      expect(notifier.state.repository, isNotNull);
      expect(notifier.state.error, isNull);
      expect(notifier.state.isConnected, isTrue);
      expect(repositories[0].calls, ['getSystemInfo']);
      expect(repositories[1].calls, ['getSystemInfo']);
      expect(retainedErrorEvents.retainedDataCalls, 1);
      expect(retainedErrorEvents.retainedErrorCalls, 1);

      notifier.dispose();
      for (final repository in repositories) {
        await repository.events.close();
      }
    },
  );

  test('retained callbacks cannot republish after quarantine', () async {
    final retainedEvents = _RetainedCallbackStream();
    final transport = _FakeTransport();
    final repository = _FakeRepository(retainedEvents: retainedEvents);
    final notifier = PodConnectionNotifier(
      connector: (_) async => (transport: transport, repository: repository),
    );
    var errorTransitions = 0;
    notifier.addListener((state) {
      if (state.error != null) errorTransitions++;
    });
    await notifier.connect(pod);

    await expectLater(
      notifier.state.repository!.getSystemInfo(),
      throwsStateError,
    );
    final quarantinedError = notifier.state.error;
    retainedEvents.emit(
      const AppTouchEvent(podId: 1, padIndex: 0, timestampUs: 1),
    );
    retainedEvents.emitError(StateError('retained post-quarantine error'));
    await Future<void>.delayed(Duration.zero);

    expect(notifier.state.repository, isNull);
    expect(notifier.state.error, quarantinedError);
    expect(errorTransitions, 1);
    expect(transport.disconnectCalls, 1);
    expect(retainedEvents.retainedDataCalls, 1);
    expect(retainedEvents.retainedErrorCalls, 1);

    notifier.dispose();
    await repository.events.close();
  });
}
