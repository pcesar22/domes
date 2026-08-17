import 'dart:async';

import 'package:domes_app/application/providers/drill_provider.dart';
import 'package:domes_app/application/providers/multi_pod_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/domain/models/drill_config.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

final class _FakeMultiPodNotifier extends MultiPodNotifier {
  final StreamController<PodTouchEvent> events =
      StreamController<PodTouchEvent>.broadcast();
  final StreamController<PodConnectionFailure> failures =
      StreamController<PodConnectionFailure>.broadcast();
  final List<SystemMode> modeCalls = [];
  final List<AppLedPattern> ledCalls = [];
  final List<(String, SystemMode)> addressedModeCalls = [];
  final List<(String, AppLedPattern)> addressedLedCalls = [];
  Completer<void>? gameModeGate;
  Completer<void>? idleModeGate;
  Completer<void>? nextLedOffGate;

  @override
  Stream<PodTouchEvent> get touchEvents => events.stream;

  @override
  Stream<PodConnectionFailure> get connectionFailures => failures.stream;

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

  void fail(String address, [Object? error]) {
    failures.add(
      PodConnectionFailure(
        address: address,
        error: error ?? StateError('connection lost'),
        stackTrace: StackTrace.current,
      ),
    );
  }

  int get greenCalls =>
      ledCalls.where((pattern) => pattern.color == (0, 255, 0, 0)).length;
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

    test('participating failure is terminal in every active phase', () async {
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
    });

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
  });

  group('DrillNotifier.simulateTouch', () {
    test('does nothing when no active pod', () {
      notifier.simulateTouch();
      // Should be no-op
      expect(container.read(drillProvider).phase, DrillPhase.idle);
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
