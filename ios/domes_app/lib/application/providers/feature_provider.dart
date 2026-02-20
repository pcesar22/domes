/// Feature management provider.
library;

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/config_protocol.dart';
import 'pod_connection_provider.dart';

/// Notifier for feature state management.
class FeatureNotifier extends StateNotifier<AsyncValue<List<AppFeatureState>>> {
  final Ref _ref;

  FeatureNotifier(this._ref) : super(const AsyncValue.loading());

  /// Load all features from the connected pod.
  Future<void> loadFeatures() async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) {
      state = AsyncValue.error(
        'Not connected to a pod',
        StackTrace.current,
      );
      return;
    }

    state = const AsyncValue.loading();
    try {
      final features = await repo.listFeatures();
      state = AsyncValue.data(features);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }

  /// Toggle a feature.
  Future<void> toggleFeature(Feature feature, bool enabled) async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) return;

    try {
      await repo.setFeature(feature, enabled);
      await loadFeatures(); // Refresh full list
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }
}

final featureProvider =
    StateNotifierProvider.autoDispose<FeatureNotifier, AsyncValue<List<AppFeatureState>>>(
        (ref) {
  return FeatureNotifier(ref);
});
