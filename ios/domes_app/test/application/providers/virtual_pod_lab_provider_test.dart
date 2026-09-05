import 'dart:async';

import 'package:domes_app/application/providers/drill_provider.dart';
import 'package:domes_app/application/providers/multi_pod_provider.dart';
import 'package:domes_app/application/providers/virtual_pod_lab_provider.dart';
import 'package:domes_app/data/transport/virtual_pod_transport.dart';
import 'package:domes_app/domain/models/app_clock.dart';
import 'package:domes_app/domain/models/drill_config.dart';
import 'package:domes_app/domain/models/pod_device.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

Future<void> _flush() async {
  await Future<void>.delayed(Duration.zero);
  await Future<void>.delayed(Duration.zero);
}

final class _GatedVirtualMultiPodNotifier extends MultiPodNotifier {
  final connectStarted = Completer<void>();
  final connectGate = Completer<void>();

  @override
  Future<void> connectVirtualPod(
    PodDevice pod,
    VirtualPodTransport transport,
  ) async {
    if (!connectStarted.isCompleted) connectStarted.complete();
    await connectGate.future;
    await super.connectVirtualPod(pod, transport);
  }
}

void main() {
  test('launches stable distinct two- and six-pod identities', () async {
    final multiPod = MultiPodNotifier();
    final lab = VirtualPodLabNotifier(multiPod, clock: DeterministicAppClock());

    await lab.launch(podCount: 2, seed: 81);
    expect(lab.state.phase, VirtualPodLabPhase.running);
    expect(lab.state.seed, 81);
    expect(lab.state.pods.map((pod) => pod.address), [
      'app-virtual-pod-01',
      'app-virtual-pod-02',
    ]);
    expect(lab.state.pods.map((pod) => pod.address).toSet(), hasLength(2));
    expect(multiPod.connectedAddresses, hasLength(2));
    final oldTransports = lab.transports.toList();

    await lab.launch(podCount: 6, seed: 81);
    expect(lab.state.pods.map((pod) => pod.address).toSet(), hasLength(6));
    expect(multiPod.connectedAddresses, hasLength(6));
    expect(oldTransports.every((transport) => !transport.isConnected), isTrue);

    await lab.stop();
    expect(lab.state.phase, VirtualPodLabPhase.stopped);
    expect(multiPod.connectedAddresses, isEmpty);
    lab.dispose();
    multiPod.dispose();
  });

  test('touch notification retains the reporting virtual identity', () async {
    final multiPod = MultiPodNotifier();
    final lab = VirtualPodLabNotifier(multiPod, clock: DeterministicAppClock());
    await lab.launch(podCount: 2);
    final touch = multiPod.touchEvents.first;

    lab.emitTouch('app-virtual-pod-02', padIndex: 3);

    final event = await touch;
    expect(event.address, 'app-virtual-pod-02');
    expect((event.event.podId, event.event.padIndex), (2, 3));
    await lab.stop();
    lab.dispose();
    multiPod.dispose();
  });

  test('physical targets cannot enter the virtual transport path', () async {
    var physicalConnects = 0;
    final multiPod = MultiPodNotifier(
      connector: (_) async {
        physicalConnects++;
        throw StateError('physical connector selected');
      },
    );
    await multiPod.connectPod(
      const PodDevice(name: 'Physical', address: 'physical-01'),
    );
    expect(physicalConnects, 1);
    expect(
      multiPod.state['physical-01']!.error,
      contains('physical connector'),
    );

    final lab = VirtualPodLabNotifier(multiPod, clock: DeterministicAppClock());
    await lab.launch(podCount: 2);
    expect(physicalConnects, 1);
    expect(
      multiPod.state['physical-01']!.device.environment,
      PodEnvironment.physical,
    );
    await lab.stop();
    lab.dispose();
    multiPod.dispose();
  });

  test(
    'fixed seed and virtual time reproduce drill commands and results',
    () async {
      Future<(List<String>, List<String>)> execute() async {
        final clock = DeterministicAppClock();
        final multiPod = MultiPodNotifier();
        final lab = VirtualPodLabNotifier(multiPod, clock: clock);
        await lab.launch(podCount: 2, seed: 912);
        final container = ProviderContainer(
          overrides: [
            drillProvider.overrideWith(
              (ref) => DrillNotifier(
                ref,
                multiPod: multiPod,
                clock: clock,
                seed: lab.state.seed,
              ),
            ),
          ],
        );
        final drill = container.read(drillProvider.notifier);
        await drill.startDrill(
          DrillConfig(
            roundCount: 3,
            timeout: const Duration(seconds: 2),
            minDelay: const Duration(milliseconds: 500),
            maxDelay: const Duration(milliseconds: 500),
            podAddresses: lab.state.pods.map((pod) => pod.address).toList(),
          ),
        );

        for (var round = 0; round < 3; round++) {
          clock.advance(const Duration(milliseconds: 500));
          await _flush();
          final active = container.read(drillProvider).activePodAddress!;
          clock.advance(const Duration(milliseconds: 120));
          lab.emitTouch(active);
          await _flush();
        }

        final state = container.read(drillProvider);
        expect(state.phase, DrillPhase.finished);
        final results = state.results
            .map(
              (result) =>
                  '${result.roundIndex}|${result.podAddress}|'
                  '${result.reactionTime?.inMilliseconds}|'
                  '${result.timestamp.toUtc().toIso8601String()}',
            )
            .toList();
        final commands =
            lab.transports
                .expand((transport) => transport.commands)
                .map((command) => command.signature)
                .toList()
              ..sort();
        container.dispose();
        await lab.stop();
        lab.dispose();
        multiPod.dispose();
        return (results, commands);
      }

      final first = await execute();
      final second = await execute();
      expect(first.$1, second.$1);
      expect(first.$2, second.$2);
    },
  );

  test('stop and dispose cancel virtual timers and stale callbacks', () async {
    final clock = DeterministicAppClock();
    final multiPod = MultiPodNotifier();
    final lab = VirtualPodLabNotifier(multiPod, clock: clock);
    await lab.launch(podCount: 2);
    final transports = lab.transports.toList();
    final container = ProviderContainer(
      overrides: [
        drillProvider.overrideWith(
          (ref) =>
              DrillNotifier(ref, multiPod: multiPod, clock: clock, seed: 4),
        ),
      ],
    );
    final drill = container.read(drillProvider.notifier);
    await drill.startDrill(
      DrillConfig(
        roundCount: 2,
        minDelay: const Duration(seconds: 1),
        maxDelay: const Duration(seconds: 1),
        podAddresses: lab.state.pods.map((pod) => pod.address).toList(),
      ),
    );
    drill.stopDrill();
    clock.advance(const Duration(seconds: 5));
    await _flush();
    expect(container.read(drillProvider).phase, DrillPhase.idle);
    expect(container.read(drillProvider).results, isEmpty);
    expect(clock.pendingTimerCount, 0);

    container.dispose();
    await lab.stop();
    await _flush();
    expect(transports.every((transport) => !transport.isConnected), isTrue);
    for (final transport in transports) {
      expect(() => transport.emitTouch(), throwsStateError);
    }
    lab.dispose();
    multiPod.dispose();
  });

  test('dispose running lab removes connections and touch callbacks', () async {
    final clock = DeterministicAppClock();
    final multiPod = MultiPodNotifier();
    final lab = VirtualPodLabNotifier(multiPod, clock: clock);
    await lab.launch(podCount: 2);
    final transports = lab.transports.toList();
    var touches = 0;
    final touchesSubscription = multiPod.touchEvents.listen((_) => touches++);

    lab.dispose();
    await lab.lifecycleSettled;

    expect(lab.transports, isEmpty);
    expect(multiPod.state, isEmpty);
    expect(multiPod.connectedAddresses, isEmpty);
    expect(clock.pendingTimerCount, 0);
    expect(transports.every((transport) => !transport.isConnected), isTrue);
    for (final transport in transports) {
      expect(() => transport.emitTouch(), throwsStateError);
    }
    await _flush();
    expect(touches, 0);

    await touchesSubscription.cancel();
    multiPod.dispose();
  });

  test('dispose during gated connect cannot leak late ownership', () async {
    final clock = DeterministicAppClock();
    final multiPod = _GatedVirtualMultiPodNotifier();
    final lab = VirtualPodLabNotifier(multiPod, clock: clock);
    final launch = lab.launch(podCount: 2);
    await multiPod.connectStarted.future;
    final transport = lab.transports.single;
    var touches = 0;
    final touchesSubscription = multiPod.touchEvents.listen((_) => touches++);

    lab.dispose();
    expect(transport.isConnected, isFalse);
    multiPod.connectGate.complete();
    await launch;
    await lab.lifecycleSettled;

    expect(lab.transports, isEmpty);
    expect(multiPod.state, isEmpty);
    expect(multiPod.connectedAddresses, isEmpty);
    expect(multiPod.activeConnectionGeneration(transport.address), isNull);
    expect(clock.pendingTimerCount, 0);
    expect(() => transport.emitTouch(), throwsStateError);
    await _flush();
    expect(touches, 0);

    await touchesSubscription.cancel();
    multiPod.dispose();
  });
}
