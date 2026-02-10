/// Model for a discovered or connected DOMES pod.
library;

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

enum PodConnectionState {
  disconnected,
  connecting,
  connected,
  disconnecting,
}

class PodDevice {
  final String name;
  final String address;
  final int rssi;
  final PodConnectionState connectionState;
  final BluetoothDevice? bleDevice;

  const PodDevice({
    required this.name,
    required this.address,
    this.rssi = 0,
    this.connectionState = PodConnectionState.disconnected,
    this.bleDevice,
  });

  PodDevice copyWith({
    String? name,
    String? address,
    int? rssi,
    PodConnectionState? connectionState,
    BluetoothDevice? bleDevice,
  }) =>
      PodDevice(
        name: name ?? this.name,
        address: address ?? this.address,
        rssi: rssi ?? this.rssi,
        connectionState: connectionState ?? this.connectionState,
        bleDevice: bleDevice ?? this.bleDevice,
      );

  bool get isConnected => connectionState == PodConnectionState.connected;

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is PodDevice &&
          runtimeType == other.runtimeType &&
          address == other.address;

  @override
  int get hashCode => address.hashCode;
}
