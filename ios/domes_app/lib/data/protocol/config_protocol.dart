/// Protocol helpers for config commands.
///
/// Port of tools/domes-cli/src/protocol/mod.rs
///
/// IMPORTANT: LIST_FEATURES_RSP (0x21) payload is pure protobuf.
/// All other responses have [status_byte][protobuf_body] format.
library;

import 'dart:typed_data';
import 'package:domes_app/data/proto/generated/config.pb.dart';

/// Protocol error types.
sealed class ProtocolError {
  const ProtocolError();
}

class UnknownMessageType extends ProtocolError {
  final int type;
  const UnknownMessageType(this.type);

  @override
  String toString() =>
      'UnknownMessageType(0x${type.toRadixString(16).padLeft(2, '0')})';
}

class UnknownFeatureId extends ProtocolError {
  final int id;
  const UnknownFeatureId(this.id);

  @override
  String toString() => 'UnknownFeatureId($id)';
}

class PayloadTooShort extends ProtocolError {
  final int expected;
  final int actual;
  const PayloadTooShort({required this.expected, required this.actual});

  @override
  String toString() => 'PayloadTooShort(expected $expected, got $actual)';
}

class DeviceError extends ProtocolError {
  final Status status;
  const DeviceError(this.status);

  @override
  String toString() => 'DeviceError($status)';
}

class DecodeFailure extends ProtocolError {
  final String message;
  const DecodeFailure(this.message);

  @override
  String toString() => 'DecodeFailure($message)';
}

/// Feature state for app use.
class AppFeatureState {
  final Feature feature;
  final bool enabled;

  const AppFeatureState({required this.feature, required this.enabled});
}

/// LED pattern for app use.
class AppLedPattern {
  final LedPatternType patternType;
  final (int, int, int, int)? color; // RGBW
  final List<(int, int, int, int)> colors;
  final int periodMs;
  final int brightness;

  const AppLedPattern({
    this.patternType = LedPatternType.LED_PATTERN_OFF,
    this.color,
    this.colors = const [],
    this.periodMs = 2000,
    this.brightness = 128,
  });

  /// Create a solid color pattern.
  factory AppLedPattern.solid(int r, int g, int b) => AppLedPattern(
        patternType: LedPatternType.LED_PATTERN_SOLID,
        color: (r, g, b, 0),
      );

  /// Create a breathing pattern.
  factory AppLedPattern.breathing(int r, int g, int b, int periodMs) =>
      AppLedPattern(
        patternType: LedPatternType.LED_PATTERN_BREATHING,
        color: (r, g, b, 0),
        periodMs: periodMs,
      );

  /// Create a color cycle pattern.
  factory AppLedPattern.colorCycle(
          List<(int, int, int, int)> colors, int periodMs) =>
      AppLedPattern(
        patternType: LedPatternType.LED_PATTERN_COLOR_CYCLE,
        colors: colors,
        periodMs: periodMs,
      );

  /// Turn LEDs off.
  factory AppLedPattern.off() => const AppLedPattern();
}

/// Mode info for app use.
class AppModeInfo {
  final SystemMode mode;
  final int timeInModeMs;

  const AppModeInfo({required this.mode, required this.timeInModeMs});
}

/// System info for app use.
class AppSystemInfo {
  final String firmwareVersion;
  final int uptimeS;
  final int freeHeap;
  final int bootCount;
  final SystemMode mode;
  final int featureMask;

  const AppSystemInfo({
    required this.firmwareVersion,
    required this.uptimeS,
    required this.freeHeap,
    required this.bootCount,
    required this.mode,
    required this.featureMask,
  });
}

// --- Status byte checking ---

/// Check status byte and return protobuf portion of payload.
/// Throws [ProtocolError] on failure.
Uint8List _checkStatus(Uint8List payload) {
  if (payload.isEmpty) {
    throw const PayloadTooShort(expected: 1, actual: 0);
  }
  final statusVal = payload[0];
  final status = Status.valueOf(statusVal);
  if (status == null) {
    throw DeviceError(Status.STATUS_ERROR);
  }
  if (status != Status.STATUS_OK) {
    throw DeviceError(status);
  }
  return Uint8List.sublistView(payload, 1);
}

// --- Serialization ---

/// Serialize a SetFeatureRequest.
Uint8List serializeSetFeature(Feature feature, bool enabled) {
  final req = SetFeatureRequest()
    ..feature = feature
    ..enabled = enabled;
  return Uint8List.fromList(req.writeToBuffer());
}

