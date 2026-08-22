import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:domes_app/application/providers/drill_provider.dart';
import 'package:domes_app/application/providers/multi_pod_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';
import 'package:domes_app/domain/models/drill_config.dart';
import 'package:domes_app/domain/models/pod_device.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

const _cycles = int.fromEnvironment('FS4_SOAK_CYCLES', defaultValue: 1000);
const _identities = <String>[
  'soak-pod-1',
  'soak-pod-2',
  'soak-pod-3',
  'soak-pod-4',
  'soak-pod-5',
  'soak-pod-6',
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

  void failAmbiguously(String stage) {
    final handler = _error;
    if (handler != null) {
      Function.apply(handler, [
        StateError('ambiguous command failure at $stage'),
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
  final _RetainedStream touchEvents;
  String? _gatedCommand;
  Completer<void>? _gate;

  void gate(String command) {
    expect(_gate, isNull);
    _gatedCommand = command;
  }

  bool get commandIsPending => _gate != null;

  void completeStaleResponse() {
    final gate = _gate;
    expect(gate, isNotNull);
    _gate = null;
    gate!.complete();
  }

  Future<void> _beforeResponse(String command) async {
    if (_gatedCommand != command) return;
    _gatedCommand = null;
    final gate = _gate = Completer<void>();
    await gate.future;
  }

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    final command = pattern.patternType == LedPatternType.LED_PATTERN_OFF
        ? 'led_off'
        : pattern.color == (0, 255, 0, 0)
        ? 'led_green'
        : 'led_red';
    await _beforeResponse(command);
    return pattern;
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    await _beforeResponse(
      mode == SystemMode.SYSTEM_MODE_GAME ? 'mode_game' : 'mode_idle',
    );
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

String _commandForStage(String stage) => switch (stage) {
  'prepare_mode' => 'mode_game',
  'prepare_clear' ||
  'hit_clear' ||
  'miss_clear' ||
  'cleanup_clear' => 'led_off',
  'arm_target' => 'led_green',
  'miss_feedback' => 'led_red',
  'cleanup_mode' => 'mode_idle',
  _ => throw StateError('unknown stage $stage'),
};

Future<void> _waitUntil(bool Function() predicate) async {
  for (var attempt = 0; attempt < 2000; attempt++) {
    if (predicate()) return;
    await Future<void>.delayed(Duration.zero);
  }
  fail('condition did not become true');
}

void main() {
  test(
    'six-identity production drill command recovery soak',
    () async {
      await runZoned(
        () async {
          expect(_cycles, greaterThanOrEqualTo(1000));
          final lifecycle = <String>[];
          final current = <String, _FakeRepository>{};
          final generations = <String, int>{};
          final identityTotals = {for (final id in _identities) id: 0};
          final stageTotals = {for (final stage in _stages) stage: 0};
          var activeSubscriptions = 0;
          var duplicateOrLostResults = 0;
          var staleMutations = 0;
          var duplicateFailureEvents = 0;
          var cleanupOrderViolations = 0;
          var healthyPeerMutations = 0;
          var quarantinedGenerationReuse = 0;
          var reconnects = 0;
          var completedResults = 0;

          final multiPod = MultiPodNotifier(
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
          final container = ProviderContainer(
            overrides: [
              drillProvider.overrideWith(
                (ref) => DrillNotifier(ref, multiPod: multiPod),
              ),
            ],
          );
          final drill = container.read(drillProvider.notifier);
          for (final identity in _identities) {
            await multiPod.connectPod(
              PodDevice(name: identity, address: identity),
            );
          }
          final failures = <PodConnectionFailure>[];
          final touches = <PodTouchEvent>[];
          final failureSubscription = multiPod.connectionFailures.listen(
            failures.add,
          );
          final touchSubscription = multiPod.touchEvents.listen(touches.add);

          Future<void> start(String identity, {int rounds = 2}) async {
            await drill.startDrill(
              DrillConfig(
                roundCount: rounds,
                timeout: const Duration(milliseconds: 1),
                minDelay: Duration.zero,
                maxDelay: Duration.zero,
                podAddresses: [identity],
              ),
            );
          }

          for (var cycle = 0; cycle < _cycles; cycle++) {
            final identity = _identities[cycle % _identities.length];
            final stage = _stages[cycle % _stages.length];
            final repository = current[identity]!;
            final generation = generations[identity]!;
            final healthyBefore = {
              for (final peer in _identities.where((id) => id != identity))
                peer: generations[peer],
            };
            final beforeLog = lifecycle.length;
            final beforeFailures = failures.length;
            final beforeTouches = touches.length;
            Future<void>? operation;

            switch (stage) {
              case 'prepare_mode':
              case 'prepare_clear':
              case 'arm_target':
                repository.gate(_commandForStage(stage));
                operation = start(identity);
                break;
              case 'hit_clear':
                await start(identity);
                await _waitUntil(
                  () => drill.state.phase == DrillPhase.waitingTouch,
                );
                final retained = List.of(drill.state.results);
                repository.gate(_commandForStage(stage));
                drill.recordTouch(identity);
                if (drill.state.results.length != retained.length + 1) {
                  duplicateOrLostResults++;
                }
                break;
              case 'miss_feedback':
                repository.gate(_commandForStage(stage));
                operation = start(identity);
                break;
              case 'miss_clear':
                await start(identity);
                await _waitUntil(
                  () => drill.state.phase == DrillPhase.waitingTouch,
                );
                repository.gate(_commandForStage(stage));
                break;
              case 'cleanup_clear':
              case 'cleanup_mode':
                await start(identity, rounds: 1);
                await _waitUntil(
                  () => drill.state.phase == DrillPhase.waitingTouch,
                );
                repository.gate(_commandForStage(stage));
                drill.recordTouch(identity);
                break;
            }
            await _waitUntil(() => repository.commandIsPending);
            final completedBeforeFault = List.of(drill.state.results);
            repository.touchEvents.failAmbiguously(stage);
            await _waitUntil(() => failures.length == beforeFailures + 1);
            final terminalAfterFault = drill.state;

            if (multiPod.state[identity]!.isConnected) {
              quarantinedGenerationReuse++;
            }
            final reconnect = multiPod.connectPod(
              PodDevice(name: identity, address: identity),
            );
            await reconnect;
            reconnects++;
            final cycleLog = lifecycle.sublist(beforeLog);
            final cancel = cycleLog.indexOf('cancel:$identity:$generation');
            final disconnect = cycleLog.indexOf(
              'disconnect:$identity:$generation',
            );
            final connect = cycleLog.indexOf(
              'connect:$identity:${generation + 1}',
            );
            if (cancel < 0 || disconnect <= cancel || connect <= disconnect) {
              cleanupOrderViolations++;
            }

            repository.touchEvents.staleTouch();
            repository.completeStaleResponse();
            await operation;
            await Future<void>.delayed(Duration.zero);
            if (touches.length != beforeTouches ||
                failures.length != beforeFailures + 1 ||
                !identical(drill.state, terminalAfterFault) ||
                drill.state.results.length != completedBeforeFault.length) {
              staleMutations++;
            }
            if (failures.length != beforeFailures + 1) {
              duplicateFailureEvents++;
            }
            for (final entry in healthyBefore.entries) {
              if (generations[entry.key] != entry.value ||
                  !multiPod.state[entry.key]!.isConnected) {
                healthyPeerMutations++;
              }
            }
            if (generations[identity] == generation ||
                !multiPod.state[identity]!.isConnected) {
              quarantinedGenerationReuse++;
            }

            await start(identity, rounds: 1);
            await _waitUntil(
              () => drill.state.phase == DrillPhase.waitingTouch,
            );
            drill.recordTouch(identity);
            await _waitUntil(() => drill.state.phase == DrillPhase.finished);
            if (drill.state.results.length != 1) {
              duplicateOrLostResults++;
            } else {
              completedResults++;
            }
            identityTotals[identity] = identityTotals[identity]! + 1;
            stageTotals[stage] = stageTotals[stage]! + 1;
          }

          if (completedResults != _cycles) duplicateOrLostResults++;
          drill.stopDrill();
          await multiPod.disconnectAll();
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
          expect(multiPod.state, isEmpty);
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
            'completed_results': completedResults,
            'per_identity': identityTotals,
            'per_stage': stageTotals,
            'terminal_state': 'disconnected',
            'invariant_counters': counters,
          };
          // ignore: avoid_print
          print('FS4_COMMAND_RECOVERY_SOAK ${jsonEncode(summary)}');

          await touchSubscription.cancel();
          await failureSubscription.cancel();
          container.dispose();
          multiPod.dispose();
        },
        zoneSpecification: ZoneSpecification(
          createTimer: (self, parent, zone, duration, callback) =>
              parent.createTimer(
                zone,
                duration == const Duration(milliseconds: 500)
                    ? Duration.zero
                    : duration,
                callback,
              ),
        ),
      );
    },
    timeout: const Timeout(Duration(minutes: 10)),
  );
}
