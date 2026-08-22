/// Multi-pod connection provider.
///
/// Manages simultaneous BLE connections to multiple DOMES pods.
library;

import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/proto/generated/config.pbenum.dart';
import '../../data/protocol/config_protocol.dart';
import '../../data/transport/ble_transport.dart';
import '../../data/transport/transport.dart';
import '../../domain/models/pod_device.dart';
import '../../domain/repositories/pod_repository.dart';
import '../../domain/repositories/pod_repository_impl.dart';

/// State for a single pod connection in multi-pod mode.
class PodConnectionEntry {
  final PodDevice device;
  final Transport? transport;
  final PodRepository? repository;
  final String? error;

  const PodConnectionEntry({
    required this.device,
    this.transport,
    this.repository,
    this.error,
  });

  bool get isConnected => transport != null && transport!.isConnected;
}

typedef PodConnector =
    Future<({Transport transport, PodRepository repository})> Function(
      PodDevice pod,
    );

/// A touch edge associated with the BLE connection that reported it.
class PodTouchEvent {
  final String address;
  final AppTouchEvent event;

  const PodTouchEvent({required this.address, required this.event});
}

/// A connection failure associated with the pod connection that reported it.
class PodConnectionFailure {
  final String address;
  final int generation;
  final Object error;
  final StackTrace stackTrace;

  const PodConnectionFailure({
    required this.address,
    required this.generation,
    required this.error,
    required this.stackTrace,
  });
}

/// An operator action that superseded a live multi-pod connection.
class PodLifecycleFailure implements Exception {
  final String address;
  final String action;

  const PodLifecycleFailure({required this.address, required this.action});

  @override
  String toString() => 'Pod $address connection superseded by $action';
}

/// Manages multiple pod connections.
class MultiPodNotifier extends StateNotifier<Map<String, PodConnectionEntry>> {
  MultiPodNotifier({PodConnector? connector})
    : _connector = connector ?? _connectBlePod,
      super({});

  final PodConnector _connector;

  final StreamController<PodTouchEvent> _touchEvents =
      StreamController<PodTouchEvent>.broadcast();
  final StreamController<PodConnectionFailure> _connectionFailures =
      StreamController<PodConnectionFailure>.broadcast();
  final StreamController<PodConnectionFailure> _lifecycleFailures =
      StreamController<PodConnectionFailure>.broadcast();
  final Map<String, StreamSubscription<AppTouchEvent>> _touchSubscriptions = {};
  final Map<String, int> _connectionGenerations = {};
  final Map<String, Future<void>> _connectionCleanupTails = {};

  /// Physical touch edges from every connected pod.
  Stream<PodTouchEvent> get touchEvents => _touchEvents.stream;

  /// Terminal failures from individual pod connections.
  Stream<PodConnectionFailure> get connectionFailures =>
      _connectionFailures.stream;

  /// Operator-driven transitions away from live connection generations.
  Stream<PodConnectionFailure> get lifecycleFailures =>
      _lifecycleFailures.stream;

