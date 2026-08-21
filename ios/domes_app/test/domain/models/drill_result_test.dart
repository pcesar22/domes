import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
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
    test('matches the deterministic two-pod scoring fixture', () {
      var fixtureFile = File(
        'tools/scoring_validation/fixtures/fixed_two_pod_v1.json',
      );
      for (var depth = 0; !fixtureFile.existsSync() && depth < 5; depth++) {
        fixtureFile = File('../${fixtureFile.path}');
      }
      expect(
        fixtureFile.existsSync(),
        isTrue,
        reason: 'fixture must be reachable',
      );
      final fixtureBytes = fixtureFile.readAsBytesSync();
      final fixture =
          jsonDecode(utf8.decode(fixtureBytes)) as Map<String, dynamic>;
      final fixtureRounds = (fixture['rounds'] as List<dynamic>)
          .cast<Map<String, dynamic>>();
      final rounds = fixtureRounds.map((round) {
        final reactionUs = round['reaction_time_us'] as int?;
        return RoundResult(
          roundIndex: round['index'] as int,
          podAddress: round['target_identity'] as String,
          hit: round['hit'] as bool,
          reactionTime: reactionUs == null
              ? null
              : Duration(microseconds: reactionUs),
          timestamp: now,
        );
      }).toList();
      final result = makeResult(rounds);

      expect(result.hits, 4);
      expect(result.misses, 2);
      expect(result.avgReactionTime, const Duration(milliseconds: 1025));
      expect(result.bestReactionTime, const Duration(milliseconds: 1));
      expect(result.worstReactionTime, const Duration(milliseconds: 2999));
      expect(result.perPodResults['local-pod-0'], hasLength(3));
      expect(result.perPodResults['peer-pod-1'], hasLength(3));
      expect(
        fixtureRounds.map((round) => round['round_token']),
        everyElement(greaterThan(0)),
      );

      final outputPath = Platform.environment['DOMES_MOBILE_SCORING_RESULT'];
      if (outputPath != null) {
        final mobilePath =
            (fixture['paths'] as Map<String, dynamic>)['mobile']
                as Map<String, dynamic>;
        File(outputPath).writeAsStringSync(
          '${const JsonEncoder.withIndent('  ').convert({
            'aggregate': {'average_reaction_us': result.avgReactionTime?.inMicroseconds, 'best_reaction_us': result.bestReactionTime?.inMicroseconds, 'hits': result.hits, 'misses': result.misses, 'worst_reaction_us': result.worstReactionTime?.inMicroseconds},
            'clock_provenance': mobilePath['clock'],
            'fixture_id': fixture['fixture_id'],
            'fixture_sha256': sha256.convert(fixtureBytes).toString(),
            'path': 'mobile',
            'result_provenance': mobilePath['result'],
            'rounds': rounds.map((round) => {'hit': round.hit, 'index': round.roundIndex, 'reaction_time_us': round.reactionTime?.inMicroseconds, 'round_token': null, 'target_identity': round.podAddress}).toList(),
            'schema_version': 1,
          })}\n',
        );
      }
    });

    test('counts hits and misses', () {
      final result = makeResult([
        RoundResult(
          roundIndex: 0,
          podAddress: 'pod-1',
          hit: true,
          reactionTime: const Duration(milliseconds: 300),
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: false,
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 2,
          podAddress: 'pod-1',
          hit: true,
          reactionTime: const Duration(milliseconds: 500),
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: true,
          reactionTime: const Duration(milliseconds: 400),
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 2,
          podAddress: 'pod-1',
          hit: true,
          reactionTime: const Duration(milliseconds: 600),
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: true,
          reactionTime: const Duration(milliseconds: 800),
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 2,
          podAddress: 'pod-1',
          hit: true,
          reactionTime: const Duration(milliseconds: 350),
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: false,
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: true,
          reactionTime: const Duration(milliseconds: 400),
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 2,
          podAddress: 'pod-1',
          hit: false,
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 3,
          podAddress: 'pod-2',
          hit: true,
          reactionTime: const Duration(milliseconds: 300),
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: false,
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 2,
          podAddress: 'pod-1',
          hit: true,
          reactionTime: const Duration(milliseconds: 500),
          timestamp: now,
        ),
      ]);

      expect(result.hitRounds.length, 2);
      expect(
        result.hitRounds.every((r) => r.hit && r.reactionTime != null),
        isTrue,
      );
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: false,
          timestamp: now,
        ),
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
          timestamp: now,
        ),
        RoundResult(
          roundIndex: 1,
          podAddress: 'pod-2',
          hit: false,
          timestamp: now,
        ),
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
          timestamp: now,
        ),
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
