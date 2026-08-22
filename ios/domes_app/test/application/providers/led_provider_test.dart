import 'package:domes_app/application/providers/led_provider.dart';
import 'package:domes_app/application/providers/pod_connection_provider.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import '../../domain/repositories/generation_test_fakes.dart';

final _testRepositoryProvider = StateProvider<PodRepository?>((ref) => null);

void main() {
  test(
    'LED publications require the repository generation that started them',
    () async {
      final old = ControllablePodRepository();
      final replacement = ControllablePodRepository();
      final container = ProviderContainer(
        overrides: [
          podRepositoryProvider.overrideWith(
            (ref) => ref.watch(_testRepositoryProvider),
          ),
        ],
      );
      container.read(_testRepositoryProvider.notifier).state = old;
      final subscription = container.listen(ledProvider, (_, _) {});
      final notifier = container.read(ledProvider.notifier);

      final stale = notifier.setPattern(AppLedPattern.solid(1, 2, 3));
      container.read(_testRepositoryProvider.notifier).state = replacement;
      old.setLedResult.complete(AppLedPattern.solid(1, 2, 3));
      await stale;
      expect(container.read(ledProvider).isLoading, isTrue);

      final oldError = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = oldError;
      final staleError = notifier.loadPattern();
      container.read(_testRepositoryProvider.notifier).state = replacement;
      oldError.getLedResult.completeError(StateError('stale LED failure'));
      await staleError;
      expect(container.read(ledProvider).isLoading, isTrue);

      final current = notifier.loadPattern();
      replacement.getLedResult.completeError(StateError('current LED failure'));
      await current;
      expect(container.read(ledProvider).error, isA<StateError>());

      final currentSuccess = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = currentSuccess;
      final loaded = notifier.loadPattern();
      final pattern = AppLedPattern.solid(9, 8, 7);
      currentSuccess.getLedResult.complete(pattern);
      await loaded;
      expect(container.read(ledProvider).value, same(pattern));

      subscription.close();
      container.dispose();
      await old.events.close();
      await oldError.events.close();
      await replacement.events.close();
      await currentSuccess.events.close();
    },
  );
}
