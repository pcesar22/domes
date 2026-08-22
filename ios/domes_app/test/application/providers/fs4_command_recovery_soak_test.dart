import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:domes_app/application/providers/multi_pod_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';
import 'package:domes_app/domain/models/pod_device.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_test/flutter_test.dart';

const _cycles = int.fromEnvironment('FS4_SOAK_CYCLES', defaultValue: 1000);
const _identities = <String>[
  'sim-pod-1',
  'sim-pod-2',
  'sim-pod-3',
  'sim-pod-4',
  'sim-pod-5',
  'sim-pod-6',
];
const _stages = <String>[
  'prepare_mode',
  'prepare_clear',
  'arm_target',
  'hit_clear',
  'miss_feedback',
  'miss_clear',
  'cleanup_clear',
  'cleanup_mode',
];

final class _RetainedStream extends Stream<AppTouchEvent> {
  _RetainedStream(this.onCancel);

  final void Function() onCancel;
  void Function(AppTouchEvent)? _data;
  Function? _error;

  void staleTouch() =>
      _data?.call(const AppTouchEvent(podId: 99, padIndex: 7, timestampUs: 1));

  void staleError() {
    final handler = _error;
    if (handler != null) {
      Function.apply(handler, [
        StateError('stale callback'),
        StackTrace.current,
      ]);
    }
  }

  @override
  StreamSubscription<AppTouchEvent> listen(
    void Function(AppTouchEvent)? onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    _data = onData;
    _error = onError;
    return _Subscription(onCancel);
  }
}

final class _Subscription implements StreamSubscription<AppTouchEvent> {
  _Subscription(this._onCancel);
  final void Function() _onCancel;

  @override
  Future<void> cancel() async => _onCancel();
  @override
  void onData(void Function(AppTouchEvent data)? handleData) {}
  @override
  void onError(Function? handleError) {}
  @override
  void onDone(void Function()? handleDone) {}
  @override
  void pause([Future<void>? resumeSignal]) {}
  @override
  void resume() {}
  @override
  bool get isPaused => false;
  @override
  Future<E> asFuture<E>([E? futureValue]) => Completer<E>().future;
}

final class _FakeTransport extends Transport {
  _FakeTransport(this.onDisconnect);
  final void Function() onDisconnect;
  bool connected = true;

