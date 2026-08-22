import 'dart:async';

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';

final class ControllablePodRepository implements PodRepository {
  final events = StreamController<AppTouchEvent>.broadcast();
  final listFeaturesResult = Completer<List<AppFeatureState>>();
  final setFeatureResult = Completer<AppFeatureState>();
  final getLedResult = Completer<AppLedPattern>();
  final setLedResult = Completer<AppLedPattern>();
  final getModeResult = Completer<AppModeInfo>();
  final setModeResult = Completer<(SystemMode, bool)>();
  final getInfoResult = Completer<AppSystemInfo>();
  final getVolumeResult = Completer<int>();
  final setVolumeResult = Completer<int>();
  final feedbackResult = Completer<bool>();

  @override
  Stream<AppTouchEvent> get touchEvents => events.stream;

  @override
  Future<List<AppFeatureState>> listFeatures() => listFeaturesResult.future;

  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      setFeatureResult.future;

  @override
  Future<AppLedPattern> getLedPattern() => getLedResult.future;

  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) =>
      setLedResult.future;

  @override
  Future<AppModeInfo> getSystemMode() => getModeResult.future;

  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) =>
      setModeResult.future;

  @override
  Future<AppSystemInfo> getSystemInfo() => getInfoResult.future;

  @override
  Future<int> getAudioVolume() => getVolumeResult.future;

  @override
  Future<int> setAudioVolume(int volume) => setVolumeResult.future;

  @override
  Future<bool> triggerFeedback(FeedbackProbe probe) => feedbackResult.future;
}

const testSystemInfo = AppSystemInfo(
  firmwareVersion: 'test',
  uptimeS: 1,
  freeHeap: 2,
  bootCount: 3,
  mode: SystemMode.SYSTEM_MODE_IDLE,
  featureMask: 0,
);

const testMode = AppModeInfo(
  mode: SystemMode.SYSTEM_MODE_IDLE,
  timeInModeMs: 1,
);
