/// Pod connection provider using Riverpod.
///
/// Manages the BLE connection lifecycle and provides access to the
/// PodRepository for the connected device.
library;

import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/config_protocol.dart';
import '../../data/transport/ble_transport.dart';
import '../../data/transport/transport.dart';
import '../../domain/models/pod_device.dart';
import '../../domain/repositories/pod_repository.dart';
import '../../domain/repositories/pod_repository_impl.dart';

/// State for a pod connection.
class ConnectedPodState {
  final PodDevice? device;
  final Transport? transport;
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

typedef SinglePodConnector =
    Future<({Transport transport, PodRepository repository})> Function(
      PodDevice pod,
    );

/// Notifier that manages connection to a single pod.
class PodConnectionNotifier extends StateNotifier<ConnectedPodState> {
  PodConnectionNotifier({SinglePodConnector? connector})
    : _connector = connector ?? _connectBlePod,
      super(const ConnectedPodState());

  final SinglePodConnector _connector;
  StreamSubscription<AppTouchEvent>? _connectionSubscription;
  int _generation = 0;

  /// Connect to a pod.
  Future<void> connect(PodDevice pod) async {
    final generation = ++_generation;
    final previous = state.transport;

    state = ConnectedPodState(
      device: pod.copyWith(connectionState: PodConnectionState.connecting),
    );

    Transport? pendingTransport;
    try {
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;
      await previous?.disconnect();
      if (!mounted || generation != _generation) return;

      final connected = await _connector(pod);
      final transport = connected.transport;
      pendingTransport = transport;
      final repository = connected.repository;
      if (!mounted || generation != _generation) {
        await transport.disconnect();
        return;
      }

      _connectionSubscription = repository.touchEvents.listen(
        (_) {},
        onError: (Object error, StackTrace _) {
          _handleConnectionStreamError(pod, transport, generation, error);
        },
      );

      state = ConnectedPodState(
        device: pod.copyWith(connectionState: PodConnectionState.connected),
        transport: transport,
        repository: repository,
      );
      pendingTransport = null;
    } catch (e) {
      await pendingTransport?.disconnect();
      if (!mounted || generation != _generation) return;
      state = ConnectedPodState(
        device: pod.copyWith(connectionState: PodConnectionState.disconnected),
        error: 'Connection failed: $e',
      );
    }
  }

  void _handleConnectionStreamError(
    PodDevice pod,
    Transport transport,
    int generation,
    Object error,
  ) {
    if (!mounted || generation != _generation) return;

    _generation++;
    final subscription = _connectionSubscription;
    _connectionSubscription = null;
    unawaited(_cleanupFailedConnection(subscription, transport));
    state = ConnectedPodState(
      device: pod.copyWith(connectionState: PodConnectionState.disconnected),
      error: '$error',
    );
  }

  static Future<void> _cleanupFailedConnection(
    StreamSubscription<AppTouchEvent>? subscription,
    Transport transport,
  ) async {
    try {
      await subscription?.cancel();
    } catch (_) {
      // Continue with transport cleanup.
    }
    try {
      await transport.disconnect();
    } catch (_) {
      // The connection is already unusable; cleanup is best effort.
    }
  }

  /// Disconnect from the current pod.
  Future<void> disconnect() async {
    _generation++;
    final transport = state.transport;
    try {
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;
      await transport?.disconnect();
    } catch (_) {
      // Best effort — always update state to disconnected
    }
    state = ConnectedPodState(
      device: state.device?.copyWith(
        connectionState: PodConnectionState.disconnected,
      ),
    );
  }

  static Future<({Transport transport, PodRepository repository})>
  _connectBlePod(PodDevice pod) async {
    final device = pod.bleDevice;
    if (device == null) {
      throw StateError('No BLE device');
    }
    final transport = await BleTransport.connect(device);
    return (transport: transport, repository: PodRepositoryImpl(transport));
  }

  @override
  void dispose() {
    _generation++;
    final subscription = _connectionSubscription;
    if (subscription != null) {
      unawaited(subscription.cancel());
    }
    final transport = state.transport;
    if (transport != null) {
      unawaited(transport.disconnect());
    }
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