  /// Connect to a pod by address.
  Future<void> connectPod(PodDevice pod) async {
    await _connectionCleanupTails[pod.address];
    if (!mounted) return;

    final previousGeneration = _connectionGenerations[pod.address];
    final previous = state[pod.address];
    final generation = (_connectionGenerations[pod.address] ?? 0) + 1;
    _connectionGenerations[pod.address] = generation;
    state = {
      ...state,
      pod.address: PodConnectionEntry(
        device: pod.copyWith(connectionState: PodConnectionState.connecting),
      ),
    };
    if (previousGeneration != null && previous?.isConnected == true) {
      _publishLifecycleFailure(
        pod.address,
        previousGeneration,
        'replacement connect',
      );
    }

    Transport? pendingTransport;
    try {
      await _touchSubscriptions.remove(pod.address)?.cancel();
      await previous?.transport?.disconnect();
      if (!mounted || _connectionGenerations[pod.address] != generation) return;

      final connected = await _connector(pod);
      final transport = connected.transport;
      pendingTransport = transport;
      final repository = connected.repository;

      if (!mounted || _connectionGenerations[pod.address] != generation) {
        await transport.disconnect();
        return;
      }

      _touchSubscriptions[pod.address] = repository.touchEvents.listen(
        (event) {
          if (!mounted || _connectionGenerations[pod.address] != generation) {
            return;
          }
          _touchEvents.add(PodTouchEvent(address: pod.address, event: event));
        },
        onError: (Object error, StackTrace stackTrace) {
          _handlePodStreamError(pod.address, generation, error, stackTrace);
        },
      );

      state = {
        ...state,
        pod.address: PodConnectionEntry(
          device: pod.copyWith(connectionState: PodConnectionState.connected),
          transport: transport,
          repository: repository,
        ),
      };
      pendingTransport = null;
    } catch (e) {
      await pendingTransport?.disconnect();
      if (!mounted || _connectionGenerations[pod.address] != generation) return;
      state = {
        ...state,
        pod.address: PodConnectionEntry(
          device: pod.copyWith(
            connectionState: PodConnectionState.disconnected,
          ),
          error: '$e',
        ),
      };
    }
  }

  void _handlePodStreamError(
    String address,
    int generation,
    Object error,
    StackTrace stackTrace,
  ) {
    if (!mounted || _connectionGenerations[address] != generation) return;

    final entry = state[address];
    if (entry == null) return;
    unawaited(
      _quarantineConnection(address, generation, entry, error, stackTrace),
    );
  }

  Future<void> _quarantineConnection(
    String address,
    int generation,
    PodConnectionEntry entry,
    Object error,
    StackTrace stackTrace,
  ) async {
    if (!mounted || _connectionGenerations[address] != generation) return;

    _connectionGenerations[address] = generation + 1;
    final subscription = _touchSubscriptions.remove(address);
    final transport = entry.transport;
    final cleanup = transport == null
        ? _cancelSubscription(subscription)
        : _cleanupFailedConnection(subscription, transport);
    _connectionCleanupTails[address] = cleanup;
    state = {
      ...state,
      address: PodConnectionEntry(
        device: entry.device.copyWith(
          connectionState: PodConnectionState.disconnected,
        ),
        error: '$error',
      ),
    };

    try {
      await cleanup;
    } finally {
      if (identical(_connectionCleanupTails[address], cleanup)) {
        _connectionCleanupTails.remove(address);
      }
    }
    if (!mounted) return;
    _connectionFailures.add(
      PodConnectionFailure(
        address: address,
        generation: generation,
        error: error,
        stackTrace: stackTrace,
      ),
    );
  }

  void _publishLifecycleFailure(String address, int generation, String action) {
    if (!mounted) return;
    _lifecycleFailures.add(
      PodConnectionFailure(
        address: address,
        generation: generation,
        error: PodLifecycleFailure(address: address, action: action),
        stackTrace: StackTrace.current,
      ),
    );
  }

  static Future<void> _cleanupFailedConnection(
    StreamSubscription<AppTouchEvent>? subscription,
    Transport transport,
  ) async {
    await _cancelSubscription(subscription);
    try {
      await transport.disconnect();
    } catch (_) {
      // The connection is already unusable; cleanup is best effort.
    }
  }

  static Future<void> _cancelSubscription(
    StreamSubscription<AppTouchEvent>? subscription,
  ) async {
    try {
      await subscription?.cancel();
    } catch (_) {
      // Subscription cleanup is best effort.
    }
  }

  static Future<({Transport transport, PodRepository repository})>
  _connectBlePod(PodDevice pod) async {
    final device = pod.bleDevice;
    if (device == null) {
      throw StateError('Pod ${pod.address} has no BLE device handle');
    }
    final transport = await BleTransport.connect(device);
    return (transport: transport, repository: PodRepositoryImpl(transport));
  }

