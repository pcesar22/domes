import 'dart:async';

import 'package:domes_app/application/providers/feedback_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_test/flutter_test.dart';

final class _FakeRepository implements PodRepository {
  final StreamController<AppTouchEvent> events = StreamController.broadcast();
  final List<int> setCalls = [];
  final List<Completer<int>> setResults = [];
  Completer<int>? getResult;

  @override
  Stream<AppTouchEvent> get touchEvents => events.stream;

  @override
  Future<int> getAudioVolume() => (getResult ??= Completer<int>()).future;

  @override
  Future<int> setAudioVolume(int volume) {
    setCalls.add(volume);
    final result = Completer<int>();
    setResults.add(result);
    return result.future;
  }

  @override
  Future<bool> triggerFeedback(FeedbackProbe probe) async => true;

  @override
  Future<AppLedPattern> getLedPattern() => throw UnsupportedError('not used');
  @override
  Future<AppModeInfo> getSystemMode() => throw UnsupportedError('not used');
  @override
  Future<AppSystemInfo> getSystemInfo() => throw UnsupportedError('not used');
  @override
  Future<List<AppFeatureState>> listFeatures() =>
      throw UnsupportedError('not used');
  @override
  Future<AppFeatureState> setFeature(Feature feature, bool enabled) =>
      throw UnsupportedError('not used');
  @override
  Future<AppLedPattern> setLedPattern(AppLedPattern pattern) =>
      throw UnsupportedError('not used');
  @override
  Future<(SystemMode, bool)> setSystemMode(SystemMode mode) =>
      throw UnsupportedError('not used');
}

void main() {
  test('serializes volume changes', () async {
    final repository = _FakeRepository();
    final notifier = FeedbackNotifier(initialRepository: repository);

    final first = notifier.setVolume(20);
    final second = notifier.setVolume(30);
    await Future<void>.delayed(Duration.zero);
    expect(repository.setCalls, [20]);
    repository.setResults.first.complete(20);
    await Future<void>.delayed(Duration.zero);
    expect(repository.setCalls, [20, 30]);
    repository.setResults.last.complete(30);
    await Future.wait([first, second]);
    expect(notifier.state.value, 30);
    notifier.dispose();
    await repository.events.close();
  });

  test('ignores a stale completion after repository replacement', () async {
    final oldRepository = _FakeRepository();
    final replacement = _FakeRepository();
    final notifier = FeedbackNotifier(initialRepository: oldRepository);

    final load = notifier.loadVolume();
    await Future<void>.delayed(Duration.zero);
    notifier.replaceRepository(replacement);
    oldRepository.getResult!.complete(91);
    await load;
    expect(notifier.state.isLoading, isTrue);

    notifier.dispose();
    await oldRepository.events.close();
    await replacement.events.close();
  });
}
