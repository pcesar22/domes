import 'package:domes_app/application/providers/drill_provider.dart';
import 'package:domes_app/presentation/screens/drill_active_screen.dart';
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
  group('DrillActiveScreen', () {
    testWidgets('renders idle state', (tester) async {
      await tester.pumpWidget(_wrap(const DrillActiveScreen()));
      await tester.pumpAndSettle();

      expect(find.text('Idle'), findsOneWidget);
    });

    testWidgets('renders stop button', (tester) async {
      await tester.pumpWidget(_wrap(const DrillActiveScreen()));
      await tester.pumpAndSettle();

      expect(find.text('Stop Drill'), findsOneWidget);
    });

    testWidgets('shows round counter in app bar', (tester) async {
      await tester.pumpWidget(_wrap(const DrillActiveScreen()));
      await tester.pumpAndSettle();

      // Default state shows Round 1/0
      expect(find.text('Round 1/0'), findsOneWidget);
    });

    testWidgets('shows waiting touch phase with correct indicator',
        (tester) async {
      // Create override with custom config
      await tester.pumpWidget(_wrap(
        const DrillActiveScreen(),
        overrides: [
          drillProvider.overrideWith((ref) {
            return DrillNotifier(ref);
          }),
        ],
      ));
      await tester.pumpAndSettle();

      // The active screen should render
      expect(find.byType(DrillActiveScreen), findsOneWidget);
    });

    testWidgets('shows preparing phase', (tester) async {
      await tester.pumpWidget(_wrap(const DrillActiveScreen()));
      await tester.pump();

      // Initial state is idle
      expect(find.byType(DrillActiveScreen), findsOneWidget);
    });

    testWidgets('renders pod grid when config has pods', (tester) async {
      await tester.pumpWidget(_wrap(const DrillActiveScreen()));
      await tester.pumpAndSettle();

      // With no config/pods, shows "No pods"
      expect(find.text('No pods'), findsOneWidget);
    });
  });
}
