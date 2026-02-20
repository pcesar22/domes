/// BLE scanner provider using Riverpod.
///
/// Scans for DOMES pods advertising the OTA service UUID or with "DOMES" in name.
library;

import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/transport/ble_transport.dart';
import '../../domain/models/pod_device.dart';

/// Provider for BLE adapter state.
final bleAdapterStateProvider = StreamProvider<BluetoothAdapterState>((ref) {
  return FlutterBluePlus.adapterState;
});

/// Provider for whether a scan is in progress.
final isScanningProvider = StreamProvider<bool>((ref) {
  return FlutterBluePlus.isScanning;
});

/// Notifier that manages BLE scanning and discovered devices.
class BleScannerNotifier extends StateNotifier<List<PodDevice>> {
  StreamSubscription<List<ScanResult>>? _scanSub;

  BleScannerNotifier() : super([]);

  /// Start scanning for DOMES pods.
  Future<void> startScan({Duration timeout = const Duration(seconds: 10)}) async {
    // Check adapter state before scanning
    final adapterState = await FlutterBluePlus.adapterState.first;
    if (adapterState != BluetoothAdapterState.on) {
      throw Exception('Bluetooth is not enabled (state: $adapterState)');
    }

    state = [];
    FlutterBluePlus.startScan(
      withServices: [Guid(kServiceUuid)],
      timeout: timeout,
    );

    _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      final pods = <PodDevice>[];
      for (final r in results) {
        final name = r.device.platformName;
        // Accept devices with DOMES in name or advertising our service
        if (name.contains('DOMES') || r.advertisementData.serviceUuids
            .any((u) => u == Guid(kServiceUuid))) {
          pods.add(PodDevice(
            name: name.isNotEmpty ? name : 'Unknown DOMES',
            address: r.device.remoteId.str,
            rssi: r.rssi,
            bleDevice: r.device,
          ));
        }
      }
      state = pods;
    });
  }

  /// Stop scanning.
  void stopScan() {
    FlutterBluePlus.stopScan();
    _scanSub?.cancel();
    _scanSub = null;
  }

  @override
  void dispose() {
    stopScan();
    super.dispose();
  }
}

/// Provider for the BLE scanner.
final bleScannerProvider =
    StateNotifierProvider.autoDispose<BleScannerNotifier, List<PodDevice>>((ref) {
  return BleScannerNotifier();
});