  /// Disconnect a specific pod.
  Future<void> disconnectPod(String address) async {
    final generation = _connectionGenerations[address];
    final entry = state[address];
    _connectionGenerations[address] =
        (_connectionGenerations[address] ?? 0) + 1;
    if (entry == null) return;
    if (generation != null && entry.isConnected) {
      _publishLifecycleFailure(address, generation, 'disconnectPod');
    }

    await _cancelSubscription(_touchSubscriptions.remove(address));
    try {
      await entry.transport?.disconnect();
    } catch (_) {
      // Always publish a deterministic disconnected state.
    }
    if (!mounted) return;
    state = {
      ...state,
      address: PodConnectionEntry(
        device: entry.device.copyWith(
          connectionState: PodConnectionState.disconnected,
        ),
      ),
    };
  }

  /// Disconnect all pods.
  Future<void> disconnectAll() async {
    final connectedGenerations = <String, int>{};
    for (final entry in state.entries) {
      final generation = _connectionGenerations[entry.key];
      if (entry.value.isConnected && generation != null) {
        connectedGenerations[entry.key] = generation;
      }
    }
    for (final address in state.keys) {
      _connectionGenerations[address] =
          (_connectionGenerations[address] ?? 0) + 1;
    }
    for (final entry in connectedGenerations.entries) {
      _publishLifecycleFailure(entry.key, entry.value, 'disconnectAll');
    }
    for (final subscription in _touchSubscriptions.values) {
      await _cancelSubscription(subscription);
    }
    _touchSubscriptions.clear();
    for (final entry in state.values) {
      try {
        await entry.transport?.disconnect();
      } catch (_) {
        // Continue disconnecting the remaining pods.
      }
    }
    if (!mounted) return;
    state = {};
  }

  /// Get connected pod addresses.
  List<String> get connectedAddresses => state.entries
      .where((e) => e.value.isConnected)
      .map((e) => e.key)
      .toList();

  /// Send SET_LED_PATTERN to a specific pod.
  Future<void> setLedPattern(String address, AppLedPattern pattern) async {
    final entry = state[address];
    if (entry?.repository == null || !entry!.isConnected) {
      throw StateError('Pod $address is not connected');
    }
    final generation = _connectionGenerations[address]!;
    try {
      final applied = await entry.repository!.setLedPattern(pattern);
      if (!pattern.matchesApplied(applied)) {
        throw StateError(
          'Pod $address reported a different applied LED pattern',
        );
      }
    } catch (error, stackTrace) {
      await _quarantineConnection(
        address,
        generation,
        entry,
        error,
        stackTrace,
      );
      Error.throwWithStackTrace(error, stackTrace);
    }
  }

  /// Send SET_MODE to a specific pod.
  Future<void> setMode(String address, SystemMode mode) async {
    final entry = state[address];
    if (entry?.repository == null || !entry!.isConnected) {
      throw StateError('Pod $address is not connected');
    }
    final generation = _connectionGenerations[address]!;
    try {
      final (reportedMode, transitionOk) = await entry.repository!
          .setSystemMode(mode);
      if (!transitionOk || reportedMode != mode) {
        throw StateError(
          'Pod $address rejected mode transition to ${mode.name} '
          '(reported ${reportedMode.name}, transitionOk=$transitionOk)',
        );
      }
    } catch (error, stackTrace) {
      await _quarantineConnection(
        address,
        generation,
        entry,
        error,
        stackTrace,
      );
      Error.throwWithStackTrace(error, stackTrace);
    }
  }

  @override
  void dispose() {
    for (final address in state.keys) {
      _connectionGenerations[address] =
          (_connectionGenerations[address] ?? 0) + 1;
    }
    for (final subscription in _touchSubscriptions.values) {
      unawaited(subscription.cancel());
    }
    _touchSubscriptions.clear();
    for (final entry in state.values) {
      final transport = entry.transport;
      if (transport != null) {
        unawaited(transport.disconnect());
      }
    }
    unawaited(_touchEvents.close());
    unawaited(_connectionFailures.close());
    unawaited(_lifecycleFailures.close());
    super.dispose();
  }
}

final multiPodProvider =
    StateNotifierProvider<MultiPodNotifier, Map<String, PodConnectionEntry>>((
      ref,
    ) {
      return MultiPodNotifier();
    });
