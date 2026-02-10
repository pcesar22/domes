/// System info and mode providers.
library;

import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/protocol/config_protocol.dart';
import 'pod_connection_provider.dart';

/// Notifier for system info.
class SystemInfoNotifier extends StateNotifier<AsyncValue<AppSystemInfo>> {
  final Ref _ref;

  SystemInfoNotifier(this._ref) : super(const AsyncValue.loading());

  Future<void> loadSystemInfo() async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) {
      state = AsyncValue.error('Not connected', StackTrace.current);
      return;
    }

    state = const AsyncValue.loading();
    try {
      final info = await repo.getSystemInfo();
      state = AsyncValue.data(info);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }
}

final systemInfoProvider =
    StateNotifierProvider<SystemInfoNotifier, AsyncValue<AppSystemInfo>>((ref) {
  return SystemInfoNotifier(ref);
});

/// Notifier for system mode.
class SystemModeNotifier extends StateNotifier<AsyncValue<AppModeInfo>> {
  final Ref _ref;

  SystemModeNotifier(this._ref) : super(const AsyncValue.loading());

  Future<void> loadMode() async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) {
      state = AsyncValue.error('Not connected', StackTrace.current);
      return;
    }

    state = const AsyncValue.loading();
    try {
      final mode = await repo.getSystemMode();
      state = AsyncValue.data(mode);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }

  Future<void> setMode(SystemMode mode) async {
    final repo = _ref.read(podRepositoryProvider);
    if (repo == null) return;

    try {
      await repo.setSystemMode(mode);
      await loadMode();
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }
}

final systemModeProvider =
    StateNotifierProvider<SystemModeNotifier, AsyncValue<AppModeInfo>>((ref) {
  return SystemModeNotifier(ref);
});
