/// Pod connection provider using Riverpod.
///
/// Manages the BLE connection lifecycle and provides access to the
/// PodRepository for the connected device.
library;

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/transport/ble_transport.dart';
import '../../domain/models/pod_device.dart';
import '../../domain/repositories/pod_repository.dart';
import '../../domain/repositories/pod_repository_impl.dart';

/// State for a pod connection.
class ConnectedPodState {
  final PodDevice? device;
  final BleTransport? transport;
  final PodRepository? repository;
  final String? error;

  const ConnectedPodState({
    this.device,
    this.transport,
    this.repository,
    this.error,
  });

  bool get isConnected => transport != null && transport!.isConnected;
}

/// Notifier that manages connection to a single pod.
class PodConnectionNotifier extends StateNotifier<ConnectedPodState> {
  PodConnectionNotifier() : super(const ConnectedPodState());

  /// Connect to a pod.
  Future<void> connect(PodDevice pod) async {
    if (pod.bleDevice == null) {
      state = const ConnectedPodState(error: 'No BLE device');
      return;
    }

    state = ConnectedPodState(
      device: pod.copyWith(
          connectionState: PodConnectionState.connecting),
    );

    try {
      final transport = await BleTransport.connect(pod.bleDevice!);
      final repository = PodRepositoryImpl(transport);

      state = ConnectedPodState(
        device: pod.copyWith(
            connectionState: PodConnectionState.connected),
        transport: transport,
        repository: repository,
      );
    } catch (e) {
      state = ConnectedPodState(
        device: pod.copyWith(
            connectionState: PodConnectionState.disconnected),
        error: 'Connection failed: $e',
      );
    }
  }

  /// Disconnect from the current pod.
  Future<void> disconnect() async {
    await state.transport?.disconnect();
    state = ConnectedPodState(
      device: state.device?.copyWith(
          connectionState: PodConnectionState.disconnected),
    );
  }

  @override
  void dispose() {
    state.transport?.disconnect();
    super.dispose();
  }
}

/// Provider for the current pod connection.
final podConnectionProvider =
    StateNotifierProvider<PodConnectionNotifier, ConnectedPodState>((ref) {
  return PodConnectionNotifier();
});

/// Convenience provider for the PodRepository (null if not connected).
final podRepositoryProvider = Provider<PodRepository?>((ref) {
  return ref.watch(podConnectionProvider).repository;
});
