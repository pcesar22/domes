/// Deterministic, protobuf-backed transport for the app virtual pod model.
library;

import 'dart:async';
import 'dart:typed_data';

import 'package:fixnum/fixnum.dart';

import '../../domain/models/app_clock.dart';
import '../proto/generated/config.pb.dart';
import 'frame_codec.dart';
import 'transport.dart';

final class VirtualPodCommand {
  const VirtualPodCommand({
    required this.timestamp,
    required this.requestType,
    required this.responseType,
    required this.requestPayload,
    required this.responsePayload,
  });

  final DateTime timestamp;
  final int requestType;
  final int responseType;
  final List<int> requestPayload;
  final List<int> responsePayload;

  String get signature =>
      '${timestamp.toUtc().toIso8601String()}|$requestType|$responseType|'
      '${requestPayload.join(',')}|${responsePayload.join(',')}';
}

final class VirtualPodTransport implements Transport {
  VirtualPodTransport({
    required this.address,
    required this.podId,
    required AppClock clock,
  }) : _clock = clock,
       _modeChangedAt = clock.now();

  final String address;
  final int podId;
  final AppClock _clock;
  final StreamController<Frame> _notifications =
      StreamController<Frame>.broadcast(sync: true);
  final List<VirtualPodCommand> _commands = [];
  final Map<Feature, bool> _features = {
    for (final feature in Feature.values)
      if (feature != Feature.FEATURE_UNKNOWN) feature: true,
  };

  bool _connected = true;
  SystemMode _mode = SystemMode.SYSTEM_MODE_IDLE;
  DateTime _modeChangedAt;
  LedPattern _ledPattern = LedPattern(
    type: LedPatternType.LED_PATTERN_OFF,
    periodMs: 2000,
    brightness: 128,
  );
  int _audioVolume = 100;

  List<VirtualPodCommand> get commands => List.unmodifiable(_commands);

  @override
  bool get isConnected => _connected;

  @override
  Stream<Frame> get unsolicitedFrames => _notifications.stream;

  @override
  int get maxOtaChunkSize => kOtaChunkSizeDefault;

  void emitTouch({int padIndex = 0}) {
    _requireConnected();
    if (padIndex < 0 || padIndex > 3) {
      throw RangeError.range(padIndex, 0, 3, 'padIndex');
    }
    final timestampUs = _clock.now().microsecondsSinceEpoch;
    final notification = TouchEventNotification(
      podId: podId,
      padIndex: padIndex,
      timestampUs: Int64(timestampUs),
    );
    _notifications.add(
      Frame(
        msgType: MsgType.MSG_TYPE_TOUCH_EVENT_NTF.value,
        payload: Uint8List.fromList(notification.writeToBuffer()),
      ),
    );
  }