  @override
  Future<void> disconnect() async {
    onDisconnect();
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
  _FakeRepository(this.touchEvents);
  @override
  final Stream<AppTouchEvent> touchEvents;
  Object? nextFailure;
  int commandCalls = 0;

  void _check() {
    commandCalls++;
    final failure = nextFailure;
    nextFailure = null;
    if (failure != null) throw failure;
  }

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    _check();
    return pattern;
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    _check();
    return (mode, true);
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

Future<void> _command(
  MultiPodNotifier notifier,
  String address,
  String stage,
) => stage.endsWith('mode')
    ? notifier.setMode(
        address,
        stage == 'cleanup_mode'
            ? SystemMode.SYSTEM_MODE_IDLE
            : SystemMode.SYSTEM_MODE_GAME,
      )
    : notifier.setLedPattern(
        address,
        stage == 'arm_target'
            ? AppLedPattern.solid(0, 255, 0)
            : stage == 'miss_feedback'
            ? AppLedPattern.solid(255, 0, 0)
            : AppLedPattern.off(),
      );

void main() {
  test('six-identity command quarantine and recovery soak', () async {
    expect(_cycles, greaterThanOrEqualTo(1000));
    final lifecycle = <String>[];
    final current = <String, _FakeRepository>{};
    final retained = <_RetainedStream>[];
    final generations = <String, int>{};
    final identityTotals = {for (final id in _identities) id: 0};
    final stageTotals = {for (final stage in _stages) stage: 0};
    final results = <String>{};
    var activeSubscriptions = 0;
    var duplicateOrLostResults = 0;
    var staleMutations = 0;
    var duplicateFailureEvents = 0;
    var cleanupOrderViolations = 0;
    var healthyPeerMutations = 0;
    var quarantinedGenerationReuse = 0;
    var reconnects = 0;

    final notifier = MultiPodNotifier(
      connector: (pod) async {
        final generation = (generations[pod.address] ?? 0) + 1;
        generations[pod.address] = generation;
        late final _RetainedStream stream;
        stream = _RetainedStream(() {
          activeSubscriptions--;
          lifecycle.add('cancel:${pod.address}:$generation');
        });
        final repository = _FakeRepository(stream);
        current[pod.address] = repository;
        retained.add(stream);
        activeSubscriptions++;
        lifecycle.add('connect:${pod.address}:$generation');
        return (
          transport: _FakeTransport(
            () => lifecycle.add('disconnect:${pod.address}:$generation'),
          ),
          repository: repository,
        );
      },
    );
    for (final identity in _identities) {
      await notifier.connectPod(PodDevice(name: identity, address: identity));
    }
    final failures = <PodConnectionFailure>[];
    final touches = <PodTouchEvent>[];
    final failureSubscription = notifier.connectionFailures.listen(
      failures.add,
    );
    final touchSubscription = notifier.touchEvents.listen(touches.add);

    for (var cycle = 0; cycle < _cycles; cycle++) {
      final identity = _identities[cycle % _identities.length];
      final stage = _stages[cycle % _stages.length];
      final healthyBefore = {
        for (final peer in _identities.where((id) => id != identity))
          peer: generations[peer],
      };
      final generation = generations[identity]!;
      final resultId = '$cycle:$identity:$stage';
      if (!results.add(resultId)) duplicateOrLostResults++;
      identityTotals[identity] = identityTotals[identity]! + 1;
      stageTotals[stage] = stageTotals[stage]! + 1;
      final beforeLog = lifecycle.length;
      final beforeFailures = failures.length;
      final beforeTouches = touches.length;
      final oldStream = retained.lastWhere(
        (stream) => identical(current[identity]!.touchEvents, stream),
      );
      current[identity]!.nextFailure = StateError(
        'ambiguous command cycle=$cycle stage=$stage',
      );

      await expectLater(_command(notifier, identity, stage), throwsStateError);
      await Future<void>.delayed(Duration.zero);
      if (failures.length != beforeFailures + 1) duplicateFailureEvents++;
      if (notifier.state[identity]!.isConnected) quarantinedGenerationReuse++;

      oldStream.staleTouch();
      oldStream.staleError();
      await Future<void>.delayed(Duration.zero);
      if (touches.length != beforeTouches ||
          failures.length != beforeFailures + 1) {
        staleMutations++;
      }

      await notifier.connectPod(PodDevice(name: identity, address: identity));
      reconnects++;
      final cycleLog = lifecycle.sublist(beforeLog);
      final cancel = cycleLog.indexOf('cancel:$identity:$generation');
      final disconnect = cycleLog.indexOf('disconnect:$identity:$generation');
      final connect = cycleLog.indexOf('connect:$identity:${generation + 1}');
      if (cancel < 0 || disconnect <= cancel || connect <= disconnect) {
        cleanupOrderViolations++;
      }
      for (final entry in healthyBefore.entries) {
        if (generations[entry.key] != entry.value ||
            !notifier.state[entry.key]!.isConnected) {
          healthyPeerMutations++;
        }
      }
      await _command(notifier, identity, stage);
      if (generations[identity] == generation ||
          !notifier.state[identity]!.isConnected) {
        quarantinedGenerationReuse++;
      }
    }

    if (results.length != _cycles) duplicateOrLostResults++;
    await notifier.disconnectAll();
    final counters = <String, int>{
      'duplicate_or_lost_results': duplicateOrLostResults,
      'stale_mutations': staleMutations,
      'duplicate_failure_events': duplicateFailureEvents,
      'cleanup_order_violations': cleanupOrderViolations,
      'leaked_subscriptions': activeSubscriptions,
      'healthy_peer_mutations': healthyPeerMutations,
      'quarantined_generation_reuse': quarantinedGenerationReuse,
    };
    expect(counters.values, everyElement(0));
    expect(reconnects, _cycles);
    expect(notifier.state, isEmpty);
    expect(failures, hasLength(_cycles));
    expect(touches, isEmpty);

    final summary = <String, Object>{
      'schema_version': 1,
      'scenario': 'fs4_command_recovery_soak',
      'identities': _identities,
      'stages': _stages,
      'cycles': _cycles,
      'faults': _cycles,
      'reconnects': reconnects,
      'completed_results': results.length,
      'per_identity': identityTotals,
      'per_stage': stageTotals,
      'terminal_state': 'disconnected',
      'invariant_counters': counters,
    };
    // The runner extracts this single canonical payload from the raw Flutter log.
    // ignore: avoid_print
    print('FS4_COMMAND_RECOVERY_SOAK ${jsonEncode(summary)}');

    await touchSubscription.cancel();
    await failureSubscription.cancel();
    notifier.dispose();
  });
}
