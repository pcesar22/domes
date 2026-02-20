/// Multi-pod connection provider.
///
/// Manages simultaneous BLE connections to multiple DOMES pods.
library;

import 'dart:typed_data';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/proto/generated/config.pbenum.dart';
import '../../data/protocol/config_protocol.dart';
import '../../data/transport/ble_transport.dart';
import '../../domain/models/pod_device.dart';
import '../../domain/repositories/pod_repository.dart';
import '../../domain/repositories/pod_repository_impl.dart';

/// State for a single pod connection in multi-pod mode.
class PodConnectionEntry {
  final PodDevice device;
  final BleTransport? transport;
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

/// Manages multiple pod connections.
class MultiPodNotifier extends StateNotifier<Map<String, PodConnectionEntry>> {
  MultiPodNotifier() : super({});

  /// Connect to a pod by address.
  Future<void> connectPod(PodDevice pod) async {
    if (pod.bleDevice == null) return;

    state = {
      ...state,
      pod.address: PodConnectionEntry(
        device: pod.copyWith(connectionState: PodConnectionState.connecting),
      ),
    };

    try {
      final transport = await BleTransport.connect(pod.bleDevice!);
      final repository = PodRepositoryImpl(transport);

      state = {
        ...state,
        pod.address: PodConnectionEntry(
          device: pod.copyWith(connectionState: PodConnectionState.connected),
          transport: transport,
          repository: repository,
        ),
      };
    } catch (e) {
      state = {
        ...state,
        pod.address: PodConnectionEntry(
          device: pod.copyWith(
              connectionState: PodConnectionState.disconnected),
          error: '$e',
        ),
      };
    }
  }

  /// Disconnect a specific pod.
  Future<void> disconnectPod(String address) async {
    final entry = state[address];
    if (entry == null) return;

    await entry.transport?.disconnect();
    state = {
      ...state,
      address: PodConnectionEntry(
        device: entry.device.copyWith(
            connectionState: PodConnectionState.disconnected),
      ),
    };
  }

  /// Disconnect all pods.
  Future<void> disconnectAll() async {
    for (final entry in state.values) {
      await entry.transport?.disconnect();
    }
    state = {};
  }

  /// Get connected pod addresses.
  List<String> get connectedAddresses => state.entries
      .where((e) => e.value.isConnected)
      .map((e) => e.key)
      .toList();

  /// Send a command to a specific pod.
  Future<void> sendToPod(
      String address, int msgType, Uint8List payload) async {
    final entry = state[address];
    if (entry?.transport == null) return;
    await entry!.transport!.sendFrame(msgType, payload);
  }

  /// Send SET_LED_PATTERN to a specific pod.
  Future<void> setLedPattern(String address, AppLedPattern pattern) async {
    final entry = state[address];
    if (entry?.transport == null) return;
    final payload = serializeSetLedPattern(pattern);
    await entry!.transport!.sendCommand(
        MsgType.MSG_TYPE_SET_LED_PATTERN_REQ.value, payload);
  }

  /// Send SET_MODE to a specific pod.
  Future<void> setMode(String address, SystemMode mode) async {
    final entry = state[address];
    if (entry?.transport == null) return;
    final payload = serializeSetMode(mode);
    await entry!.transport!.sendCommand(
        MsgType.MSG_TYPE_SET_MODE_REQ.value, payload);
  }

  @override
  void dispose() {
    for (final entry in state.values) {
      entry.transport?.disconnect();
    }
    super.dispose();
  }
}

final multiPodProvider =
    StateNotifierProvider<MultiPodNotifier, Map<String, PodConnectionEntry>>(
        (ref) {
  return MultiPodNotifier();
});
