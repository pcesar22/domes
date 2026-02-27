/// LED pattern provider.
library;

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/config_protocol.dart';
import 'pod_connection_provider.dart';

/// Notifier for LED pattern state.
class LedNotifier extends StateNotifier<AsyncValue<AppLedPattern>> {
  final Ref _ref;

  LedNotifier(this._ref) : super(const AsyncValue.loading());

  /// Load current LED pattern from the connected pod.
  Future<void> loadPattern() async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) {
      state = AsyncValue.error('Not connected', StackTrace.current);
      return;
    }

    state = const AsyncValue.loading();
    try {
      final pattern = await repo.getLedPattern();
      state = AsyncValue.data(pattern);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }

  /// Set a new LED pattern.
  Future<void> setPattern(AppLedPattern pattern) async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) return;

    try {
      final result = await repo.setLedPattern(pattern);
      state = AsyncValue.data(result);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }
}

final ledProvider =
    StateNotifierProvider.autoDispose<LedNotifier, AsyncValue<AppLedPattern>>((ref) {
  return LedNotifier(ref);
});
