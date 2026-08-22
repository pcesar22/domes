/// Abstract repository interface for pod communication.
library;

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';

abstract class PodRepository {
  /// Physical touch edges reported by the connected pod.
  Stream<AppTouchEvent> get touchEvents;

  /// List all features and their states.
  Future<List<AppFeatureState>> listFeatures();

  /// Set a feature enabled/disabled.
  Future<AppFeatureState> setFeature(Feature feature, bool enabled);

  /// Get the current LED pattern.
  Future<AppLedPattern> getLedPattern();

  /// Set the LED pattern.
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern);

  /// Get the current system mode.
  Future<AppModeInfo> getSystemMode();

  /// Set the system mode.
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode);

  /// Get system info.
  Future<AppSystemInfo> getSystemInfo();

  /// Read the device-owned 0-100 audio software gain.
  Future<int> getAudioVolume();

  /// Persist and apply the device-owned 0-100 audio software gain.
  Future<int> setAudioVolume(int volume);

  /// Request one known probe. True means accepted, never physically observed.
  Future<bool> triggerFeedback(FeedbackProbe probe);
}
