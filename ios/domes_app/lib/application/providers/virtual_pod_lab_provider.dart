/// Lifecycle owner for the deterministic app virtual pod model.
library;

import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/transport/virtual_pod_transport.dart';
import '../../domain/models/app_clock.dart';
import '../../domain/models/pod_device.dart';
import 'multi_pod_provider.dart';

enum VirtualPodLabPhase { stopped, starting, running, error }

final class VirtualPodLabState {
  const VirtualPodLabState({
    this.phase = VirtualPodLabPhase.stopped,
    this.pods = const [],
    this.seed = 197,
    this.error,
  });

  final VirtualPodLabPhase phase;
  final List<PodDevice> pods;
  final int seed;
  final String? error;

  bool ownsAddress(String? address) =>
      address != null && pods.any((pod) => pod.address == address);
}

final class VirtualPodLabNotifier extends StateNotifier<VirtualPodLabState> {
  VirtualPodLabNotifier(
    this._multiPod, {
    AppClock clock = const SystemAppClock(),
  }) : _clock = clock,
       super(const VirtualPodLabState());

  final MultiPodNotifier _multiPod;
  final AppClock _clock;
  final Map<String, VirtualPodTransport> _transports = {};
  int _generation = 0;
  Future<void> _operationTail = Future<void>.value();

  List<VirtualPodTransport> get transports =>
      List.unmodifiable(_transports.values);

  Future<void> launch({required int podCount, int seed = 197}) async {
    if (podCount != 2 && podCount != 6) {
      throw ArgumentError.value(podCount, 'podCount', 'must be 2 or 6');
    }

    final generation = ++_generation;
    return _enqueue(() => _launch(generation, podCount, seed));
  }

  Future<void> _launch(int generation, int podCount, int seed) async {
    await _disconnectOwnedPods();
    if (!mounted || generation != _generation) return;

    final pods = List<PodDevice>.generate(
      podCount,
      (index) => PodDevice(
        name: 'Virtual Pod ${index + 1}',
        address: 'app-virtual-pod-${(index + 1).toString().padLeft(2, '0')}',
        rssi: -40 - index,
        environment: PodEnvironment.appVirtualModel,
      ),
      growable: false,
    );
    state = VirtualPodLabState(
      phase: VirtualPodLabPhase.starting,
      pods: pods,
      seed: seed,
    );

    try {
      for (var index = 0; index < pods.length; index++) {
        final pod = pods[index];
        final transport = VirtualPodTransport(
          address: pod.address,
          podId: index + 1,
          clock: _clock,
        );
        _transports[pod.address] = transport;
        await _multiPod.connectVirtualPod(pod, transport);
        if (!mounted || generation != _generation) {
          await transport.disconnect();
          return;
        }
      }
      state = VirtualPodLabState(
        phase: VirtualPodLabPhase.running,
        pods: pods,
        seed: seed,
      );
    } catch (error) {
      await _disconnectOwnedPods();
      if (!mounted || generation != _generation) return;
      state = VirtualPodLabState(
        phase: VirtualPodLabPhase.error,
        seed: seed,
        error: '$error',
      );
    }
  }

  void emitTouch(String address, {int padIndex = 0}) {
    if (state.phase != VirtualPodLabPhase.running ||
        !state.ownsAddress(address) ||
        _multiPod.activeConnectionGeneration(address) == null) {
      throw StateError('Virtual pod $address is not active in this lab');
    }
    _transports[address]!.emitTouch(padIndex: padIndex);
  }

  Future<void> stop() async {
    final generation = ++_generation;
    await _enqueue(() async {
      await _disconnectOwnedPods();
      if (mounted && generation == _generation) {
        state = const VirtualPodLabState();
      }
    });
  }

  Future<void> _enqueue(Future<void> Function() operation) {
    final result = _operationTail.then((_) => operation());
    _operationTail = result.catchError((_) {});
    return result;
  }

  Future<void> _disconnectOwnedPods() async {
    final addresses = _transports.keys.toList(growable: false);
    for (final address in addresses) {
      await _multiPod.disconnectPod(address);
    }
    _transports.clear();
  }

  @override
  void dispose() {
    ++_generation;
    for (final entry in _transports.entries) {
      if (_multiPod.mounted) {
        unawaited(_multiPod.disconnectPod(entry.key));
      } else {
        unawaited(entry.value.disconnect());
      }
    }
    _transports.clear();
    super.dispose();
  }
}

final virtualPodLabProvider =
    StateNotifierProvider<VirtualPodLabNotifier, VirtualPodLabState>((ref) {
      return VirtualPodLabNotifier(ref.read(multiPodProvider.notifier));
    });
