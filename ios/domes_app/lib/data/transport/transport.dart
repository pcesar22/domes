/// Abstract transport interface for DOMES device communication.
///
/// Port of tools/domes-cli/src/transport/mod.rs
library;

import 'dart:typed_data';
import 'frame_codec.dart';

/// Default OTA chunk size for serial/TCP (matches firmware kOtaChunkSize)
const int kOtaChunkSizeDefault = 1016;

/// BLE OTA chunk size - smaller to fit within BLE MTU limits
const int kOtaChunkSizeBle = 400;

/// Abstract transport for device communication.
abstract class Transport {
  /// Send a frame to the device.
  Future<void> sendFrame(int msgType, Uint8List payload);

  /// Receive a frame from the device with timeout.
  Future<Frame> receiveFrame(Duration timeout);

  /// Send a command and wait for response.
  Future<Frame> sendCommand(int msgType, Uint8List payload);

  /// Get the maximum OTA chunk size for this transport.
  int get maxOtaChunkSize => kOtaChunkSizeDefault;

  /// Disconnect from the device.
  Future<void> disconnect();

  /// Whether the transport is currently connected.
  bool get isConnected;
}
