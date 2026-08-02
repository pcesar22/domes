/// BLE transport for DOMES device communication.
///
/// Port of tools/domes-cli/src/transport/ble.rs
/// Uses flutter_blue_plus for BLE Central role.
library;

import 'dart:async';
import 'dart:typed_data';

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_frame_channel.dart';
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
  final StreamController<Frame> _unsolicitedFrames =
      StreamController<Frame>.broadcast();
  late final BleFrameChannel _frameChannel;
  bool _connected = false;

  BleTransport._(this._device) {
    _frameChannel = BleFrameChannel(
      writeChunk: _writeChunk,
      maximumWriteSize: _maximumWriteSize,
      unsolicitedMessageTypes: {MsgType.MSG_TYPE_TOUCH_EVENT_NTF.value},
      onUnsolicitedFrame: _unsolicitedFrames.add,
    );
  }

  /// Connect to a DOMES device.
  static Future<BleTransport> connect(BluetoothDevice device) async {
    final transport = BleTransport._(device);
    try {
      await transport._connect();
      return transport;
    } catch (_) {
      await transport._cleanupConnection(closeEvents: true);
      rethrow;
    }
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

    // Install the listener before enabling notifications so no value can be
    // emitted between the CCCD write and stream subscription.
    _notificationSub = _statusChar!.onValueReceived.listen((data) {
      _frameChannel.addNotification(data);
    }, onError: _frameChannel.addError);
    await _statusChar!.setNotifyValue(true);

    // Listen for disconnection
    _connectionStateSub = _device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) {
        _connected = false;
        final error = StateError('BLE disconnected');
        _frameChannel.reset(error);
        if (!_unsolicitedFrames.isClosed) {
          _unsolicitedFrames.addError(error, StackTrace.current);
        }
      }
    });
  }

  Future<void> _writeChunk(Uint8List chunk) async {
    await _dataChar!.write(chunk.toList(), withoutResponse: true);
  }

  int _maximumWriteSize() {
    final mtu = _device.mtuNow;
    if (mtu <= 3) {
      throw StateError('BLE negotiated an invalid MTU: $mtu');
    }
    return mtu - 3;
  }

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {
    if (!_connected) throw Exception('BLE not connected');
    await _frameChannel.sendFrame(msgType, payload);
  }

  @override
  Future<Frame> receiveFrame(Duration timeout) async {
    return _frameChannel.receiveFrame(timeout);
  }

  @override
  Future<Frame> transactFrame(
    int msgType,
    Uint8List payload,
    Duration timeout, {
    void Function()? onFrameSent,
  }) async {
    if (!_connected) throw Exception('BLE not connected');
    return _frameChannel.transactFrame(
      msgType,
      payload,
      timeout,
      onFrameSent: onFrameSent,
    );
  }

  @override
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload, {
    required int expectedResponseType,
  }) async {
    if (!_connected) throw Exception('BLE not connected');
    return _frameChannel.sendCommand(
      msgType,
      payload,
      kDefaultTimeout,
      expectedResponseType,
    );
  }

  @override
  Stream<Frame> get unsolicitedFrames => _unsolicitedFrames.stream;

  @override
  int get maxOtaChunkSize => kOtaChunkSizeBle;

  @override
  Future<void> disconnect() async {
    await _cleanupConnection(closeEvents: true);
  }

  Future<void> _cleanupConnection({required bool closeEvents}) async {
    await _notificationSub?.cancel();
    _notificationSub = null;
    await _connectionStateSub?.cancel();
    _connectionStateSub = null;
    try {
      await _device.disconnect();
    } catch (_) {
      // Continue local teardown even when the platform link is already gone.
    }
    _connected = false;
    _frameChannel.reset(StateError('BLE disconnected'));
    if (closeEvents && !_unsolicitedFrames.isClosed) {
      await _unsolicitedFrames.close();
    }
  }

  @override
  bool get isConnected => _connected;

  /// The underlying BluetoothDevice.
  BluetoothDevice get device => _device;

  /// Device name.
  String get deviceName =>
      _device.platformName.isNotEmpty ? _device.platformName : 'Unknown';
}
