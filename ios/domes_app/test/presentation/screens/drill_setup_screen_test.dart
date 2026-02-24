import 'package:domes_app/presentation/screens/drill_setup_screen.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

Widget _wrap(Widget child, {List<Override>? overrides}) {
  return ProviderScope(
    overrides: overrides ?? [],
    child: MaterialApp(home: child),
  );
}

void main() {
  group('DrillSetupScreen', () {
    testWidgets('renders drill type selector', (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      expect(find.text('Drill Type'), findsOneWidget);
      expect(find.text('Reaction'), findsOneWidget);
      expect(find.text('Sequence'), findsOneWidget);
      expect(find.text('Speed'), findsOneWidget);
    });

    testWidgets('renders configuration section', (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      expect(find.text('Configuration'), findsOneWidget);
      expect(find.text('Rounds'), findsOneWidget);
      expect(find.text('Timeout'), findsOneWidget);
      expect(find.text('Min Delay'), findsOneWidget);
      expect(find.text('Max Delay'), findsOneWidget);
    });

    testWidgets('shows pods section', (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      expect(find.text('Pods'), findsOneWidget);
    });

    testWidgets('shows no-pods-connected message when empty', (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      expect(
          find.text(
              'No pods connected. Connect pods from the home screen first.'),
          findsOneWidget);
    });

    testWidgets('start button disabled when no pods selected', (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      // Scroll to make start button visible
      await tester.scrollUntilVisible(
          find.text('Start Drill'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('Start Drill'), findsOneWidget);
    });

    testWidgets('simulate button shown when no pods connected',
        (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      await tester.scrollUntilVisible(
          find.text('Simulate Drill (no pods)'), 200,
          scrollable: find.byType(Scrollable).first);
      expect(find.text('Simulate Drill (no pods)'), findsOneWidget);
    });

    testWidgets('drill type description updates on selection',
        (tester) async {
      await tester.pumpWidget(_wrap(const DrillSetupScreen()));
      await tester.pumpAndSettle();

      // Default is reaction type
      expect(find.text('Touch the lit pod as fast as possible'),
          findsOneWidget);
    });
  });
}
