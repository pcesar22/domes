/// Drill orchestration provider.
///
/// Phone-side drill orchestrator that:
/// 1. Sets participating pods to GAME mode
/// 2. Random delay between rounds
/// 3. Selects target pod, sets LED color
/// 4. Waits for touch event or timeout
/// 5. Records result, advances to next round
/// 6. Computes stats on completion
library;

import 'dart:async';
import 'dart:math';

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/config_protocol.dart';
import '../../domain/models/drill_config.dart';
import '../../domain/models/drill_result.dart';
import 'multi_pod_provider.dart';

/// Drill execution state.
enum DrillPhase {
  idle,
  preparing,
  waitingDelay,
  armed,
  waitingTouch,
  roundComplete,
  finished,
  error,
}

/// Live drill state.
class DrillState {
  final DrillPhase phase;
  final DrillConfig? config;
  final int currentRound;
  final String? activePodAddress;
  final List<RoundResult> results;
  final DateTime? roundStartTime;
  final String? errorMessage;

  const DrillState({
    this.phase = DrillPhase.idle,
    this.config,
    this.currentRound = 0,
    this.activePodAddress,
    this.results = const [],
    this.roundStartTime,
    this.errorMessage,
  });

  DrillState copyWith({
    DrillPhase? phase,
    DrillConfig? config,
    int? currentRound,
    String? activePodAddress,
    List<RoundResult>? results,
    DateTime? roundStartTime,
    String? errorMessage,
  }) =>
      DrillState(
        phase: phase ?? this.phase,
        config: config ?? this.config,
        currentRound: currentRound ?? this.currentRound,
        activePodAddress: activePodAddress ?? this.activePodAddress,
        results: results ?? this.results,
        roundStartTime: roundStartTime ?? this.roundStartTime,
        errorMessage: errorMessage ?? this.errorMessage,
      );

  bool get isRunning =>
      phase != DrillPhase.idle &&
      phase != DrillPhase.finished &&
      phase != DrillPhase.error;

  /// Last reaction time for display.
  Duration? get lastReactionTime {
    if (results.isEmpty) return null;
    return results.last.reactionTime;
  }
}

/// Drill orchestrator notifier.
class DrillNotifier extends StateNotifier<DrillState> {
  final Ref _ref;
  final Random _random = Random();
  Timer? _delayTimer;
  Timer? _timeoutTimer;
  DateTime? _drillStartTime;

  DrillNotifier(this._ref) : super(const DrillState());

  /// Start a drill with the given config.
  Future<void> startDrill(DrillConfig config) async {
    if (state.isRunning) return;

    _drillStartTime = DateTime.now();

    state = DrillState(
      phase: DrillPhase.preparing,
      config: config,
      currentRound: 0,
      results: [],
    );

    // Set all participating pods to GAME mode
    final multiPod = _ref.read(multiPodProvider.notifier);
    try {
      for (final addr in config.podAddresses) {
        await multiPod.setMode(addr, SystemMode.SYSTEM_MODE_GAME);
      }
    } catch (e) {
      state = state.copyWith(
        phase: DrillPhase.error,
        errorMessage: 'Failed to set pods to GAME mode: $e',
      );
      return;
    }

    // Turn off all LEDs first
    for (final addr in config.podAddresses) {
      await multiPod.setLedPattern(addr, AppLedPattern.off());
    }

    // Start first round
    _startNextRound();
  }

  /// Stop the drill.
  void stopDrill() {
    _delayTimer?.cancel();
    _timeoutTimer?.cancel();

    if (state.results.isNotEmpty) {
      state = state.copyWith(phase: DrillPhase.finished);
    } else {
      state = const DrillState();
    }

    // Turn off all LEDs and return to IDLE
    _cleanupPods();
  }

