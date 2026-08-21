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
  }) => DrillState(
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
  final Random _random = Random();
  late final MultiPodNotifier _multiPod;
  Timer? _delayTimer;
  Timer? _timeoutTimer;
  Timer? _ledOffTimer;
  DateTime? _drillStartTime;
  late final StreamSubscription<PodTouchEvent> _touchSubscription;
  late final StreamSubscription<PodConnectionFailure>
  _connectionFailureSubscription;
  int _generation = 0;
  Future<void> _cleanupTail = Future<void>.value();

  DrillNotifier(Ref ref, {MultiPodNotifier? multiPod})
    : super(const DrillState()) {
    _multiPod = multiPod ?? ref.read(multiPodProvider.notifier);
    _touchSubscription = _multiPod.touchEvents.listen(
      (event) => recordTouch(event.address),
    );
    _connectionFailureSubscription = _multiPod.connectionFailures.listen(
      _handleConnectionFailure,
    );
  }

  bool get supportsTouchSimulation =>
      state.activePodAddress?.startsWith('sim-pod-') ?? false;

  /// Start a drill with the given config.
  Future<void> startDrill(DrillConfig config) async {
    if (state.isRunning) return;

    final validationError = _validateConfig(config);
    if (validationError != null) {
      _cancelSession();
      state = DrillState(
        phase: DrillPhase.error,
        config: config,
        errorMessage: validationError,
      );
      return;
    }

    final generation = ++_generation;

    _drillStartTime = DateTime.now();

    state = DrillState(
      phase: DrillPhase.preparing,
      config: config,
      currentRound: 0,
      results: [],
    );

    await _cleanupTail;
    if (!_isCurrent(generation, DrillPhase.preparing)) return;

    // Set all physical pods to GAME mode and start from a dark LED state.
    for (final addr in config.podAddresses) {
      if (!_isSimulatedAddress(addr)) {
        try {
          await _multiPod.setMode(addr, SystemMode.SYSTEM_MODE_GAME);
          if (!_isCurrent(generation, DrillPhase.preparing)) return;
        } catch (e) {
          _failSession(generation, 'Failed to prepare pod $addr: $e');
          return;
        }
      }
    }
    for (final addr in config.podAddresses) {
      if (!_isSimulatedAddress(addr)) {
        try {
          await _multiPod.setLedPattern(addr, AppLedPattern.off());
          if (!_isCurrent(generation, DrillPhase.preparing)) return;
        } catch (e) {
          _failSession(generation, 'Failed to prepare pod $addr: $e');
          return;
        }
      }
    }

    if (_isCurrent(generation, DrillPhase.preparing)) {
      _startNextRound(generation);
    }
  }

  /// Stop the drill.
  void stopDrill() {
    final config = state.config;
    _cancelSession();

    if (state.results.isNotEmpty) {
      state = state.copyWith(phase: DrillPhase.finished);
    } else {
      state = const DrillState();
    }

    // Turn off all LEDs and return to IDLE
    unawaited(_scheduleCleanup(config));
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
    final generation = _generation;

    state = state.copyWith(
      phase: DrillPhase.roundComplete,
      results: newResults,
    );

    // Check if drill is complete
    if (newResults.length >= (state.config?.roundCount ?? 0)) {
      _finishDrill(generation);
    } else {
      unawaited(_completeHitRound(generation, podAddress));
    }
  }

  /// Simulate a touch for testing when real hardware isn't available.
  void simulateTouch() {
    if (supportsTouchSimulation && state.activePodAddress != null) {
      recordTouch(state.activePodAddress!);
    }
  }

  Future<void> _completeHitRound(int generation, String podAddress) async {
    if (!_isSimulatedAddress(podAddress)) {
      try {
        await _multiPod.setLedPattern(podAddress, AppLedPattern.off());
      } catch (e) {
        _failSession(generation, 'Failed to clear pod $podAddress: $e');
        return;
      }
    }
    if (_isCurrent(generation, DrillPhase.roundComplete)) {
      _startNextRound(generation);
    }
  }

  void _startNextRound(int generation) {
    if (!_isCurrent(generation)) return;
    final config = state.config;
    if (config == null) return;

    final nextRound = state.results.length;

    state = state.copyWith(
      phase: DrillPhase.waitingDelay,
      currentRound: nextRound,
    );

    // Random delay before arming
    final minDelayMs = config.minDelay.inMilliseconds;
    final delayRangeMs = config.maxDelay.inMilliseconds - minDelayMs;
    final delayMs = delayRangeMs == 0
        ? minDelayMs
        : minDelayMs + _random.nextInt(delayRangeMs + 1);

    _delayTimer = Timer(
      Duration(milliseconds: delayMs),
      () => unawaited(_armRound(generation)),
    );
  }

  Future<void> _armRound(int generation) async {
    final config = state.config;
    if (!_isCurrent(generation, DrillPhase.waitingDelay) ||
        config == null ||
        config.podAddresses.isEmpty ||
        state.phase != DrillPhase.waitingDelay) {
      return;
    }

    // Pick a random pod
    final targetPod =
        config.podAddresses[_random.nextInt(config.podAddresses.length)];

    if (!_isSimulatedAddress(targetPod)) {
      try {
        await _multiPod.setLedPattern(
          targetPod,
          AppLedPattern.solid(0, 255, 0), // Green = go!
        );
      } catch (e) {
        _failSession(generation, 'Failed to arm pod $targetPod: $e');
        return;
      }
    }

    if (!_isCurrent(generation, DrillPhase.waitingDelay)) return;

    final now = DateTime.now();
    state = state.copyWith(
      phase: DrillPhase.waitingTouch,
      activePodAddress: targetPod,
      roundStartTime: now,
    );

    // Start timeout timer
    _timeoutTimer = Timer(
      config.timeout,
      () => unawaited(_handleTimeout(generation)),
    );
  }

  Future<void> _handleTimeout(int generation) async {
    if (!_isCurrent(generation, DrillPhase.waitingTouch)) return;
    final activePod = state.activePodAddress;
    if (activePod == null) return;

    // Record miss
    final result = RoundResult(
      roundIndex: state.currentRound,
      podAddress: activePod,
      hit: false,
      timestamp: DateTime.now(),
    );

    final newResults = [...state.results, result];

    state = state.copyWith(
      phase: DrillPhase.roundComplete,
      results: newResults,
    );

    if (newResults.length >= (state.config?.roundCount ?? 0)) {
      _finishDrill(generation);
      return;
    }

    if (_isSimulatedAddress(activePod)) {
      _startNextRound(generation);
      return;
    }

    try {
      await _multiPod.setLedPattern(activePod, AppLedPattern.solid(255, 0, 0));
    } catch (e) {
      _failSession(
        generation,
        'Failed to show miss feedback on $activePod: $e',
      );
      return;
    }

    if (!_isCurrent(generation, DrillPhase.roundComplete)) return;
    _ledOffTimer?.cancel();
    _ledOffTimer = Timer(
      const Duration(milliseconds: 500),
      () => unawaited(_finishMissFeedback(generation, activePod)),
    );
  }

  Future<void> _finishMissFeedback(int generation, String podAddress) async {
    if (!_isCurrent(generation, DrillPhase.roundComplete)) return;
    try {
      await _multiPod.setLedPattern(podAddress, AppLedPattern.off());
    } catch (e) {
      _failSession(
        generation,
        'Failed to clear miss feedback on $podAddress: $e',
      );
      return;
    }
    if (_isCurrent(generation, DrillPhase.roundComplete)) {
      _startNextRound(generation);
    }
  }

  void _finishDrill(int generation) {
    if (!_isCurrent(generation)) return;
    final config = state.config;
    _cancelSession();
    state = state.copyWith(phase: DrillPhase.finished);
    unawaited(_scheduleCleanup(config));
  }

  Future<void> _cleanupPods(DrillConfig? config) async {
    if (config == null) return;

    for (final addr in config.podAddresses) {
      if (_isSimulatedAddress(addr)) continue;
      try {
        await _multiPod.setLedPattern(addr, AppLedPattern.off());
      } catch (_) {
        // Continue to mode cleanup even when the LED command fails.
      }
      try {
        await _multiPod.setMode(addr, SystemMode.SYSTEM_MODE_IDLE);
      } catch (_) {
        // Best effort cleanup.
      }
    }
  }

  Future<void> _scheduleCleanup(DrillConfig? config) {
    _cleanupTail = _cleanupTail
        .then((_) => _cleanupPods(config))
        .catchError((_) {});
    return _cleanupTail;
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
    final config = state.config;
    _cancelSession();
    state = const DrillState();
    unawaited(_scheduleCleanup(config));
  }

  void _handleConnectionFailure(PodConnectionFailure failure) {
    if (!mounted || !state.isRunning) return;
    if (!(state.config?.podAddresses.contains(failure.address) ?? false)) {
      return;
    }
    _failSession(
      _generation,
      'Pod ${failure.address} connection failed: ${failure.error}',
    );
  }

  void _failSession(int generation, String message) {
    if (!_isCurrent(generation)) return;
    final config = state.config;
    _cancelSession();
    state = state.copyWith(phase: DrillPhase.error, errorMessage: message);
    unawaited(_scheduleCleanup(config));
  }

  void _cancelSession() {
    _generation++;
    _delayTimer?.cancel();
    _timeoutTimer?.cancel();
    _ledOffTimer?.cancel();
    _delayTimer = null;
    _timeoutTimer = null;
    _ledOffTimer = null;
  }

  bool _isCurrent(int generation, [DrillPhase? phase]) =>
      mounted &&
      generation == _generation &&
      (phase == null || state.phase == phase);

  static String? _validateConfig(DrillConfig config) {
    if (config.podAddresses.isEmpty) return 'Select at least one pod';
    if (config.roundCount <= 0) return 'Round count must be positive';
    if (config.timeout <= Duration.zero) {
      return 'Round timeout must be positive';
    }
    if (config.minDelay < Duration.zero) {
      return 'Minimum delay cannot be negative';
    }
    if (config.maxDelay < config.minDelay) {
      return 'Maximum delay cannot be less than minimum delay';
    }
    return null;
  }

  static bool _isSimulatedAddress(String address) =>
      address.startsWith('sim-pod-');

  @override
  void dispose() {
    final config = state.config;
    _cancelSession();
    unawaited(_scheduleCleanup(config));
    unawaited(_touchSubscription.cancel());
    unawaited(_connectionFailureSubscription.cancel());
    super.dispose();
  }
}

final drillProvider = StateNotifierProvider<DrillNotifier, DrillState>((ref) {
  return DrillNotifier(ref);
});
