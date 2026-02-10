/// OTA protocol for firmware updates.
///
/// Port of tools/domes-cli/src/commands/ota.rs
///
/// OTA messages are binary, NOT protobuf.
library;

import 'dart:typed_data';
import 'dart:math';

import 'package:crypto/crypto.dart';

import '../transport/transport.dart';
import 'msg_types.dart';

/// OTA status codes.
enum OtaStatus {
  ok(0, 'OK'),
  busy(1, 'Busy'),
  flashError(2, 'Flash error'),
  verifyFailed(3, 'Verification failed'),
  sizeMismatch(4, 'Size mismatch'),
  offsetMismatch(5, 'Offset mismatch'),
  versionError(6, 'Version error'),
  partitionError(7, 'Partition error'),
  aborted(8, 'Aborted');

  final int value;
  final String description;
  const OtaStatus(this.value, this.description);

  static OtaStatus fromByte(int byte) {
    return OtaStatus.values.firstWhere(
      (s) => s.value == byte,
      orElse: () => OtaStatus.aborted,
    );
  }
}

/// OTA transfer state.
enum OtaState {
  idle,
  preparing,
  transferring,
  verifying,
  completed,
  error,
}

/// OTA progress callback.
typedef OtaProgressCallback = void Function(
    OtaState state, int bytesSent, int totalBytes, String message);

const int _sha256Size = 32;
const int _versionMaxLen = 32;
const Duration _otaTimeout = Duration(seconds: 5);
const Duration _otaEndTimeout = Duration(seconds: 30);

/// Serialize OTA_BEGIN payload.
/// Format: [u32 firmwareSize][32 bytes sha256][32 bytes version]
Uint8List _serializeOtaBegin(
    int firmwareSize, Uint8List sha256Hash, String version) {
  final payload = ByteData(4 + _sha256Size + _versionMaxLen);

  // Firmware size (little-endian)
  payload.setUint32(0, firmwareSize, Endian.little);

  // SHA256
  final bytes = payload.buffer.asUint8List();
  bytes.setRange(4, 4 + _sha256Size, sha256Hash);

  // Version (null-terminated, padded to 32 bytes)
  final versionBytes = version.codeUnits;
  final copyLen = min(versionBytes.length, _versionMaxLen - 1);
  bytes.setRange(4 + _sha256Size, 4 + _sha256Size + copyLen,
      versionBytes.sublist(0, copyLen));

  return bytes;
}

/// Serialize OTA_DATA payload.
/// Format: [u32 offset][u16 length][data...]
Uint8List _serializeOtaData(int offset, Uint8List data) {
  final payload = Uint8List(4 + 2 + data.length);
  final bd = ByteData.sublistView(payload);

  bd.setUint32(0, offset, Endian.little);
  bd.setUint16(4, data.length, Endian.little);
  payload.setRange(6, 6 + data.length, data);

  return payload;
}

/// Deserialize OTA_ACK payload.
/// Format: [u8 status][u32 nextOffset]
(OtaStatus, int) _deserializeOtaAck(Uint8List payload) {
  if (payload.length < 5) {
    throw Exception(
        'OTA_ACK payload too short: ${payload.length} bytes, expected 5');
  }
  final status = OtaStatus.fromByte(payload[0]);
  final bd = ByteData.sublistView(payload);
  final nextOffset = bd.getUint32(1, Endian.little);
  return (status, nextOffset);
}

/// Send OTA firmware update to device.
Future<void> otaFlash(
  Transport transport,
  Uint8List firmware, {
  String version = 'unknown',
  OtaProgressCallback? onProgress,
}) async {
  onProgress?.call(
      OtaState.preparing, 0, firmware.length, 'Computing SHA256...');

  // Compute SHA256
  final digest = sha256.convert(firmware);
  final sha256Hash = Uint8List.fromList(digest.bytes);

  // Send OTA_BEGIN
  onProgress?.call(
      OtaState.preparing, 0, firmware.length, 'Sending OTA_BEGIN...');
  final beginPayload =
      _serializeOtaBegin(firmware.length, sha256Hash, version);

  final beginResp = await _sendAndWaitAck(
      transport, kOtaBegin, beginPayload, _otaTimeout);
  if (beginResp.$1 != OtaStatus.ok) {
    onProgress?.call(OtaState.error, 0, firmware.length,
        'Device rejected OTA_BEGIN: ${beginResp.$1.description}');
    throw Exception(
        'Device rejected OTA_BEGIN: ${beginResp.$1.description}');
  }

  // Send firmware chunks
  final chunkSize = transport.maxOtaChunkSize;
  var offset = 0;
  final total = firmware.length;

  while (offset < total) {
    final end = min(offset + chunkSize, total);
    final chunk = Uint8List.sublistView(firmware, offset, end);
    final dataPayload = _serializeOtaData(offset, chunk);

    onProgress?.call(OtaState.transferring, offset, total,
        'Sending chunk at offset $offset...');

    final dataResp = await _sendAndWaitAck(
        transport, kOtaData, dataPayload, _otaTimeout);
    if (dataResp.$1 != OtaStatus.ok) {
      throw Exception(
          'Device rejected chunk at offset $offset: ${dataResp.$1.description}');
    }

    offset = end;
  }

  // Send OTA_END
  onProgress?.call(
      OtaState.verifying, total, total, 'Sending OTA_END...');
  final endResp = await _sendAndWaitAck(
      transport, kOtaEnd, Uint8List(0), _otaEndTimeout);
  if (endResp.$1 != OtaStatus.ok) {
    throw Exception(
        'Device rejected OTA_END: ${endResp.$1.description}');
  }

  onProgress?.call(
      OtaState.completed, total, total, 'OTA complete! Device will reboot.');
}

/// Send a frame and wait for ACK.
Future<(OtaStatus, int)> _sendAndWaitAck(
  Transport transport,
  int msgType,
  Uint8List payload,
  Duration timeout,
) async {
  await transport.sendFrame(msgType, payload);
  final frame = await transport.receiveFrame(timeout);

  if (frame.msgType == kOtaAck) {
    return _deserializeOtaAck(frame.payload);
  } else if (frame.msgType == kOtaAbort) {
    if (frame.payload.isEmpty) {
      throw Exception('Device aborted OTA (no reason)');
    }
    final reason = OtaStatus.fromByte(frame.payload[0]);
    throw Exception('Device aborted OTA: ${reason.description}');
  } else {
    throw Exception(
        'Unexpected response type: 0x${frame.msgType.toRadixString(16).padLeft(2, '0')}');
  }
}