  /// Record a touch event from a pod.
  void recordTouch(String podAddress) {
    if (state.phase != DrillPhase.waitingTouch) return;
    if (podAddress != state.activePodAddress) return;

    _timeoutTimer?.cancel();

    final reactionTime = state.roundStartTime != null
        ? DateTime.now().difference(state.roundStartTime!)
        : null;

    final result = RoundResult(
      roundIndex: state.currentRound,
      podAddress: podAddress,
      hit: true,
      reactionTime: reactionTime,
      timestamp: DateTime.now(),
    );

    final newResults = [...state.results, result];

    // Turn off the LED
    final multiPod = _ref.read(multiPodProvider.notifier);
    multiPod.setLedPattern(podAddress, AppLedPattern.off());

    state = state.copyWith(
      phase: DrillPhase.roundComplete,
      results: newResults,
    );

    // Check if drill is complete
    if (newResults.length >= (state.config?.roundCount ?? 0)) {
      _finishDrill();
    } else {
      _startNextRound();
    }
  }

  /// Simulate a touch for testing when real hardware isn't available.
  void simulateTouch() {
    if (state.activePodAddress != null) {
      recordTouch(state.activePodAddress!);
    }
  }

  void _startNextRound() {
    final config = state.config;
    if (config == null) return;

    final nextRound = state.results.length;

    state = state.copyWith(
      phase: DrillPhase.waitingDelay,
      currentRound: nextRound,
    );

    // Random delay before arming
    final delayMs = config.minDelay.inMilliseconds +
        _random.nextInt(
            config.maxDelay.inMilliseconds - config.minDelay.inMilliseconds);

    _delayTimer = Timer(Duration(milliseconds: delayMs), () {
      _armRound();
    });
  }

  void _armRound() {
    final config = state.config;
    if (config == null || config.podAddresses.isEmpty) return;

    // Pick a random pod
    final targetPod =
        config.podAddresses[_random.nextInt(config.podAddresses.length)];

    // Light up the target pod
    final multiPod = _ref.read(multiPodProvider.notifier);
    multiPod.setLedPattern(
      targetPod,
      AppLedPattern.solid(0, 255, 0), // Green = go!
    );

    final now = DateTime.now();
    state = state.copyWith(
      phase: DrillPhase.waitingTouch,
      activePodAddress: targetPod,
      roundStartTime: now,
    );

    // Start timeout timer
    _timeoutTimer = Timer(config.timeout, () {
      _handleTimeout();
    });
  }

  void _handleTimeout() {
    final activePod = state.activePodAddress;
    if (activePod == null) return;

    // Record miss
    final result = RoundResult(
      roundIndex: state.currentRound,
      podAddress: activePod,
      hit: false,
      timestamp: DateTime.now(),
    );

    // Turn LED red briefly then off
    final multiPod = _ref.read(multiPodProvider.notifier);
    multiPod.setLedPattern(activePod, AppLedPattern.solid(255, 0, 0));
    Future.delayed(const Duration(milliseconds: 500), () {
      multiPod.setLedPattern(activePod, AppLedPattern.off());
    });

    final newResults = [...state.results, result];

    state = state.copyWith(
      phase: DrillPhase.roundComplete,
      results: newResults,
    );

    if (newResults.length >= (state.config?.roundCount ?? 0)) {
      _finishDrill();
    } else {
      _startNextRound();
    }
  }

  void _finishDrill() {
    _delayTimer?.cancel();
    _timeoutTimer?.cancel();

    state = state.copyWith(phase: DrillPhase.finished);
    _cleanupPods();
  }

  Future<void> _cleanupPods() async {
    final multiPod = _ref.read(multiPodProvider.notifier);
    final config = state.config;
    if (config == null) return;

    for (final addr in config.podAddresses) {
      try {
        await multiPod.setLedPattern(addr, AppLedPattern.off());
        await multiPod.setMode(addr, SystemMode.SYSTEM_MODE_IDLE);
      } catch (_) {
        // Best effort cleanup
      }
    }
  }

  /// Build a DrillResult from the current state.
  DrillResult? get drillResult {
    if (state.config == null || state.results.isEmpty) return null;
    return DrillResult(
      config: state.config!,
      rounds: state.results,
      startTime: _drillStartTime ?? DateTime.now(),
      endTime: DateTime.now(),
    );
  }

  /// Reset drill state to idle.
  void reset() {
    _delayTimer?.cancel();
    _timeoutTimer?.cancel();
    state = const DrillState();
  }

  @override
  void dispose() {
    _delayTimer?.cancel();
    _timeoutTimer?.cancel();
    super.dispose();
  }
}

final drillProvider =
    StateNotifierProvider<DrillNotifier, DrillState>((ref) {
  return DrillNotifier(ref);
});
