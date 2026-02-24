import 'package:domes_app/domain/models/drill_config.dart';
import 'package:domes_app/domain/models/drill_result.dart';
import 'package:domes_app/presentation/screens/drill_results_screen.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

Widget _wrap(Widget child) {
  return ProviderScope(
    child: MaterialApp(home: child),
  );
}

DrillResult _makeResult({
  List<RoundResult>? rounds,
  DrillType type = DrillType.reaction,
}) {
  final now = DateTime(2025, 6, 15, 12, 0, 0);
  return DrillResult(
    config: DrillConfig(
      type: type,
      roundCount: 5,
      podAddresses: ['pod-1', 'pod-2'],
    ),
    rounds: rounds ??
        [
          RoundResult(
              roundIndex: 0,
              podAddress: 'pod-1',
              hit: true,
              reactionTime: const Duration(milliseconds: 250),
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
              reactionTime: const Duration(milliseconds: 180),
              timestamp: now),
        ],
    startTime: now,
    endTime: now.add(const Duration(seconds: 30)),
  );
}

void main() {
  group('DrillResultsScreen', () {
    testWidgets('shows summary card with hit rate', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Summary'), findsOneWidget);
      expect(find.text('Hit Rate'), findsOneWidget);
      expect(find.text('75% (3/4)'), findsOneWidget);
    });

    testWidgets('shows avg, best, worst reaction times', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Avg Reaction'), findsOneWidget);
      expect(find.text('Best'), findsOneWidget);
      expect(find.text('Worst'), findsOneWidget);
      expect(find.text('180ms'), findsAtLeastNWidgets(1)); // Best
      expect(find.text('400ms'), findsAtLeastNWidgets(1)); // Worst
    });

    testWidgets('shows duration', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Duration'), findsOneWidget);
      expect(find.text('30s'), findsOneWidget);
    });

    testWidgets('shows drill type', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Drill Type'), findsOneWidget);
      expect(find.text('Reaction'), findsOneWidget);
    });

    testWidgets('shows pods used count', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Pods Used'), findsOneWidget);
      // '2' may appear multiple times (chart labels etc), just verify it exists
      expect(find.text('2'), findsAtLeastNWidgets(1));
    });

    testWidgets('shows round details section', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      // Scroll down to find Round Details
      await tester.scrollUntilVisible(
          find.text('Round Details'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('Round Details'), findsOneWidget);
    });

    testWidgets('shows miss label for missed rounds', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      // Scroll down to find the miss text
      await tester.scrollUntilVisible(
          find.text('miss'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('miss'), findsOneWidget);
    });

    testWidgets('shows per-pod breakdown for multi-pod drills',
        (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      await tester.scrollUntilVisible(
          find.text('Per-Pod Breakdown'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('Per-Pod Breakdown'), findsOneWidget);
    });

    testWidgets('shows reaction time chart', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Reaction Times'), findsOneWidget);
    });

    testWidgets('done button is present', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      await tester.scrollUntilVisible(
          find.text('Done'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('Done'), findsOneWidget);
    });

    testWidgets('share menu is present', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      // Share button in app bar
      expect(find.byIcon(Icons.share), findsOneWidget);
    });

    testWidgets('share menu shows text and JSON options', (tester) async {
      final result = _makeResult();
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      // Tap share button
      await tester.tap(find.byIcon(Icons.share));
      await tester.pumpAndSettle();

      expect(find.text('Copy as Text'), findsOneWidget);
      expect(find.text('Copy as JSON'), findsOneWidget);
    });

    testWidgets('handles empty results gracefully', (tester) async {
      final result = _makeResult(rounds: []);
      await tester.pumpWidget(_wrap(DrillResultsScreen(result: result)));
      await tester.pumpAndSettle();

      expect(find.text('Summary'), findsOneWidget);
      expect(find.text('Hit Rate'), findsOneWidget);
    });
  });
}
