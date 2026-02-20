/// BLE implementation of the PodRepository.
library;

import 'dart:typed_data';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/transport.dart';

import 'pod_repository.dart';

class PodRepositoryImpl implements PodRepository {
  final Transport _transport;

  PodRepositoryImpl(this._transport);

  @override
  Future<List<AppFeatureState>> listFeatures() async {
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_LIST_FEATURES_REQ.value, Uint8List(0));
    return parseListFeaturesResponse(frame.payload);
  }

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) async {
    final payload = serializeSetFeature(feature, enabled);
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_SET_FEATURE_REQ.value, payload);
    return parseFeatureResponse(frame.payload);
  }

  @override
  Future<AppLedPattern> getLedPattern() async {
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_GET_LED_PATTERN_REQ.value, Uint8List(0));
    return parseLedPatternResponse(frame.payload);
  }

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) async {
    final payload = serializeSetLedPattern(pattern);
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_SET_LED_PATTERN_REQ.value, payload);
    return parseLedPatternResponse(frame.payload);
  }

  @override
  Future<AppModeInfo> getSystemMode() async {
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_GET_MODE_REQ.value, Uint8List(0));
    return parseGetModeResponse(frame.payload);
  }

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) async {
    final payload = serializeSetMode(mode);
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_SET_MODE_REQ.value, payload);
    return parseSetModeResponse(frame.payload);
  }

  @override
  Future<AppSystemInfo> getSystemInfo() async {
    final frame = await _transport.sendCommand(
        MsgType.MSG_TYPE_GET_SYSTEM_INFO_REQ.value, Uint8List(0));
    return parseGetSystemInfoResponse(frame.payload);
  }
}
