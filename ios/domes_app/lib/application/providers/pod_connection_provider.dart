/// Pod connection provider using Riverpod.
///
/// Manages the BLE connection lifecycle and provides access to the
/// PodRepository for the connected device.
library;

import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/proto/generated/config.pb.dart';
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
  Future<void> _cleanupBarrier = Future<void>.value();
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
      await _cleanupBarrier;
      if (!mounted || generation != _generation) return;
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;
      await previous?.disconnect();
      if (!mounted || generation != _generation) return;

      final connected = await _connector(pod);
      final transport = connected.transport;
      pendingTransport = transport;
      final repository = _QuarantiningPodRepository(
        connected.repository,
        (error) => _quarantine(pod, transport, generation, error),
      );
      if (!mounted || generation != _generation) {
        await transport.disconnect();
        return;
      }

      _connectionSubscription = repository.touchEvents.listen(
        (_) {},
        onError: (Object error, StackTrace _) {
          repository.quarantine(error);
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

  void _quarantine(
    PodDevice pod,
    Transport transport,
    int generation,
    Object error,
  ) {
    if (!mounted || generation != _generation) return;

    _generation++;
    final subscription = _connectionSubscription;
    _connectionSubscription = null;
    _cleanupBarrier = _cleanupFailedConnection(subscription, transport);
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
    final generation = ++_generation;
    final subscription = _connectionSubscription;
    _connectionSubscription = null;
    final transport = state.transport;
    final device = state.device;
    state = ConnectedPodState(
      device: device?.copyWith(
        connectionState: PodConnectionState.disconnected,
      ),
    );
    await _cleanupBarrier;
    if (transport != null) {
      await _cleanupFailedConnection(subscription, transport);
    } else {
      try {
        await subscription?.cancel();
      } catch (_) {
        // Best effort — the captured generation is already disconnected.
      }
    }
    if (!mounted || generation != _generation) return;
    state = ConnectedPodState(
      device: device?.copyWith(
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

final class _QuarantiningPodRepository implements PodRepository {
  _QuarantiningPodRepository(this._delegate, this._onFailure);

  final PodRepository _delegate;
  final void Function(Object error) _onFailure;
  bool _quarantined = false;

  void quarantine(Object error) {
    if (_quarantined) return;
    _quarantined = true;
    _onFailure(error);
  }

  Future<T> _guard<T>(Future<T> Function() operation) async {
    if (_quarantined) {
      throw StateError('Pod repository requires an explicit reconnect');
    }
    try {
      return await operation();
    } catch (error) {
      quarantine(error);
      rethrow;
    }
  }

  @override
  Stream<AppTouchEvent> get touchEvents => _delegate.touchEvents;

  @override
  Future<List<AppFeatureState>> listFeatures() =>
      _guard(_delegate.listFeatures);

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      _guard(() => _delegate.setFeature(feature, enabled));

  @override
  Future<AppLedPattern> getLedPattern() => _guard(_delegate.getLedPattern);

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) =>
      _guard(() => _delegate.setLedPattern(pattern));

  @override
  Future<AppModeInfo> getSystemMode() => _guard(_delegate.getSystemMode);

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) =>
      _guard(() => _delegate.setSystemMode(mode));

  @override
  Future<AppSystemInfo> getSystemInfo() => _guard(_delegate.getSystemInfo);

  @override
  Future<int> getAudioVolume() => _guard(_delegate.getAudioVolume);

  @override
  Future<int> setAudioVolume(int volume) =>
      _guard(() => _delegate.setAudioVolume(volume));

  @override
  Future<bool> triggerFeedback(FeedbackProbe probe) =>
      _guard(() => _delegate.triggerFeedback(probe));
}
