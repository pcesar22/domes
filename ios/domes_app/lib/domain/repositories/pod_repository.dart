/// Abstract repository interface for pod communication.
library;

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';

abstract class PodRepository {
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
}
