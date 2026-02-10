/// OTA firmware update provider.
library;

import 'dart:typed_data';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/ota_protocol.dart' as ota;
import 'pod_connection_provider.dart';

/// OTA transfer phase.
enum OtaPhase {
  idle,
  preparing,
  transferring,
  verifying,
  completed,
  error,
}

/// OTA update state.
class OtaUpdateState {
  final OtaPhase phase;
  final int bytesSent;
  final int totalBytes;
  final String message;
  final String? error;

  const OtaUpdateState({
    this.phase = OtaPhase.idle,
    this.bytesSent = 0,
    this.totalBytes = 0,
    this.message = '',
    this.error,
  });

  double get progress => totalBytes > 0 ? bytesSent / totalBytes : 0;
}

/// OTA update notifier.
class OtaNotifier extends StateNotifier<OtaUpdateState> {
  final Ref _ref;

  OtaNotifier(this._ref) : super(const OtaUpdateState());

  /// Start OTA update with firmware bytes.
  Future<void> startOta(Uint8List firmware,
      {String version = 'unknown'}) async {
    final connection = _ref.read(podConnectionProvider);
    if (!connection.isConnected || connection.transport == null) {
      state = const OtaUpdateState(
        phase: OtaPhase.error,
        error: 'Not connected to a pod',
      );
      return;
    }

    state = OtaUpdateState(
      phase: OtaPhase.preparing,
      totalBytes: firmware.length,
      message: 'Preparing OTA update...',
    );

    try {
      await ota.otaFlash(
        connection.transport!,
        firmware,
        version: version,
        onProgress: (otaState, bytesSent, totalBytes, message) {
          state = OtaUpdateState(
            phase: _mapOtaState(otaState),
            bytesSent: bytesSent,
            totalBytes: totalBytes,
            message: message,
          );
        },
      );

      state = OtaUpdateState(
        phase: OtaPhase.completed,
        bytesSent: firmware.length,
        totalBytes: firmware.length,
        message: 'OTA complete! Device will reboot.',
      );
    } catch (e) {
      state = OtaUpdateState(
        phase: OtaPhase.error,
        error: '$e',
        message: 'OTA failed',
      );
    }
  }

  void reset() {
    state = const OtaUpdateState();
  }

  OtaPhase _mapOtaState(ota.OtaState otaState) {
    return switch (otaState) {
      ota.OtaState.idle => OtaPhase.idle,
      ota.OtaState.preparing => OtaPhase.preparing,
      ota.OtaState.transferring => OtaPhase.transferring,
      ota.OtaState.verifying => OtaPhase.verifying,
      ota.OtaState.completed => OtaPhase.completed,
      ota.OtaState.error => OtaPhase.error,
    };
  }
}

final otaProvider =
    StateNotifierProvider<OtaNotifier, OtaUpdateState>((ref) {
  return OtaNotifier(ref);
});
