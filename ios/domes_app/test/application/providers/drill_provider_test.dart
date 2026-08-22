import 'dart:async';
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

final class _FakeMultiPodNotifier extends MultiPodNotifier {
  final StreamController<PodTouchEvent> events =
      StreamController<PodTouchEvent>.broadcast();
  final StreamController<PodConnectionFailure> failures =
      StreamController<PodConnectionFailure>.broadcast();
  final StreamController<PodConnectionFailure> lifecycleEvents =
      StreamController<PodConnectionFailure>.broadcast();
  final List<SystemMode> modeCalls = [];
  final List<AppLedPattern> ledCalls = [];
  final List<(String, SystemMode)> addressedModeCalls = [];
  final List<(String, AppLedPattern)> addressedLedCalls = [];
  final Map<String, int> connectionGenerations = {};
  Completer<void>? gameModeGate;
  Completer<void>? idleModeGate;
  Completer<void>? nextLedOffGate;

  @override
  Stream<PodTouchEvent> get touchEvents => events.stream;

  @override
  Stream<PodConnectionFailure> get connectionFailures => failures.stream;

  @override
  Stream<PodConnectionFailure> get lifecycleFailures => lifecycleEvents.stream;

  @override
  int? activeConnectionGeneration(String address) =>
      connectionGenerations[address] ?? 1;

  @override
  Future<void> setMode(String address, SystemMode mode) async {
    modeCalls.add(mode);
    addressedModeCalls.add((address, mode));
    if (mode == SystemMode.SYSTEM_MODE_GAME) {
      await gameModeGate?.future;
    } else if (mode == SystemMode.SYSTEM_MODE_IDLE) {
      await idleModeGate?.future;
    }
  }

  @override
  Future<void> setLedPattern(String address, AppLedPattern pattern) async {
    ledCalls.add(pattern);
    addressedLedCalls.add((address, pattern));
    if (pattern.patternType == LedPatternType.LED_PATTERN_OFF &&
        nextLedOffGate != null) {
      final gate = nextLedOffGate!;
      nextLedOffGate = null;
      await gate.future;
    }
  }

  void fail(String address, [Object? error, int generation = 1]) {
    failures.add(
      PodConnectionFailure(
        address: address,
        generation: generation,
        error: error ?? StateError('connection lost'),
        stackTrace: StackTrace.current,
      ),
    );
  }

  void lifecycle(String address, String action, [int generation = 1]) {
    lifecycleEvents.add(
      PodConnectionFailure(
        address: address,
        generation: generation,
        error: PodLifecycleFailure(address: address, action: action),
        stackTrace: StackTrace.current,
      ),
    );
  }

  int get greenCalls =>
      ledCalls.where((pattern) => pattern.color == (0, 255, 0, 0)).length;
}

final class _LifecycleTransport extends Transport {
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

final class _LifecycleRepository implements PodRepository {
  final StreamController<AppTouchEvent> events =
      StreamController<AppTouchEvent>.broadcast();
  Completer<void>? modeGate;
  Completer<void>? nextLedGate;

  @override
  Stream<AppTouchEvent> get touchEvents => events.stream;

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    final gate = nextLedGate;
    nextLedGate = null;
    await gate?.future;
    return pattern;
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    await modeGate?.future;
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

