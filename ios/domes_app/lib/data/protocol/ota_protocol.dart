/// OTA protocol for firmware updates.
///
/// Port of tools/domes-cli/src/commands/ota.rs
///
/// OTA messages are binary, NOT protobuf.
library;

import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

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
    for (final status in OtaStatus.values) {
      if (status.value == byte) {
        return status;
      }
    }
    throw FormatException('Unknown OTA status code: $byte');
  }
}

/// OTA transfer state.
enum OtaState { idle, preparing, transferring, verifying, completed, error }

/// OTA progress callback.
typedef OtaProgressCallback =
    void Function(
      OtaState state,
      int bytesSent,
      int totalBytes,
      String message,
    );

const int _sha256Size = 32;
const int _versionMaxLen = 32;
const int _otaChunkMaxLen = 1016;
const int _uint32Max = 0xffffffff;
const Duration _otaTimeout = Duration(seconds: 5);
const Duration _otaEndTimeout = Duration(seconds: 30);

/// Serialize OTA_BEGIN payload.
/// Format: [u32 firmwareSize][32 bytes sha256][32 bytes version]
Uint8List _serializeOtaBegin(
  int firmwareSize,
  Uint8List sha256Hash,
  String version,
) {
  validateOtaVersion(version);
  final payload = ByteData(4 + _sha256Size + _versionMaxLen);

  // Firmware size (little-endian)
  payload.setUint32(0, firmwareSize, Endian.little);

  // SHA256
  final bytes = payload.buffer.asUint8List();
  bytes.setRange(4, 4 + _sha256Size, sha256Hash);

  // Version (null-terminated, padded to 32 bytes)
  final versionBytes = ascii.encode(version);
  bytes.setRange(
    4 + _sha256Size,
    4 + _sha256Size + versionBytes.length,
    versionBytes,
  );

  return bytes;
}

/// Serialize OTA_DATA payload.
/// Format: [u32 offset][u16 length][data...]
Uint8List _serializeOtaData(int offset, Uint8List data) {
  if (offset < 0 || offset > _uint32Max) {
    throw RangeError.range(offset, 0, _uint32Max, 'offset');
  }
  if (data.isEmpty || data.length > _otaChunkMaxLen) {
    throw ArgumentError.value(
      data.length,
      'data.length',
      'must be between 1 and $_otaChunkMaxLen bytes',
    );
  }
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
  if (payload.length != 5) {
    throw Exception(
      'OTA_ACK payload has ${payload.length} bytes, expected exactly 5',
    );
  }
  final status = OtaStatus.fromByte(payload[0]);
  final bd = ByteData.sublistView(payload);
  final nextOffset = bd.getUint32(1, Endian.little);
  return (status, nextOffset);
}

void _validateNextOffset(String operation, int actual, int expected) {
  if (actual != expected) {
    throw Exception(
      '$operation acknowledged next offset $actual, expected $expected',
    );
  }
}

Future<void> _sendOtaAbort(Transport transport) {
  return transport.sendFrame(
    kOtaAbort,
    Uint8List.fromList([OtaStatus.aborted.value]),
  );
}

Future<void> _abortOrDisconnect(Transport transport) async {
  try {
    await _sendOtaAbort(transport);
  } catch (_) {
    try {
      await transport.disconnect();
    } catch (_) {
      // Preserve the transfer failure; disconnect is final best-effort cleanup.
    }
  }
}

/// Return a user-facing validation error, or null for a parser-valid version.
String? otaVersionValidationError(String version) {
  if (version.isEmpty) return 'OTA version is required';
  if (version.length >= _versionMaxLen) {
    return 'OTA version must be at most ${_versionMaxLen - 1} ASCII characters';
  }
  if (version.codeUnits.any((unit) => unit == 0 || unit > 0x7f)) {
    return 'OTA version must contain only non-NUL ASCII characters';
  }

  final normalized = version.startsWith('v') || version.startsWith('V')
      ? version.substring(1)
      : version;
  final dashIndex = normalized.indexOf('-');
  final core = dashIndex < 0 ? normalized : normalized.substring(0, dashIndex);
  final suffix = dashIndex < 0 ? null : normalized.substring(dashIndex + 1);
  final components = core.split('.');
  if (components.length != 3 || components.any((part) => !_isUint32(part))) {
    return 'OTA version is not parser-valid: $version';
  }

  if (suffix == null || suffix == 'dirty') return null;
  final suffixParts = suffix.split('-');
  if ((suffixParts.length != 2 && suffixParts.length != 3) ||
      (suffixParts.length == 3 && suffixParts[2] != 'dirty') ||
      !_isUint32(suffixParts[0])) {
    return 'OTA version is not parser-valid: $version';
  }
  final hashPart = suffixParts[1];
  if (!hashPart.startsWith('g')) {
    return 'OTA version is not parser-valid: $version';
  }
  final hash = hashPart.substring(1);
  if (hash.isEmpty ||
      hash.length > 40 ||
      !RegExp(r'^[0-9a-fA-F]+$').hasMatch(hash)) {
    return 'OTA version is not parser-valid: $version';
  }
  return null;
}

void validateOtaVersion(String version) {
  final error = otaVersionValidationError(version);
  if (error != null) {
    throw ArgumentError.value(version, 'version', error);
  }
}

