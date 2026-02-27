/// Drill result models.
library;

import 'dart:convert';

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

  /// Export as plain text summary.
  String toTextSummary() {
    final buf = StringBuffer()
      ..writeln('DOMES Drill Results')
      ..writeln('=' * 30)
      ..writeln('Type: ${config.type.label}')
      ..writeln('Date: ${startTime.toIso8601String().substring(0, 19)}')
      ..writeln('Duration: ${totalDuration.inSeconds}s')
      ..writeln()
      ..writeln('Hit Rate: ${(hitRate * 100).toStringAsFixed(0)}% ($hits/$totalRounds)')
      ..writeln('Avg Reaction: ${avgReactionTime?.inMilliseconds ?? "N/A"}ms')
      ..writeln('Best: ${bestReactionTime?.inMilliseconds ?? "N/A"}ms')
      ..writeln('Worst: ${worstReactionTime?.inMilliseconds ?? "N/A"}ms')
      ..writeln()
      ..writeln('Rounds:');

    for (final r in rounds) {
      final ms = r.reactionTime?.inMilliseconds;
      buf.writeln(
          '  ${r.roundIndex + 1}. ${r.hit ? "HIT" : "MISS"} ${ms != null ? "${ms}ms" : ""}');
    }

    return buf.toString();
  }

  /// Export as JSON map.
  Map<String, dynamic> toJson() {
    return {
      'drillType': config.type.name,
      'roundCount': config.roundCount,
      'timeoutMs': config.timeout.inMilliseconds,
      'startTime': startTime.toIso8601String(),
      'endTime': endTime.toIso8601String(),
      'durationMs': totalDuration.inMilliseconds,
      'hits': hits,
      'misses': misses,
      'hitRate': hitRate,
      'avgReactionMs': avgReactionTime?.inMilliseconds,
      'bestReactionMs': bestReactionTime?.inMilliseconds,
      'worstReactionMs': worstReactionTime?.inMilliseconds,
      'rounds': rounds
          .map((r) => {
                'round': r.roundIndex + 1,
                'pod': r.podAddress,
                'hit': r.hit,
                'reactionMs': r.reactionTime?.inMilliseconds,
              })
          .toList(),
    };
  }

  /// Export as JSON string.
  String toJsonString() => const JsonEncoder.withIndent('  ').convert(toJson());
}