  Future<void> close() => events.close();
}

void main() {
  late ProviderContainer container;
  late DrillNotifier notifier;

  setUp(() {
    container = ProviderContainer();
    notifier = container.read(drillProvider.notifier);
  });

  tearDown(() {
    container.dispose();
  });

  group('DrillNotifier initial state', () {
    test('starts in idle phase', () {
      expect(container.read(drillProvider).phase, DrillPhase.idle);
    });

    test('isRunning is false when idle', () {
      expect(container.read(drillProvider).isRunning, isFalse);
    });

    test('has no results', () {
      expect(container.read(drillProvider).results, isEmpty);
    });

    test('config is null', () {
      expect(container.read(drillProvider).config, isNull);
    });

    test('lastReactionTime is null', () {
      expect(container.read(drillProvider).lastReactionTime, isNull);
    });
  });

  group('DrillNotifier.startDrill', () {
    test('transitions from idle to preparing', () async {
      final config = DrillConfig(
        type: DrillType.reaction,
        roundCount: 5,
        podAddresses: ['sim-pod-1'],
      );

      await notifier.startDrill(config);

      final state = container.read(drillProvider);
      expect(state.config, isNotNull);
      expect(state.config!.type, DrillType.reaction);
      expect(state.config!.roundCount, 5);
    });

    test('rejects invalid configs before scheduling a round', () async {
      for (final config in [
        const DrillConfig(podAddresses: []),
        const DrillConfig(roundCount: 0, podAddresses: ['sim-pod-1']),
        const DrillConfig(timeout: Duration.zero, podAddresses: ['sim-pod-1']),
        const DrillConfig(
          minDelay: Duration(seconds: 2),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['sim-pod-1'],
        ),
      ]) {
        await notifier.startDrill(config);
        expect(container.read(drillProvider).phase, DrillPhase.error);
        notifier.reset();
      }
    });

    test('equal delays arm deterministically without nextInt(0)', () async {
      const config = DrillConfig(
        roundCount: 1,
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: ['sim-pod-1'],
      );

      await notifier.startDrill(config);
      await Future<void>.delayed(Duration.zero);

      expect(container.read(drillProvider).phase, DrillPhase.waitingTouch);
    });

    test('does not start if already running', () async {
      final config = DrillConfig(
        type: DrillType.reaction,
        roundCount: 5,
        podAddresses: ['sim-pod-1'],
      );

      await notifier.startDrill(config);
      final stateAfterFirst = container.read(drillProvider);

      // Trying to start again should be a no-op if running
      // (but since error state isn't "running", this tests the guard)
      if (stateAfterFirst.isRunning) {
        await notifier.startDrill(config);
        // State shouldn't have reset
        expect(container.read(drillProvider).results, isEmpty);
      }
    });
  });

  group('DrillNotifier.stopDrill', () {
    test('resets to idle when no results', () {
      notifier.stopDrill();
      final state = container.read(drillProvider);
      expect(state.phase, DrillPhase.idle);
      expect(state.results, isEmpty);
    });
  });

  group('DrillNotifier hardware lifecycle', () {
    late _FakeMultiPodNotifier multiPod;
    late ProviderContainer hardwareContainer;
    late DrillNotifier hardwareNotifier;

    setUp(() {
      multiPod = _FakeMultiPodNotifier();
      hardwareContainer = ProviderContainer(
        overrides: [
          drillProvider.overrideWith(
            (ref) => DrillNotifier(ref, multiPod: multiPod),
          ),
        ],
      );
      hardwareNotifier = hardwareContainer.read(drillProvider.notifier);
    });

    tearDown(() async {
      hardwareContainer.dispose();
      await multiPod.events.close();
      await multiPod.failures.close();
      await multiPod.lifecycleEvents.close();
      multiPod.dispose();
    });

    test('zero-result stop still turns LEDs off and returns to IDLE', () async {
      const config = DrillConfig(
        minDelay: Duration(seconds: 1),
        maxDelay: Duration(seconds: 1),
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);

      hardwareNotifier.stopDrill();
      await Future<void>.delayed(Duration.zero);

      expect(multiPod.modeCalls, contains(SystemMode.SYSTEM_MODE_IDLE));
      expect(
        multiPod.ledCalls.last.patternType,
        LedPatternType.LED_PATTERN_OFF,
      );
    });

    test('restart waits for prior cleanup to reach IDLE', () async {
      const config = DrillConfig(
        minDelay: Duration(seconds: 1),
        maxDelay: Duration(seconds: 1),
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);
      multiPod.idleModeGate = Completer<void>();
      hardwareNotifier.stopDrill();

      final restart = hardwareNotifier.startDrill(config);
      await Future<void>.delayed(Duration.zero);
      expect(
        multiPod.modeCalls.where((mode) => mode == SystemMode.SYSTEM_MODE_GAME),
        hasLength(1),
      );

      multiPod.idleModeGate!.complete();
      await restart;
      expect(multiPod.modeCalls, [
        SystemMode.SYSTEM_MODE_GAME,
        SystemMode.SYSTEM_MODE_IDLE,
        SystemMode.SYSTEM_MODE_GAME,
      ]);
    });

    test('miss feedback clears before a subsequent round arms', () async {
      const config = DrillConfig(
        roundCount: 2,
        timeout: Duration(milliseconds: 10),
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);
      await Future<void>.delayed(const Duration(milliseconds: 30));
      expect(multiPod.greenCalls, 1);

      await Future<void>.delayed(const Duration(milliseconds: 200));
      expect(multiPod.greenCalls, 1);

      await Future<void>.delayed(const Duration(milliseconds: 350));
      expect(multiPod.greenCalls, 2);
    });

    test('operator lifecycle is terminal in every active phase', () async {
      Future<void> expectFailure({
        required DrillPhase phase,
        required String action,
        required Future<void> Function(DrillConfig config) reachPhase,
      }) async {
        hardwareNotifier.reset();
        await Future<void>.delayed(Duration.zero);
        const config = DrillConfig(
          roundCount: 3,
          timeout: Duration(seconds: 1),
          minDelay: Duration(seconds: 1),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['pod-1', 'pod-2'],
        );
        var terminalErrors = 0;
        final subscription = hardwareContainer.listen<DrillState>(
          drillProvider,
          (_, next) {
            if (next.phase == DrillPhase.error) terminalErrors++;
          },
        );

        await reachPhase(config);
        expect(hardwareContainer.read(drillProvider).phase, phase);
        final completed = List.of(
          hardwareContainer.read(drillProvider).results,
        );
        final failedAddress =
            hardwareContainer.read(drillProvider).activePodAddress ?? 'pod-1';
        multiPod.lifecycle(failedAddress, action);
        await Future<void>.delayed(Duration.zero);

        final failed = hardwareContainer.read(drillProvider);
        expect(failed.phase, DrillPhase.error);
        expect(failed.errorMessage, contains(failedAddress));
        expect(failed.results, completed);
        expect(terminalErrors, 1);

        multiPod.gameModeGate?.complete();
        multiPod.gameModeGate = null;
        multiPod.nextLedOffGate?.complete();
        multiPod.nextLedOffGate = null;
        await Future<void>.delayed(const Duration(milliseconds: 20));
        expect(hardwareContainer.read(drillProvider).phase, DrillPhase.error);
        expect(hardwareContainer.read(drillProvider).results, completed);
        subscription.close();
      }

      for (final action in [
        'disconnectPod',
        'disconnectAll',
        'replacement connect',
      ]) {
        await expectFailure(
          phase: DrillPhase.preparing,
          action: action,
          reachPhase: (config) async {
            multiPod.gameModeGate = Completer<void>();
            unawaited(hardwareNotifier.startDrill(config));
            await Future<void>.delayed(Duration.zero);
          },
        );
        await expectFailure(
          phase: DrillPhase.waitingDelay,
          action: action,
          reachPhase: hardwareNotifier.startDrill,
        );
        await expectFailure(
          phase: DrillPhase.waitingTouch,
          action: action,
          reachPhase: (config) async {
            final immediate = DrillConfig(
              roundCount: config.roundCount,
              timeout: config.timeout,
              minDelay: Duration.zero,
              maxDelay: Duration.zero,
              podAddresses: config.podAddresses,
            );
            await hardwareNotifier.startDrill(immediate);
            await Future<void>.delayed(Duration.zero);
          },
        );
        await expectFailure(
          phase: DrillPhase.roundComplete,
          action: action,
          reachPhase: (config) async {
            final immediate = DrillConfig(
              roundCount: config.roundCount,
              timeout: config.timeout,
              minDelay: Duration.zero,
              maxDelay: Duration.zero,
              podAddresses: const ['pod-1'],
            );
            await hardwareNotifier.startDrill(immediate);
            await Future<void>.delayed(Duration.zero);
            multiPod.nextLedOffGate = Completer<void>();
            hardwareNotifier.recordTouch('pod-1');
          },
        );
      }

      final idleParticipants = multiPod.addressedModeCalls
          .where((call) => call.$2 == SystemMode.SYSTEM_MODE_IDLE)
          .map((call) => call.$1)
          .toSet();
      expect(idleParticipants, containsAll(['pod-1', 'pod-2']));
    });

    test(
      'participating stream failure is terminal in every active phase',
      () async {
        Future<void> expectFailure({
          required DrillPhase phase,
          required Future<void> Function(DrillConfig config) reachPhase,
        }) async {
          hardwareNotifier.reset();
          await Future<void>.delayed(Duration.zero);
          const config = DrillConfig(
            roundCount: 3,
            timeout: Duration(seconds: 1),
            minDelay: Duration(seconds: 1),
            maxDelay: Duration(seconds: 1),
            podAddresses: ['pod-1', 'pod-2'],
          );
          var terminalErrors = 0;
          final subscription = hardwareContainer.listen<DrillState>(
            drillProvider,
            (_, next) {
              if (next.phase == DrillPhase.error) terminalErrors++;
            },
          );

          await reachPhase(config);
          expect(hardwareContainer.read(drillProvider).phase, phase);
          final completed = List.of(
            hardwareContainer.read(drillProvider).results,
          );
          final failedAddress =
              hardwareContainer.read(drillProvider).activePodAddress ?? 'pod-1';
          multiPod.fail(failedAddress);
          await Future<void>.delayed(Duration.zero);

          final failed = hardwareContainer.read(drillProvider);
          expect(failed.phase, DrillPhase.error);
          expect(failed.errorMessage, contains(failedAddress));
          expect(failed.results, completed);
          expect(terminalErrors, 1);

          multiPod.gameModeGate?.complete();
          multiPod.gameModeGate = null;
          multiPod.nextLedOffGate?.complete();
          multiPod.nextLedOffGate = null;
          await Future<void>.delayed(const Duration(milliseconds: 20));
          expect(hardwareContainer.read(drillProvider).phase, DrillPhase.error);
          expect(hardwareContainer.read(drillProvider).results, completed);
          subscription.close();
        }

        await expectFailure(
          phase: DrillPhase.preparing,
          reachPhase: (config) async {
            multiPod.gameModeGate = Completer<void>();
            unawaited(hardwareNotifier.startDrill(config));
            await Future<void>.delayed(Duration.zero);
          },
        );
        await expectFailure(
          phase: DrillPhase.waitingDelay,
          reachPhase: hardwareNotifier.startDrill,
        );
        await expectFailure(
          phase: DrillPhase.waitingTouch,
          reachPhase: (config) async {
            final immediate = DrillConfig(
              roundCount: config.roundCount,
              timeout: config.timeout,
              minDelay: Duration.zero,
              maxDelay: Duration.zero,
              podAddresses: config.podAddresses,
            );
            await hardwareNotifier.startDrill(immediate);
            await Future<void>.delayed(Duration.zero);
          },
        );
        await expectFailure(
          phase: DrillPhase.roundComplete,
          reachPhase: (config) async {
            final immediate = DrillConfig(
              roundCount: config.roundCount,
              timeout: config.timeout,
              minDelay: Duration.zero,
              maxDelay: Duration.zero,
              podAddresses: const ['pod-1'],
            );
            await hardwareNotifier.startDrill(immediate);
            await Future<void>.delayed(Duration.zero);
            multiPod.nextLedOffGate = Completer<void>();
            hardwareNotifier.recordTouch('pod-1');
          },
        );

        final idleParticipants = multiPod.addressedModeCalls
            .where((call) => call.$2 == SystemMode.SYSTEM_MODE_IDLE)
            .map((call) => call.$1)
            .toSet();
        expect(idleParticipants, containsAll(['pod-1', 'pod-2']));
      },
    );

    test(
      'preparation command failure identifies pod before stream failure',
      () async {
        const config = DrillConfig(
          minDelay: Duration(seconds: 1),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['pod-1'],
        );
        var terminalErrors = 0;
        final subscription = hardwareContainer.listen<DrillState>(
          drillProvider,
          (_, next) {
            if (next.phase == DrillPhase.error) terminalErrors++;
          },
        );
        multiPod.gameModeGate = Completer<void>();

        final start = hardwareNotifier.startDrill(config);
        await Future<void>.delayed(Duration.zero);
        multiPod.gameModeGate!.completeError(StateError('link reset'));
        await start;

        final commandFailure = hardwareContainer.read(drillProvider);
        expect(commandFailure.phase, DrillPhase.error);
        expect(commandFailure.errorMessage, contains('pod-1'));
        expect(commandFailure.results, isEmpty);
        expect(terminalErrors, 1);

        multiPod.fail('pod-1', StateError('connection lost'));
        await Future<void>.delayed(Duration.zero);

        expect(hardwareContainer.read(drillProvider), same(commandFailure));
        expect(terminalErrors, 1);
        subscription.close();
      },
    );

    test('non-participating failure does not mutate active drill', () async {
      const config = DrillConfig(
        minDelay: Duration(seconds: 1),
        maxDelay: Duration(seconds: 1),
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);
      final before = hardwareContainer.read(drillProvider);

      multiPod.fail('pod-outside');
      await Future<void>.delayed(Duration.zero);

      expect(hardwareContainer.read(drillProvider), same(before));
    });

    test('non-participating lifecycle does not mutate active drill', () async {
      const config = DrillConfig(
        minDelay: Duration(seconds: 1),
        maxDelay: Duration(seconds: 1),
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);
      final before = hardwareContainer.read(drillProvider);

      multiPod.lifecycle('pod-outside', 'replacement connect');
      await Future<void>.delayed(Duration.zero);

      expect(hardwareContainer.read(drillProvider), same(before));
    });

    test(
      'queued lifecycle from prior generation cannot fail restart',
      () async {
        const config = DrillConfig(
          minDelay: Duration(seconds: 1),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['pod-1'],
        );
        multiPod.connectionGenerations['pod-1'] = 1;
        await hardwareNotifier.startDrill(config);

        multiPod.lifecycle('pod-1', 'replacement connect', 1);
        hardwareNotifier.stopDrill();
        multiPod.connectionGenerations['pod-1'] = 2;
        unawaited(hardwareNotifier.startDrill(config));
        await Future<void>.delayed(Duration.zero);

        final restarted = hardwareContainer.read(drillProvider);
        expect(restarted.isRunning, isTrue);
        expect(restarted.phase, isNot(DrillPhase.error));
        expect(restarted.errorMessage, isNull);
      },
    );

    test(
      'lifecycle events cannot mutate inactive or terminal drills',
      () async {
        const delayed = DrillConfig(
          minDelay: Duration(seconds: 1),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['pod-1'],
        );
        const immediate = DrillConfig(
          roundCount: 1,
          minDelay: Duration.zero,
          maxDelay: Duration.zero,
          podAddresses: ['pod-1'],
        );

        final idle = hardwareContainer.read(drillProvider);
        multiPod.lifecycle('pod-1', 'disconnectPod');
        await Future<void>.delayed(Duration.zero);
        expect(hardwareContainer.read(drillProvider), same(idle));

        await hardwareNotifier.startDrill(delayed);
        hardwareNotifier.stopDrill();
        final stopped = hardwareContainer.read(drillProvider);
        multiPod.lifecycle('pod-1', 'disconnectAll', 2);
        await Future<void>.delayed(Duration.zero);
        expect(hardwareContainer.read(drillProvider), same(stopped));

        await hardwareNotifier.startDrill(delayed);
        hardwareNotifier.reset();
        final reset = hardwareContainer.read(drillProvider);
        multiPod.lifecycle('pod-1', 'replacement connect', 3);
        await Future<void>.delayed(Duration.zero);
        expect(hardwareContainer.read(drillProvider), same(reset));

        await hardwareNotifier.startDrill(immediate);
        await Future<void>.delayed(Duration.zero);
        hardwareNotifier.recordTouch('pod-1');
        final finished = hardwareContainer.read(drillProvider);
        expect(finished.phase, DrillPhase.finished);
        multiPod.lifecycle('pod-1', 'disconnectPod', 4);
        await Future<void>.delayed(Duration.zero);
        expect(hardwareContainer.read(drillProvider), same(finished));

        await hardwareNotifier.startDrill(delayed);
        multiPod.lifecycle('pod-1', 'disconnectPod');
        await Future<void>.delayed(Duration.zero);
        final failed = hardwareContainer.read(drillProvider);
        expect(failed.phase, DrillPhase.error);
        multiPod.lifecycle('pod-1', 'replacement connect', 6);
        await Future<void>.delayed(Duration.zero);
        expect(hardwareContainer.read(drillProvider), same(failed));
      },
    );

    test(
      'lifecycle event after dispose cannot mutate retained state',
      () async {
        final disposedMultiPod = _FakeMultiPodNotifier();
        final disposedContainer = ProviderContainer(
          overrides: [
            drillProvider.overrideWith(
              (ref) => DrillNotifier(ref, multiPod: disposedMultiPod),
            ),
          ],
        );
        final disposedNotifier = disposedContainer.read(drillProvider.notifier);
        var stateMutations = 0;
        DrillState? lastState;
        disposedContainer.listen<DrillState>(drillProvider, (_, next) {
          stateMutations++;
          lastState = next;
        });
        const config = DrillConfig(
          minDelay: Duration(seconds: 1),
          maxDelay: Duration(seconds: 1),
          podAddresses: ['pod-1'],
        );
        await disposedNotifier.startDrill(config);
        final mutationsBeforeDispose = stateMutations;
        final before = lastState;

        disposedContainer.dispose();
        disposedMultiPod.lifecycle('pod-1', 'disconnectAll');
        await Future<void>.delayed(Duration.zero);

        expect(stateMutations, mutationsBeforeDispose);
        expect(lastState, same(before));
        await disposedMultiPod.events.close();
        await disposedMultiPod.failures.close();
        await disposedMultiPod.lifecycleEvents.close();
        disposedMultiPod.dispose();
      },
    );

    test('restart after failure waits for cleanup and can finish', () async {
      const config = DrillConfig(
        roundCount: 1,
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: ['pod-1'],
      );
      await hardwareNotifier.startDrill(config);
      await Future<void>.delayed(Duration.zero);
      multiPod.idleModeGate = Completer<void>();
      multiPod.fail('pod-1');
      await Future<void>.delayed(Duration.zero);

      final restart = hardwareNotifier.startDrill(config);
      await Future<void>.delayed(Duration.zero);
      expect(
        multiPod.modeCalls.where((mode) => mode == SystemMode.SYSTEM_MODE_GAME),
        hasLength(1),
      );

      multiPod.idleModeGate!.complete();
      await restart;
      await Future<void>.delayed(Duration.zero);
      hardwareNotifier.recordTouch('pod-1');
      expect(hardwareContainer.read(drillProvider).phase, DrillPhase.finished);
    });
  });

  group('DrillNotifier.reset', () {
    test('resets all state to idle', () {
      notifier.reset();
      final state = container.read(drillProvider);
      expect(state.phase, DrillPhase.idle);
      expect(state.config, isNull);
      expect(state.results, isEmpty);
      expect(state.currentRound, 0);
      expect(state.activePodAddress, isNull);
      expect(state.roundStartTime, isNull);
      expect(state.errorMessage, isNull);
    });
  });

  group('DrillNotifier.recordTouch', () {
    test('ignores touch when not in waitingTouch phase', () {
      notifier.recordTouch('sim-pod-1');
      // Should be no-op - state remains idle
      expect(container.read(drillProvider).phase, DrillPhase.idle);
    });

    test('rejects wrong-pod touch without scoring the round', () async {
      const config = DrillConfig(
        roundCount: 1,
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: ['sim-pod-1'],
      );
      await notifier.startDrill(config);
      await Future<void>.delayed(Duration.zero);
      expect(container.read(drillProvider).phase, DrillPhase.waitingTouch);

      notifier.recordTouch('sim-pod-2');

      expect(container.read(drillProvider).phase, DrillPhase.waitingTouch);
      expect(container.read(drillProvider).results, isEmpty);
    });
  });

  group('integrated operator lifecycle races', () {
    test(
      'real lifecycle operations terminate once in every active phase',
      () async {
        const addresses = ['pod-1', 'pod-2'];
        const actions = [
          'disconnectPod',
          'disconnectAll',
          'replacement connect',
        ];
        const phases = [
          DrillPhase.preparing,
          DrillPhase.waitingDelay,
          DrillPhase.waitingTouch,
          DrillPhase.roundComplete,
        ];

        for (final action in actions) {
          for (final phase in phases) {
            final repositories = <String, _LifecycleRepository>{};
            final allRepositories = <_LifecycleRepository>[];
            final multiPod = MultiPodNotifier(
              connector: (pod) async {
                final repository = _LifecycleRepository();
                repositories[pod.address] = repository;
                allRepositories.add(repository);
                return (
                  transport: _LifecycleTransport(),
                  repository: repository,
                );
              },
            );
            for (final address in addresses) {
              await multiPod.connectPod(
                PodDevice(name: address, address: address),
              );
            }
            final raceContainer = ProviderContainer(
              overrides: [
                drillProvider.overrideWith(
                  (ref) => DrillNotifier(ref, multiPod: multiPod),
                ),
              ],
            );
            final raceNotifier = raceContainer.read(drillProvider.notifier);
            var terminalEvents = 0;
            final terminalSubscription = raceContainer.listen<DrillState>(
              drillProvider,
              (_, next) {
                if (next.phase == DrillPhase.error) terminalEvents++;
              },
            );

            Future<void> waitForPhase(DrillPhase expected) async {
              for (var attempt = 0; attempt < 20; attempt++) {
                if (raceContainer.read(drillProvider).phase == expected) return;
                await Future<void>.delayed(const Duration(milliseconds: 2));
              }
              fail(
                'action=$action expected=$expected '
                'actual=${raceContainer.read(drillProvider).phase}',
              );
            }

            const delayedConfig = DrillConfig(
              roundCount: 3,
              timeout: Duration(milliseconds: 40),
              minDelay: Duration(milliseconds: 40),
              maxDelay: Duration(milliseconds: 40),
              podAddresses: addresses,
            );
            const immediateConfig = DrillConfig(
              roundCount: 3,
              timeout: Duration(milliseconds: 40),
              minDelay: Duration.zero,
              maxDelay: Duration.zero,
              podAddresses: addresses,
            );

            if (phase == DrillPhase.preparing) {
              repositories['pod-1']!.modeGate = Completer<void>();
              unawaited(raceNotifier.startDrill(delayedConfig));
              await waitForPhase(phase);
            } else if (phase == DrillPhase.waitingDelay) {
              await raceNotifier.startDrill(delayedConfig);
              await waitForPhase(phase);
            } else {
              await raceNotifier.startDrill(immediateConfig);
              await waitForPhase(DrillPhase.waitingTouch);
              if (phase == DrillPhase.roundComplete) {
                final target = raceContainer
                    .read(drillProvider)
                    .activePodAddress!;
                repositories[target]!.nextLedGate = Completer<void>();
                raceNotifier.recordTouch(target);
                await waitForPhase(phase);
              }
            }

            final beforeFailure = raceContainer.read(drillProvider);
            final failedAddress =
                phase == DrillPhase.preparing || action == 'disconnectAll'
                ? 'pod-1'
                : beforeFailure.activePodAddress ?? 'pod-1';
            final retainedResults = List.of(beforeFailure.results);
            final peerAddress = addresses.firstWhere(
              (address) => address != failedAddress,
            );
            final peerTransport = multiPod.state[peerAddress]!.transport;

            if (action == 'disconnectPod') {
              await multiPod.disconnectPod(failedAddress);
            } else if (action == 'disconnectAll') {
              await multiPod.disconnectAll();
            } else {
              await multiPod.connectPod(
                PodDevice(name: failedAddress, address: failedAddress),
              );
            }
            await Future<void>.delayed(Duration.zero);
            await Future<void>.delayed(Duration.zero);

            final failed = raceContainer.read(drillProvider);
            expect(failed.phase, DrillPhase.error, reason: '$action $phase');
            expect(failed.errorMessage, contains(failedAddress));
            expect(failed.results, retainedResults);
            expect(terminalEvents, 1, reason: '$action $phase');
            if (action != 'disconnectAll') {
              expect(
                multiPod.state[peerAddress]!.transport,
                same(peerTransport),
              );
              expect(multiPod.state[peerAddress]!.isConnected, isTrue);
            }

            repositories['pod-1']?.modeGate?.complete();
            for (final repository in allRepositories) {
              repository.nextLedGate?.complete();
            }
            await Future<void>.delayed(const Duration(milliseconds: 70));
            expect(raceContainer.read(drillProvider), same(failed));
            expect(terminalEvents, 1, reason: 'stale work: $action $phase');

            terminalSubscription.close();
            raceContainer.dispose();
            multiPod.dispose();
            for (final repository in allRepositories) {
              await repository.close();
            }
          }
        }
      },
    );

    test(
      'software-only six-identity real-provider lifecycle regression',
      () async {
        const identities = [
          'sim-pod-1',
          'sim-pod-2',
          'sim-pod-3',
          'sim-pod-4',
          'sim-pod-5',
          'sim-pod-6',
        ];
        const lifecycleCycles = 60;
        final allRepositories = <_LifecycleRepository>[];
        final multiPod = MultiPodNotifier(
          connector: (pod) async {
            final repository = _LifecycleRepository();
            allRepositories.add(repository);
            return (transport: _LifecycleTransport(), repository: repository);
          },
        );
        for (final identity in identities) {
          await multiPod.connectPod(
            PodDevice(name: identity, address: identity),
          );
        }
        final stressContainer = ProviderContainer(
          overrides: [
            drillProvider.overrideWith(
              (ref) => DrillNotifier(ref, multiPod: multiPod),
            ),
          ],
        );
        final stressNotifier = stressContainer.read(drillProvider.notifier);
        var terminalEvents = 0;
        var duplicateTerminalEvents = 0;
        var reconnects = 0;
        var retainedResults = 0;
        var staleMutations = 0;
        var duplicateResults = 0;
        var lostResults = 0;
        var peerMutations = 0;
        final terminalSubscription = stressContainer.listen<DrillState>(
          drillProvider,
          (_, next) {
            if (next.phase == DrillPhase.error) terminalEvents++;
          },
        );

        Future<void> waitForPhase(DrillPhase expected, int cycle) async {
          for (var attempt = 0; attempt < 20; attempt++) {
            if (stressContainer.read(drillProvider).phase == expected) return;
            await Future<void>.delayed(Duration.zero);
          }
          fail(
            'cycle=$cycle expected=$expected '
            'actual=${stressContainer.read(drillProvider).phase}',
          );
        }

        const config = DrillConfig(
          roundCount: 2,
          timeout: Duration(seconds: 1),
          minDelay: Duration.zero,
          maxDelay: Duration.zero,
          podAddresses: identities,
        );
        for (var cycle = 0; cycle < lifecycleCycles; cycle++) {
          await stressNotifier.startDrill(config);
          await waitForPhase(DrillPhase.waitingTouch, cycle);
          stressNotifier.simulateTouch();
          await waitForPhase(DrillPhase.waitingTouch, cycle);

          final failedAddress = identities[cycle % identities.length];
          final peersBefore = {
            for (final peer in identities.where(
              (peer) => peer != failedAddress,
            ))
              peer: multiPod.state[peer]!.transport,
          };
          final terminalsBefore = terminalEvents;
          if ((cycle ~/ identities.length).isEven) {
            await multiPod.disconnectPod(failedAddress);
            await multiPod.connectPod(
              PodDevice(name: failedAddress, address: failedAddress),
            );
          } else {
            await multiPod.connectPod(
              PodDevice(name: failedAddress, address: failedAddress),
            );
          }
          reconnects++;
          await Future<void>.delayed(Duration.zero);
          await Future<void>.delayed(Duration.zero);

          final failed = stressContainer.read(drillProvider);
          expect(failed.phase, DrillPhase.error, reason: 'cycle=$cycle');
          expect(failed.errorMessage, contains(failedAddress));
          retainedResults += failed.results.length;
          final indexes = failed.results.map((result) => result.roundIndex);
          duplicateResults += failed.results.length - indexes.toSet().length;
          lostResults += 1 - indexes.toSet().length;
          if (terminalEvents - terminalsBefore != 1) {
            duplicateTerminalEvents++;
          }
          for (final peer in peersBefore.entries) {
            if (!identical(multiPod.state[peer.key]!.transport, peer.value) ||
                !multiPod.state[peer.key]!.isConnected) {
              peerMutations++;
            }
          }

          final terminal = stressContainer.read(drillProvider);
          stressNotifier.recordTouch(
            terminal.activePodAddress ?? failedAddress,
          );
          await Future<void>.delayed(Duration.zero);
          if (!identical(stressContainer.read(drillProvider), terminal)) {
            staleMutations++;
          }
        }

        await multiPod.disconnectAll();
        await Future<void>.delayed(Duration.zero);
        expect(terminalEvents, lifecycleCycles);
        expect(duplicateTerminalEvents, 0);
        expect(reconnects, lifecycleCycles);
        expect(retainedResults, lifecycleCycles);
        expect(staleMutations, 0);
        expect(duplicateResults, 0);
        expect(lostResults, 0);
        expect(peerMutations, 0);
        expect(multiPod.state, isEmpty);
        // Software-only fake-provider evidence; no physical behavior is claimed.
        // ignore: avoid_print
        print(
          'FS4_REAL_PROVIDER_LIFECYCLE_SOFTWARE_ONLY '
          'identities=${identities.length} fault_types=2 '
          'lifecycle_cycles=$lifecycleCycles terminal_events=$terminalEvents '
          'reconnects=$reconnects retained_results=$retainedResults '
          'duplicate_terminal_events=$duplicateTerminalEvents '
          'stale_mutations=$staleMutations duplicate_results=$duplicateResults '
          'lost_results=$lostResults peer_mutations=$peerMutations '
          'terminal=disconnected',
        );

        terminalSubscription.close();
        stressContainer.dispose();
        multiPod.dispose();
        for (final repository in allRepositories) {
          await repository.close();
        }
      },
    );
  });

  group('DrillNotifier.simulateTouch', () {
    test('does nothing when no active pod', () {
      notifier.simulateTouch();
      // Should be no-op
      expect(container.read(drillProvider).phase, DrillPhase.idle);
    });

    test('six-pod deterministic lifecycle stress campaign', () async {
      const identities = [
        'sim-pod-1',
        'sim-pod-2',
        'sim-pod-3',
        'sim-pod-4',
        'sim-pod-5',
        'sim-pod-6',
      ];
      const lifecycleCycles = 100;
      const roundsPerCycle = 10;
      const expectedRounds = lifecycleCycles * roundsPerCycle;
      final stressMultiPod = _FakeMultiPodNotifier();
      final stressContainer = ProviderContainer(
        overrides: [
          drillProvider.overrideWith(
            (ref) => DrillNotifier(ref, multiPod: stressMultiPod),
          ),
        ],
      );
      final stressNotifier = stressContainer.read(drillProvider.notifier);
      var completedRounds = 0;
      var staleMutations = 0;
      var duplicateResults = 0;
      var lostResults = 0;
      var participantFailuresRecovered = 0;
      var isolatedNonParticipantFailures = 0;
      var terminalEvents = 0;
      var duplicateTerminalEvents = 0;
      var retainedResults = 0;
      var reconnects = 0;
      final terminalSubscription = stressContainer.listen<DrillState>(
        drillProvider,
        (_, next) {
          if (next.phase == DrillPhase.error) terminalEvents++;
        },
      );

      Future<void> waitForPhase(DrillPhase phase, int cycle) async {
        for (var attempt = 0; attempt < 10; attempt++) {
          if (stressContainer.read(drillProvider).phase == phase) return;
          await Future<void>.delayed(Duration.zero);
        }
        fail(
          'case=cycle-$cycle expected=$phase '
          'actual=${stressContainer.read(drillProvider).phase}',
        );
      }

      const roundConfig = DrillConfig(
        roundCount: roundsPerCycle,
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: identities,
      );
      const lifecycleConfig = DrillConfig(
        roundCount: 2,
        minDelay: Duration.zero,
        maxDelay: Duration.zero,
        podAddresses: identities,
      );

      for (var cycle = 0; cycle < lifecycleCycles; cycle++) {
        await stressNotifier.startDrill(roundConfig);
        for (var round = 0; round < roundsPerCycle; round++) {
          await waitForPhase(DrillPhase.waitingTouch, cycle);
          stressNotifier.simulateTouch();
        }

        final completed = stressContainer.read(drillProvider);
        expect(
          completed.phase,
          DrillPhase.finished,
          reason: 'case=cycle-$cycle completed campaign terminal',
        );
        final indexes = completed.results.map((result) => result.roundIndex);
        final uniqueIndexes = indexes.toSet();
        duplicateResults += completed.results.length - uniqueIndexes.length;
        lostResults += roundsPerCycle - uniqueIndexes.length;
        expect(
          uniqueIndexes,
          equals(Set<int>.from(List<int>.generate(roundsPerCycle, (i) => i))),
          reason: 'case=cycle-$cycle result indexes',
        );
        completedRounds += completed.results.length;

        final lifecycleGeneration = cycle + 1;
        for (final identity in identities) {
          stressMultiPod.connectionGenerations[identity] = lifecycleGeneration;
        }
        await stressNotifier.startDrill(lifecycleConfig);
        await waitForPhase(DrillPhase.waitingTouch, cycle);
        stressNotifier.simulateTouch();
        await waitForPhase(DrillPhase.waitingTouch, cycle);
        final beforeIsolatedFailure = stressContainer.read(drillProvider);
        stressMultiPod.fail('sim-pod-outside');
        await Future<void>.delayed(Duration.zero);
        if (identical(
          stressContainer.read(drillProvider),
          beforeIsolatedFailure,
        )) {
          isolatedNonParticipantFailures++;
        }

        final failedAddress = identities[cycle % identities.length];
        final terminalsBefore = terminalEvents;
        stressMultiPod.lifecycle(
          failedAddress,
          cycle.isEven ? 'disconnectPod' : 'replacement connect',
          lifecycleGeneration,
        );
        await Future<void>.delayed(Duration.zero);
        final failed = stressContainer.read(drillProvider);
        expect(
          failed.phase,
          DrillPhase.error,
          reason: 'case=cycle-$cycle participating failure',
        );
        expect(failed.errorMessage, contains(failedAddress));
        expect(failed.results, hasLength(1));
        retainedResults += failed.results.length;
        if (terminalEvents - terminalsBefore != 1) duplicateTerminalEvents++;

        await stressNotifier.startDrill(lifecycleConfig);
        reconnects++;
        await waitForPhase(DrillPhase.waitingTouch, cycle);
        participantFailuresRecovered++;
        final cancelledAddress = stressContainer
            .read(drillProvider)
            .activePodAddress!;
        stressNotifier.stopDrill();
        final stopped = stressContainer.read(drillProvider);
        expect(
          stopped.phase,
          DrillPhase.idle,
          reason: 'case=cycle-$cycle stop terminal',
        );
        stressNotifier.recordTouch(cancelledAddress);
        await Future<void>.delayed(Duration.zero);
        if (!identical(stressContainer.read(drillProvider), stopped)) {
          staleMutations++;
        }
      }

      expect(completedRounds, expectedRounds);
      expect(staleMutations, 0);
      expect(duplicateResults, 0);
      expect(lostResults, 0);
      expect(participantFailuresRecovered, lifecycleCycles);
      expect(isolatedNonParticipantFailures, lifecycleCycles);
      expect(terminalEvents, lifecycleCycles);
      expect(duplicateTerminalEvents, 0);
      expect(retainedResults, lifecycleCycles);
      expect(reconnects, lifecycleCycles);
      expect(stressContainer.read(drillProvider).phase, DrillPhase.idle);
      // Retained CI evidence: values are deterministic and intentionally omit time.
      // ignore: avoid_print
      print(
        'FS4_DRILL_LIFECYCLE_SOFTWARE_ONLY identities=${identities.length} '
        'fault_types=2 rounds=$completedRounds '
        'lifecycle_cycles=$lifecycleCycles terminal_events=$terminalEvents '
        'reconnects=$reconnects retained_results=$retainedResults '
        'duplicate_terminal_events=$duplicateTerminalEvents '
        'participant_failures_recovered=$participantFailuresRecovered '
        'isolated_nonparticipant_failures=$isolatedNonParticipantFailures '
        'stale_mutations=$staleMutations duplicate_results=$duplicateResults '
        'lost_results=$lostResults terminal=${stressContainer.read(drillProvider).phase.name}',
      );

      terminalSubscription.close();
      stressContainer.dispose();
      await stressMultiPod.events.close();
      await stressMultiPod.failures.close();
      await stressMultiPod.lifecycleEvents.close();
      stressMultiPod.dispose();
    });
  });

  group('DrillNotifier.drillResult', () {
    test('returns null when no config', () {
      expect(notifier.drillResult, isNull);
    });

    test('returns null when no results', () {
      expect(notifier.drillResult, isNull);
    });
  });

  group('DrillState', () {
    test('isRunning is true for active phases', () {
      for (final phase in [
        DrillPhase.preparing,
        DrillPhase.waitingDelay,
        DrillPhase.armed,
        DrillPhase.waitingTouch,
        DrillPhase.roundComplete,
      ]) {
        final state = DrillState(phase: phase);
        expect(state.isRunning, isTrue, reason: '$phase should be running');
      }
    });

    test('isRunning is false for terminal phases', () {
      for (final phase in [
        DrillPhase.idle,
        DrillPhase.finished,
        DrillPhase.error,
      ]) {
        final state = DrillState(phase: phase);
        expect(
          state.isRunning,
          isFalse,
          reason: '$phase should not be running',
        );
      }
    });

    test('copyWith preserves unset fields', () {
      const config = DrillConfig(roundCount: 10, podAddresses: ['a']);
      const state = DrillState(
        phase: DrillPhase.waitingTouch,
        config: config,
        currentRound: 3,
        activePodAddress: 'a',
      );

      final updated = state.copyWith(phase: DrillPhase.roundComplete);
      expect(updated.phase, DrillPhase.roundComplete);
      expect(updated.config, config);
      expect(updated.currentRound, 3);
      expect(updated.activePodAddress, 'a');
    });
  });
}
