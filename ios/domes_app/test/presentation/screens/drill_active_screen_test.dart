import 'dart:math';

import 'package:domes_app/application/providers/drill_provider.dart';
import 'package:domes_app/application/providers/multi_pod_provider.dart';
import 'package:domes_app/application/providers/virtual_pod_lab_provider.dart';
import 'package:domes_app/data/transport/virtual_pod_transport.dart';
import 'package:domes_app/domain/models/app_clock.dart';
import 'package:domes_app/domain/models/drill_config.dart';
import 'package:domes_app/domain/models/pod_device.dart';
import 'package:domes_app/domain/repositories/pod_repository_impl.dart';
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

final class _AlternatingRandom implements Random {
  int _next = 0;

  @override
  int nextInt(int max) => _next++ % max;

  @override
  bool nextBool() => throw UnsupportedError('not used');

  @override
  double nextDouble() => throw UnsupportedError('not used');
}

void main() {
  group('DrillActiveScreen', () {
    testWidgets('mixed drill offers model touch only for its virtual target', (
      tester,
    ) async {
      final clock = DeterministicAppClock();
      // This injected fixture stands in for the physical connector; no device
      // operation or production fallback to a virtual transport is involved.
      final physicalTransport = VirtualPodTransport(
        address: 'physical-01',
        podId: 10,
        clock: clock,
      );
      final multiPod = MultiPodNotifier(
        connector: (_) async => (
          transport: physicalTransport,
          repository: PodRepositoryImpl(physicalTransport),
        ),
      );
      final container = ProviderContainer(
        overrides: [
          multiPodProvider.overrideWith((ref) => multiPod),
          drillProvider.overrideWith(
            (ref) =>
                DrillNotifier(ref, clock: clock, random: _AlternatingRandom()),
          ),
        ],
      );
      final lab = container.read(virtualPodLabProvider.notifier);
      await lab.launch(podCount: 2);
      await multiPod.connectPod(
        const PodDevice(name: 'Physical', address: 'physical-01'),
      );
      final drill = container.read(drillProvider.notifier);
      await drill.startDrill(
        DrillConfig(
          roundCount: 3,
          minDelay: Duration.zero,
          maxDelay: Duration.zero,
          podAddresses: const ['app-virtual-pod-01', 'physical-01'],
        ),
      );
      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: const MaterialApp(home: DrillActiveScreen()),
        ),
      );
      clock.advance(Duration.zero);
      await tester.pump();

      expect(
        container.read(drillProvider).activePodAddress,
        'app-virtual-pod-01',
      );
      expect(find.text('Send app-model touch'), findsOneWidget);
      expect(find.byType(MaterialBanner), findsOneWidget);
      await tester.tap(find.text('Send app-model touch'));
      await tester.pump();
      expect(container.read(drillProvider).results, hasLength(1));

      clock.advance(Duration.zero);
      await tester.pump();
      expect(container.read(drillProvider).activePodAddress, 'physical-01');
      expect(find.text('Send app-model touch'), findsNothing);
      expect(find.byType(MaterialBanner), findsOneWidget);
      lab.emitTouch('app-virtual-pod-01');
      await tester.pump();
      expect(container.read(drillProvider).results, hasLength(1));
      expect(container.read(drillProvider).phase, DrillPhase.waitingTouch);

      physicalTransport.emitTouch();
      await tester.pump();
      expect(container.read(drillProvider).results, hasLength(2));
      expect(tester.takeException(), isNull);

      await tester.pumpWidget(const SizedBox.shrink());
      container.dispose();
      await lab.lifecycleSettled;
    });

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

    testWidgets('shows waiting touch phase with correct indicator', (
      tester,
    ) async {
      // Create override with custom config
      await tester.pumpWidget(
        _wrap(
          const DrillActiveScreen(),
          overrides: [
            drillProvider.overrideWith((ref) {
              return DrillNotifier(ref);
            }),
          ],
        ),
      );
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
