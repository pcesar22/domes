/// Device-owned audio software-gain state with stale-operation protection.
library;

import 'dart:async';

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../domain/repositories/pod_repository.dart';
import 'pod_connection_provider.dart';

class FeedbackNotifier extends StateNotifier<AsyncValue<int>> {
  FeedbackNotifier({PodRepository? initialRepository})
    : _repository = initialRepository,
      super(const AsyncValue.loading());

  PodRepository? _repository;
  int _generation = 0;
  Future<void> _pending = Future<void>.value();

  void replaceRepository(PodRepository? next) {
    if (identical(next, _repository)) return;
    _repository = next;
    _generation++;
    state = next == null
        ? AsyncValue.error('Not connected', StackTrace.current)
        : const AsyncValue.loading();
  }

  Future<void> loadVolume() {
    final repository = _repository;
    final generation = _generation;
    state = const AsyncValue.loading();
    return _enqueue(() async {
      if (repository == null) {
        if (mounted && generation == _generation) {
          state = AsyncValue.error('Not connected', StackTrace.current);
        }
        return;
      }
      try {
        final volume = await repository.getAudioVolume();
        if (mounted &&
            generation == _generation &&
            identical(repository, _repository)) {
          state = AsyncValue.data(volume);
        }
      } catch (error, stack) {
        if (mounted &&
            generation == _generation &&
            identical(repository, _repository)) {
          state = AsyncValue.error(error, stack);
        }
      }
    });
  }

  Future<void> setVolume(int volume) {
    final repository = _repository;
    final generation = _generation;
    return _enqueue(() async {
      if (repository == null || generation != _generation) return;
      try {
        final applied = await repository.setAudioVolume(volume);
        if (mounted &&
            generation == _generation &&
            identical(repository, _repository)) {
          state = AsyncValue.data(applied);
        }
      } catch (error, stack) {
        if (mounted &&
            generation == _generation &&
            identical(repository, _repository)) {
          state = AsyncValue.error(error, stack);
        }
      }
    });
  }

  Future<bool> trigger(FeedbackProbe probe) async {
    final repository = _repository;
    final generation = _generation;
    if (repository == null) return false;
    final completer = Completer<bool>();
    await _enqueue(() async {
      if (generation != _generation || !identical(repository, _repository)) {
        completer.complete(false);
        return;
      }
      try {
        completer.complete(await repository.triggerFeedback(probe));
      } catch (error, stack) {
        if (mounted && generation == _generation) {
          state = AsyncValue.error(error, stack);
        }
        completer.completeError(error, stack);
      }
    });
    return completer.future;
  }

  Future<void> _enqueue(Future<void> Function() operation) {
    final next = _pending.then((_) => operation());
    _pending = next.catchError((_) {});
    return next;
  }
}

final feedbackProvider =
    StateNotifierProvider.autoDispose<FeedbackNotifier, AsyncValue<int>>((ref) {
      final notifier = FeedbackNotifier(
        initialRepository: ref.read(podRepositoryProvider),
      );
      ref.listen<PodRepository?>(podRepositoryProvider, (_, next) {
        notifier.replaceRepository(next);
      });
      return notifier;
    });
