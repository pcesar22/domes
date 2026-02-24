import 'dart:convert';

import 'package:domes_app/domain/models/drill_config.dart';
import 'package:domes_app/domain/models/drill_result.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final now = DateTime(2025, 1, 15, 12, 0, 0);

  DrillResult makeResult(List<RoundResult> rounds, {int durationSec = 30}) {
    return DrillResult(
      config: const DrillConfig(
        type: DrillType.reaction,
        roundCount: 10,
        podAddresses: ['pod-1', 'pod-2'],
      ),
      rounds: rounds,
      startTime: now,
      endTime: now.add(Duration(seconds: durationSec)),
    );
  }

  group('DrillResult stats', () {
    test('counts hits and misses', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 300),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: false,
            timestamp: now),
        RoundResult(
            roundIndex: 2,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 500),
            timestamp: now),
      ]);

      expect(result.totalRounds, 3);
      expect(result.hits, 2);
      expect(result.misses, 1);
      expect(result.hitRate, closeTo(0.667, 0.001));
    });

    test('computes avg reaction time', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 200),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: true,
            reactionTime: const Duration(milliseconds: 400),
            timestamp: now),
        RoundResult(
            roundIndex: 2,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 600),
            timestamp: now),
      ]);

      expect(result.avgReactionTime, const Duration(milliseconds: 400));
    });

    test('computes best and worst reaction time', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 150),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: true,
            reactionTime: const Duration(milliseconds: 800),
            timestamp: now),
        RoundResult(
            roundIndex: 2,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 350),
            timestamp: now),
      ]);

      expect(result.bestReactionTime, const Duration(milliseconds: 150));
      expect(result.worstReactionTime, const Duration(milliseconds: 800));
    });

    test('returns null for empty results', () {
      final result = makeResult([]);

      expect(result.avgReactionTime, isNull);
      expect(result.bestReactionTime, isNull);
      expect(result.worstReactionTime, isNull);
      expect(result.hitRate, 0);
    });

    test('returns null for all misses', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: false,
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: false,
            timestamp: now),
      ]);

      expect(result.avgReactionTime, isNull);
      expect(result.bestReactionTime, isNull);
      expect(result.worstReactionTime, isNull);
      expect(result.hitRate, 0);
    });

    test('computes total duration', () {
      final result = makeResult([], durationSec: 45);
      expect(result.totalDuration, const Duration(seconds: 45));
    });

    test('per-pod breakdown', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 200),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: true,
            reactionTime: const Duration(milliseconds: 400),
            timestamp: now),
        RoundResult(
            roundIndex: 2,
            podAddress: 'pod-1',
            hit: false,
            timestamp: now),
        RoundResult(
            roundIndex: 3,
            podAddress: 'pod-2',
            hit: true,
            reactionTime: const Duration(milliseconds: 300),
            timestamp: now),
      ]);

      final perPod = result.perPodResults;
      expect(perPod.keys, containsAll(['pod-1', 'pod-2']));
      expect(perPod['pod-1']!.length, 2);
      expect(perPod['pod-2']!.length, 2);
      expect(perPod['pod-1']!.where((r) => r.hit).length, 1);
      expect(perPod['pod-2']!.where((r) => r.hit).length, 2);
    });

    test('hitRounds only includes hits with reaction time', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 200),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: false,
            timestamp: now),
        RoundResult(
            roundIndex: 2,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 500),
            timestamp: now),
      ]);

      expect(result.hitRounds.length, 2);
      expect(
          result.hitRounds
              .every((r) => r.hit && r.reactionTime != null),
          isTrue);
    });
  });

  group('DrillResult export', () {
    test('toTextSummary contains key info', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 250),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: false,
            timestamp: now),
      ]);

      final text = result.toTextSummary();
      expect(text, contains('DOMES Drill Results'));
      expect(text, contains('Type: Reaction'));
      expect(text, contains('Hit Rate: 50% (1/2)'));
      expect(text, contains('Avg Reaction: 250ms'));
      expect(text, contains('Best: 250ms'));
      expect(text, contains('HIT'));
      expect(text, contains('MISS'));
    });

    test('toJson produces valid JSON with correct fields', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 300),
            timestamp: now),
        RoundResult(
            roundIndex: 1,
            podAddress: 'pod-2',
            hit: false,
            timestamp: now),
      ]);

      final json = result.toJson();
      expect(json['drillType'], 'reaction');
      expect(json['roundCount'], 10);
      expect(json['hits'], 1);
      expect(json['misses'], 1);
      expect(json['hitRate'], 0.5);
      expect(json['avgReactionMs'], 300);
      expect(json['bestReactionMs'], 300);
      expect(json['worstReactionMs'], 300);
      expect(json['rounds'], isList);
      expect((json['rounds'] as List).length, 2);
    });

    test('toJsonString produces parseable JSON', () {
      final result = makeResult([
        RoundResult(
            roundIndex: 0,
            podAddress: 'pod-1',
            hit: true,
            reactionTime: const Duration(milliseconds: 200),
            timestamp: now),
      ]);

      final jsonStr = result.toJsonString();
      final parsed = jsonDecode(jsonStr) as Map<String, dynamic>;
      expect(parsed['drillType'], 'reaction');
      expect(parsed['hits'], 1);
    });

    test('toJson handles empty results', () {
      final result = makeResult([]);
      final json = result.toJson();
      expect(json['hits'], 0);
      expect(json['misses'], 0);
      expect(json['hitRate'], 0);
      expect(json['avgReactionMs'], isNull);
      expect(json['bestReactionMs'], isNull);
      expect(json['worstReactionMs'], isNull);
      expect((json['rounds'] as List), isEmpty);
    });
  });
}