bool _isUint32(String value) {
  if (value.isEmpty || !RegExp(r'^\d+$').hasMatch(value)) return false;
  final parsed = int.tryParse(value);
  return parsed != null && parsed <= _uint32Max;
}

/// Send OTA firmware update to device.
Future<void> otaFlash(
  Transport transport,
  Uint8List firmware, {
  required String version,
  OtaProgressCallback? onProgress,
}) async {
  if (firmware.isEmpty) {
    throw ArgumentError.value(firmware, 'firmware', 'must not be empty');
  }
  if (firmware.length > _uint32Max) {
    throw ArgumentError.value(
      firmware.length,
      'firmware.length',
      'exceeds the OTA protocol 32-bit size limit',
    );
  }
  validateOtaVersion(version);
  onProgress?.call(
    OtaState.preparing,
    0,
    firmware.length,
    'Computing SHA256...',
  );

  // Compute SHA256
  final digest = sha256.convert(firmware);
  final sha256Hash = Uint8List.fromList(digest.bytes);

  // Send OTA_BEGIN
  onProgress?.call(
    OtaState.preparing,
    0,
    firmware.length,
    'Sending OTA_BEGIN...',
  );
  final beginPayload = _serializeOtaBegin(firmware.length, sha256Hash, version);

  late final (OtaStatus, int) beginResp;
  var beginMayHaveReachedDevice = false;
  try {
    // A fragmented write can fail after delivering only part of OTA_BEGIN.
    beginMayHaveReachedDevice = true;
    beginResp = await _sendAndWaitAck(
      transport,
      kOtaBegin,
      beginPayload,
      _otaTimeout,
    );
  } catch (error, stackTrace) {
    if (beginMayHaveReachedDevice) {
      await _abortOrDisconnect(transport);
    }
    onProgress?.call(OtaState.error, 0, firmware.length, 'OTA failed: $error');
    Error.throwWithStackTrace(error, stackTrace);
  }
  if (beginResp.$1 != OtaStatus.ok) {
    onProgress?.call(
      OtaState.error,
      0,
      firmware.length,
      'Device rejected OTA_BEGIN: ${beginResp.$1.description}',
    );
    throw Exception('Device rejected OTA_BEGIN: ${beginResp.$1.description}');
  }

  var offset = 0;
  final total = firmware.length;

  try {
    _validateNextOffset('OTA_BEGIN', beginResp.$2, 0);

    // Send firmware chunks using the transport-specific maximum.
    final chunkSize = transport.maxOtaChunkSize;
    if (chunkSize <= 0 || chunkSize > _otaChunkMaxLen) {
      throw StateError(
        'Transport reported invalid OTA chunk size $chunkSize; '
        'expected 1-$_otaChunkMaxLen',
      );
    }

    while (offset < total) {
      final expectedNext = min(offset + chunkSize, total);
      final chunk = Uint8List.sublistView(firmware, offset, expectedNext);
      final dataPayload = _serializeOtaData(offset, chunk);

      onProgress?.call(
        OtaState.transferring,
        offset,
        total,
        'Sending chunk at offset $offset...',
      );

      final dataResp = await _sendAndWaitAck(
        transport,
        kOtaData,
        dataPayload,
        _otaTimeout,
      );
      if (dataResp.$1 != OtaStatus.ok) {
        throw Exception(
          'Device rejected chunk at offset $offset: ${dataResp.$1.description}',
        );
      }
      _validateNextOffset('OTA_DATA', dataResp.$2, expectedNext);

      offset = dataResp.$2;
    }

    // Send OTA_END
    onProgress?.call(OtaState.verifying, total, total, 'Sending OTA_END...');
    final endResp = await _sendAndWaitAck(
      transport,
      kOtaEnd,
      Uint8List(0),
      _otaEndTimeout,
    );
    if (endResp.$1 != OtaStatus.ok) {
      throw Exception('Device rejected OTA_END: ${endResp.$1.description}');
    }
    _validateNextOffset('OTA_END', endResp.$2, total);
  } catch (error, stackTrace) {
    await _abortOrDisconnect(transport);
    onProgress?.call(OtaState.error, offset, total, 'OTA failed: $error');
    Error.throwWithStackTrace(error, stackTrace);
  }

  onProgress?.call(
    OtaState.completed,
    total,
    total,
    'OTA complete! Device will reboot.',
  );
}

/// Send a frame and wait for ACK.
Future<(OtaStatus, int)> _sendAndWaitAck(
  Transport transport,
  int msgType,
  Uint8List payload,
  Duration timeout, {
  void Function()? onFrameSent,
}) async {
  final frame = await transport.transactFrame(
    msgType,
    payload,
    timeout,
    onFrameSent: onFrameSent,
  );

  if (frame.msgType == kOtaAck) {
    return _deserializeOtaAck(frame.payload);
  } else if (frame.msgType == kOtaAbort) {
    if (frame.payload.length != 1) {
      throw Exception(
        'OTA_ABORT payload has ${frame.payload.length} bytes, expected exactly 1',
      );
    }
    final reason = OtaStatus.fromByte(frame.payload[0]);
    throw Exception('Device aborted OTA: ${reason.description}');
  } else {
    final responseType = frame.msgType
        .toRadixString(16)
        .padLeft(2, '0')
        .toUpperCase();
    throw Exception('Unexpected response type: 0x$responseType');
  }
}