  @override
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload, {
    required int expectedResponseType,
  }) async {
    _requireConnected();
    final response = _handleCommand(msgType, payload);
    if (response.msgType != expectedResponseType) {
      throw StateError(
        'Virtual pod $address produced ${response.msgType}, '
        'expected $expectedResponseType',
      );
    }
    _commands.add(
      VirtualPodCommand(
        timestamp: _clock.now(),
        requestType: msgType,
        responseType: response.msgType,
        requestPayload: List<int>.unmodifiable(payload),
        responsePayload: List<int>.unmodifiable(response.payload),
      ),
    );
    return response;
  }

  Frame _handleCommand(int msgType, Uint8List payload) {
    if (msgType == MsgType.MSG_TYPE_LIST_FEATURES_REQ.value) {
      return _frame(
        MsgType.MSG_TYPE_LIST_FEATURES_RSP,
        ListFeaturesResponse(
          podId: podId,
          features: _features.entries.map(
            (entry) => FeatureState(feature: entry.key, enabled: entry.value),
          ),
        ),
        status: false,
      );
    }
    if (msgType == MsgType.MSG_TYPE_SET_FEATURE_REQ.value) {
      final request = SetFeatureRequest.fromBuffer(payload);
      _features[request.feature] = request.enabled;
      return _frame(
        MsgType.MSG_TYPE_SET_FEATURE_RSP,
        SetFeatureResponse(
          feature: FeatureState(
            feature: request.feature,
            enabled: request.enabled,
          ),
        ),
      );
    }
    if (msgType == MsgType.MSG_TYPE_SET_LED_PATTERN_REQ.value) {
      _ledPattern = SetLedPatternRequest.fromBuffer(payload).pattern.deepCopy();
      return _frame(
        MsgType.MSG_TYPE_SET_LED_PATTERN_RSP,
        SetLedPatternResponse(pattern: _ledPattern.deepCopy()),
      );
    }
    if (msgType == MsgType.MSG_TYPE_GET_LED_PATTERN_REQ.value) {
      return _frame(
        MsgType.MSG_TYPE_GET_LED_PATTERN_RSP,
        GetLedPatternResponse(pattern: _ledPattern.deepCopy()),
      );
    }
    if (msgType == MsgType.MSG_TYPE_SET_MODE_REQ.value) {
      _mode = SetModeRequest.fromBuffer(payload).mode;
      _modeChangedAt = _clock.now();
      return _frame(
        MsgType.MSG_TYPE_SET_MODE_RSP,
        SetModeResponse(mode: _mode, transitionOk: true),
      );
    }
    if (msgType == MsgType.MSG_TYPE_GET_MODE_REQ.value) {
      return _frame(
        MsgType.MSG_TYPE_GET_MODE_RSP,
        GetModeResponse(
          mode: _mode,
          timeInModeMs: _clock.now().difference(_modeChangedAt).inMilliseconds,
        ),
      );
    }
    if (msgType == MsgType.MSG_TYPE_GET_SYSTEM_INFO_REQ.value) {
      return _frame(
        MsgType.MSG_TYPE_GET_SYSTEM_INFO_RSP,
        GetSystemInfoResponse(
          firmwareVersion: 'app-virtual-model-v1',
          uptimeS: 0,
          freeHeap: 0,
          bootCount: 1,
          mode: _mode,
          featureMask: 0,
          podId: podId,
        ),
      );
    }
    if (msgType == MsgType.MSG_TYPE_GET_AUDIO_VOLUME_REQ.value) {
      return _frame(
        MsgType.MSG_TYPE_GET_AUDIO_VOLUME_RSP,
        GetAudioVolumeResponse(volume: _audioVolume),
      );
    }
    if (msgType == MsgType.MSG_TYPE_SET_AUDIO_VOLUME_REQ.value) {
      _audioVolume = SetAudioVolumeRequest.fromBuffer(payload).volume;
      return _frame(
        MsgType.MSG_TYPE_SET_AUDIO_VOLUME_RSP,
        SetAudioVolumeResponse(volume: _audioVolume),
      );
    }
    if (msgType == MsgType.MSG_TYPE_TRIGGER_FEEDBACK_REQ.value) {
      final request = TriggerFeedbackRequest.fromBuffer(payload);
      return _frame(
        MsgType.MSG_TYPE_TRIGGER_FEEDBACK_RSP,
        TriggerFeedbackResponse(probe: request.probe, accepted: true),
      );
    }
    throw UnsupportedError(
      'Message type 0x${msgType.toRadixString(16)} is outside the app '
      'virtual model',
    );
  }

  static Frame _frame(MsgType type, Object message, {bool status = true}) {
    final bytes = switch (message) {
      ListFeaturesResponse value => value.writeToBuffer(),
      SetFeatureResponse value => value.writeToBuffer(),
      SetLedPatternResponse value => value.writeToBuffer(),
      GetLedPatternResponse value => value.writeToBuffer(),
      SetModeResponse value => value.writeToBuffer(),
      GetModeResponse value => value.writeToBuffer(),
      GetSystemInfoResponse value => value.writeToBuffer(),
      GetAudioVolumeResponse value => value.writeToBuffer(),
      SetAudioVolumeResponse value => value.writeToBuffer(),
      TriggerFeedbackResponse value => value.writeToBuffer(),
      _ => throw ArgumentError.value(message, 'message'),
    };
    return Frame(
      msgType: type.value,
      payload: Uint8List.fromList([
        if (status) Status.STATUS_OK.value,
        ...bytes,
      ]),
    );
  }

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {
    _requireConnected();
    _handleCommand(msgType, payload);
  }

  @override
  Future<Frame> receiveFrame(Duration timeout) =>
      Future.error(UnsupportedError('Virtual commands are request/response'));

  @override
  Future<Frame> transactFrame(
    int msgType,
    Uint8List payload,
    Duration timeout, {
    void Function()? onFrameSent,
  }) async {
    _requireConnected();
    onFrameSent?.call();
    return _handleCommand(msgType, payload);
  }

  @override
  Future<void> disconnect() async {
    if (!_connected) return;
    _connected = false;
    await _notifications.close();
  }

  void _requireConnected() {
    if (!_connected) {
      throw StateError('Virtual pod $address is disconnected');
    }
  }
}