/// Serialize a SetLedPatternRequest.
Uint8List serializeSetLedPattern(AppLedPattern pattern) {
  final proto = LedPattern()
    ..type = pattern.patternType
    ..periodMs = pattern.periodMs
    ..brightness = pattern.brightness;

  if (pattern.color != null) {
    final (r, g, b, w) = pattern.color!;
    proto.color = Color()
      ..r = r
      ..g = g
      ..b = b
      ..w = w;
  }

  for (final (r, g, b, w) in pattern.colors) {
    proto.colors.add(Color()
      ..r = r
      ..g = g
      ..b = b
      ..w = w);
  }

  final req = SetLedPatternRequest()..pattern = proto;
  return Uint8List.fromList(req.writeToBuffer());
}

/// Serialize a SetModeRequest.
Uint8List serializeSetMode(SystemMode mode) {
  final req = SetModeRequest()..mode = mode;
  return Uint8List.fromList(req.writeToBuffer());
}

// --- Parsing ---

/// Parse ListFeaturesResponse payload (pure protobuf, NO status byte).
List<AppFeatureState> parseListFeaturesResponse(Uint8List payload) {
  try {
    final resp = ListFeaturesResponse.fromBuffer(payload);
    return resp.features.map((fs) {
      return AppFeatureState(feature: fs.feature, enabled: fs.enabled);
    }).toList();
  } catch (e) {
    throw DecodeFailure('Failed to decode ListFeaturesResponse: $e');
  }
}

/// Parse SetFeatureResponse or GetFeatureResponse payload.
/// Format: [status_byte][protobuf_SetFeatureResponse]
AppFeatureState parseFeatureResponse(Uint8List payload) {
  final protoBytes = _checkStatus(payload);
  try {
    final resp = SetFeatureResponse.fromBuffer(protoBytes);
    final fs = resp.feature;
    return AppFeatureState(feature: fs.feature, enabled: fs.enabled);
  } catch (e) {
    throw DecodeFailure('Failed to decode SetFeatureResponse: $e');
  }
}

/// Parse SetLedPatternResponse or GetLedPatternResponse payload.
/// Format: [status_byte][protobuf_response]
AppLedPattern parseLedPatternResponse(Uint8List payload) {
  final protoBytes = _checkStatus(payload);
  LedPattern? pattern;
  try {
    final resp = SetLedPatternResponse.fromBuffer(protoBytes);
    pattern = resp.pattern;
  } catch (_) {
    try {
      final resp = GetLedPatternResponse.fromBuffer(protoBytes);
      pattern = resp.pattern;
    } catch (e) {
      throw DecodeFailure('Failed to decode LED pattern response: $e');
    }
  }

  return AppLedPattern(
    patternType: pattern.type,
    color: pattern.hasColor()
        ? (
            pattern.color.r,
            pattern.color.g,
            pattern.color.b,
            pattern.color.w,
          )
        : null,
    colors: pattern.colors
        .map((c) => (c.r, c.g, c.b, c.w))
        .toList(),
    periodMs: pattern.periodMs,
    brightness: pattern.brightness,
  );
}

/// Parse GetModeResponse payload.
/// Format: [status_byte][protobuf_GetModeResponse]
AppModeInfo parseGetModeResponse(Uint8List payload) {
  final protoBytes = _checkStatus(payload);
  try {
    final resp = GetModeResponse.fromBuffer(protoBytes);
    return AppModeInfo(
      mode: resp.mode,
      timeInModeMs: resp.timeInModeMs,
    );
  } catch (e) {
    throw DecodeFailure('Failed to decode GetModeResponse: $e');
  }
}

/// Parse SetModeResponse payload.
/// Format: [status_byte][protobuf_SetModeResponse]
(SystemMode, bool) parseSetModeResponse(Uint8List payload) {
  final protoBytes = _checkStatus(payload);
  try {
    final resp = SetModeResponse.fromBuffer(protoBytes);
    return (resp.mode, resp.transitionOk);
  } catch (e) {
    throw DecodeFailure('Failed to decode SetModeResponse: $e');
  }
}

/// Parse GetSystemInfoResponse payload.
/// Format: [status_byte][protobuf_GetSystemInfoResponse]
AppSystemInfo parseGetSystemInfoResponse(Uint8List payload) {
  final protoBytes = _checkStatus(payload);
  try {
    final resp = GetSystemInfoResponse.fromBuffer(protoBytes);
    return AppSystemInfo(
      firmwareVersion: resp.firmwareVersion,
      uptimeS: resp.uptimeS,
      freeHeap: resp.freeHeap,
      bootCount: resp.bootCount,
      mode: resp.mode,
      featureMask: resp.featureMask,
    );
  } catch (e) {
    throw DecodeFailure('Failed to decode GetSystemInfoResponse: $e');
  }
}
