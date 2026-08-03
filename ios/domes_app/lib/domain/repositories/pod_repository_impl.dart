/// BLE implementation of the PodRepository.
library;

import 'dart:typed_data';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';

import 'pod_repository.dart';

class PodRepositoryImpl implements PodRepository {
  final Transport _transport;

  PodRepositoryImpl(this._transport);

  @override
  Stream<AppTouchEvent> get touchEvents => _transport.unsolicitedFrames
      .where((frame) => frame.msgType == MsgType.MSG_TYPE_TOUCH_EVENT_NTF.value)
      .map((frame) => parseTouchEventNotification(frame.payload));

  @override
  Future<List<AppFeatureState>> listFeatures() async {
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_LIST_FEATURES_REQ,
      MsgType.MSG_TYPE_LIST_FEATURES_RSP,
      Uint8List(0),
    );
    return parseListFeaturesResponse(frame.payload);
  }

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) async {
    final payload = serializeSetFeature(feature, enabled);
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_SET_FEATURE_REQ,
      MsgType.MSG_TYPE_SET_FEATURE_RSP,
      payload,
    );
    return parseFeatureResponse(frame.payload);
  }

  @override
  Future<AppLedPattern> getLedPattern() async {
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_GET_LED_PATTERN_REQ,
      MsgType.MSG_TYPE_GET_LED_PATTERN_RSP,
      Uint8List(0),
    );
    return parseLedPatternResponse(frame.payload);
  }

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    final payload = serializeSetLedPattern(pattern);
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_SET_LED_PATTERN_REQ,
      MsgType.MSG_TYPE_SET_LED_PATTERN_RSP,
      payload,
    );
    return parseLedPatternResponse(frame.payload);
  }

  @override
  Future<AppModeInfo> getSystemMode() async {
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_GET_MODE_REQ,
      MsgType.MSG_TYPE_GET_MODE_RSP,
      Uint8List(0),
    );
    return parseGetModeResponse(frame.payload);
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    final payload = serializeSetMode(mode);
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_SET_MODE_REQ,
      MsgType.MSG_TYPE_SET_MODE_RSP,
      payload,
    );
    return parseSetModeResponse(frame.payload);
  }

  @override
  Future<AppSystemInfo> getSystemInfo() async {
    final frame = await _sendCommand(
      MsgType.MSG_TYPE_GET_SYSTEM_INFO_REQ,
      MsgType.MSG_TYPE_GET_SYSTEM_INFO_RSP,
      Uint8List(0),
    );
    return parseGetSystemInfoResponse(frame.payload);
  }

  Future<Frame> _sendCommand(
    MsgType requestType,
    MsgType responseType,
    Uint8List payload,
  ) async {
    final frame = await _transport.sendCommand(
      requestType.value,
      payload,
      expectedResponseType: responseType.value,
    );
    if (frame.msgType != responseType.value) {
      throw StateError(
        'Unexpected response type 0x${frame.msgType.toRadixString(16).padLeft(2, '0')}; '
        'expected 0x${responseType.value.toRadixString(16).padLeft(2, '0')}',
      );
    }
    return frame;
  }
}
