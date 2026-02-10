/// Drill result models.
library;

import 'drill_config.dart';

class RoundResult {
  final int roundIndex;
  final String podAddress;
  final bool hit;
  final Duration? reactionTime;
  final DateTime timestamp;

  const RoundResult({
    required this.roundIndex,
    required this.podAddress,
    required this.hit,
    this.reactionTime,
    required this.timestamp,
  });
}

class DrillResult {
  final DrillConfig config;
  final List<RoundResult> rounds;
  final DateTime startTime;
  final DateTime endTime;

  const DrillResult({
    required this.config,
    required this.rounds,
    required this.startTime,
    required this.endTime,
  });

  int get totalRounds => rounds.length;
  int get hits => rounds.where((r) => r.hit).length;
  int get misses => rounds.where((r) => !r.hit).length;
  double get hitRate => totalRounds > 0 ? hits / totalRounds : 0;

  Duration get totalDuration => endTime.difference(startTime);

  List<RoundResult> get hitRounds =>
      rounds.where((r) => r.hit && r.reactionTime != null).toList();

  Duration? get avgReactionTime {
    final hitList = hitRounds;
    if (hitList.isEmpty) return null;
    final totalMs = hitList.fold<int>(
        0, (sum, r) => sum + r.reactionTime!.inMilliseconds);
    return Duration(milliseconds: totalMs ~/ hitList.length);
  }

  Duration? get bestReactionTime {
    final hitList = hitRounds;
    if (hitList.isEmpty) return null;
    return hitList
        .map((r) => r.reactionTime!)
        .reduce((a, b) => a < b ? a : b);
  }

  Duration? get worstReactionTime {
    final hitList = hitRounds;
    if (hitList.isEmpty) return null;
    return hitList
        .map((r) => r.reactionTime!)
        .reduce((a, b) => a > b ? a : b);
  }

  /// Per-pod breakdown.
  Map<String, List<RoundResult>> get perPodResults {
    final map = <String, List<RoundResult>>{};
    for (final r in rounds) {
      map.putIfAbsent(r.podAddress, () => []).add(r);
    }
    return map;
  }
}
