/// BLE transport for DOMES device communication.
///
/// Port of tools/domes-cli/src/transport/ble.rs
/// Uses flutter_blue_plus for BLE Central role.
library;

import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'frame_codec.dart';
import 'transport.dart';

/// OTA Service UUID
const String kServiceUuid = '12345678-1234-5678-1234-56789abcdef0';

/// OTA Data Characteristic UUID (Write without response)
const String kDataCharUuid = '12345678-1234-5678-1234-56789abcdef1';

/// OTA Status Characteristic UUID (Notify)
const String kStatusCharUuid = '12345678-1234-5678-1234-56789abcdef2';

/// Default BLE operation timeout
const Duration kDefaultTimeout = Duration(seconds: 5);

/// BLE transport implementation.
class BleTransport extends Transport {
  final BluetoothDevice _device;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _statusChar;
  StreamSubscription<List<int>>? _notificationSub;
  StreamSubscription<BluetoothConnectionState>? _connectionStateSub;
  final FrameDecoder _decoder = FrameDecoder();
  StreamController<Frame> _frameController =
      StreamController<Frame>.broadcast();
  bool _connected = false;

  BleTransport._(this._device);

  /// Connect to a DOMES device.
  static Future<BleTransport> connect(BluetoothDevice device) async {
    final transport = BleTransport._(device);
    await transport._connect();
    return transport;
  }

  Future<void> _connect() async {
    await _device.connect(autoConnect: false);
    _connected = true;

    // Discover services
    final services = await _device.discoverServices();

    // Find the OTA service
    final otaService = services.firstWhere(
      (s) => s.uuid == Guid(kServiceUuid),
      orElse: () =>
          throw Exception('OTA service not found. Is this a DOMES device?'),
    );

    // Find characteristics
    _dataChar = otaService.characteristics.firstWhere(
      (c) => c.uuid == Guid(kDataCharUuid),
      orElse: () => throw Exception('Data characteristic not found'),
    );

    _statusChar = otaService.characteristics.firstWhere(
      (c) => c.uuid == Guid(kStatusCharUuid),
      orElse: () => throw Exception('Status characteristic not found'),
    );

    // Subscribe to notifications on status characteristic
    await _statusChar!.setNotifyValue(true);

    // Listen for notification data, feed into frame decoder
    _notificationSub = _statusChar!.onValueReceived.listen((data) {
      for (final byte in data) {
        final result = _decoder.feedByte(byte);
        switch (result) {
          case DecodeSuccess(:final frame):
            _frameController.add(frame);
            _decoder.reset();
          case DecodeError(:final error):
            _frameController.addError(error);
            _decoder.reset();
          case DecodeNone():
            break;
        }
      }
    });

    // Listen for disconnection
    _connectionStateSub = _device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) {
        _connected = false;
      }
    });
  }

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {
    if (!_connected) throw Exception('BLE not connected');
    final frame = encodeFrame(msgType, payload);
    await _dataChar!.write(frame.toList(), withoutResponse: true);
  }

  @override
  Future<Frame> receiveFrame(Duration timeout) async {
    return _frameController.stream.first.timeout(
      timeout,
      onTimeout: () => throw TimeoutException('BLE response timeout', timeout),
    );
  }

  @override
  Future<Frame> sendCommand(int msgType, Uint8List payload) async {
    await sendFrame(msgType, payload);
    return receiveFrame(kDefaultTimeout);
  }

  @override
  int get maxOtaChunkSize => kOtaChunkSizeBle;

  @override
  Future<void> disconnect() async {
    await _notificationSub?.cancel();
    _notificationSub = null;
    await _connectionStateSub?.cancel();
    _connectionStateSub = null;
    if (_connected) {
      await _device.disconnect();
    }
    _connected = false;
    _decoder.reset();
    // Close old controller and create a fresh one so this transport
    // can be reconnected without "stream already closed" errors.
    await _frameController.close();
    _frameController = StreamController<Frame>.broadcast();
  }

  @override
  bool get isConnected => _connected;

  /// The underlying BluetoothDevice.
  BluetoothDevice get device => _device;

  /// Device name.
  String get deviceName =>
      _device.platformName.isNotEmpty ? _device.platformName : 'Unknown';
}
