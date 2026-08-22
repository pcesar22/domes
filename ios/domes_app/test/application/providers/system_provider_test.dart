import 'package:domes_app/application/providers/pod_connection_provider.dart';
import 'package:domes_app/application/providers/system_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import '../../domain/repositories/generation_test_fakes.dart';

final _testRepositoryProvider = StateProvider<PodRepository?>((ref) => null);

void main() {
  test(
    'system info ignores superseded data and observes current data',
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
      final subscription = container.listen(systemInfoProvider, (_, _) {});
      final notifier = container.read(systemInfoProvider.notifier);

      final stale = notifier.loadSystemInfo();
      container.read(_testRepositoryProvider.notifier).state = replacement;
      old.getInfoResult.complete(testSystemInfo);
      await stale;
      expect(container.read(systemInfoProvider).isLoading, isTrue);

      final oldError = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = oldError;
      final staleError = notifier.loadSystemInfo();
      container.read(_testRepositoryProvider.notifier).state = replacement;
      oldError.getInfoResult.completeError(StateError('stale info failure'));
      await staleError;
      expect(container.read(systemInfoProvider).isLoading, isTrue);

      final current = notifier.loadSystemInfo();
      replacement.getInfoResult.complete(testSystemInfo);
      await current;
      expect(container.read(systemInfoProvider).value, same(testSystemInfo));

      final currentError = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = currentError;
      final failed = notifier.loadSystemInfo();
      currentError.getInfoResult.completeError(
        StateError('current info failure'),
      );
      await failed;
      expect(container.read(systemInfoProvider).error, isA<StateError>());

      subscription.close();
      container.dispose();
      await old.events.close();
      await oldError.events.close();
      await replacement.events.close();
      await currentError.events.close();
    },
  );

  test(
    'system mode follow-up read cannot cross repository replacement',
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
      final subscription = container.listen(systemModeProvider, (_, _) {});
      final notifier = container.read(systemModeProvider.notifier);

      final stale = notifier.setMode(SystemMode.SYSTEM_MODE_IDLE);
      old.setModeResult.complete((SystemMode.SYSTEM_MODE_IDLE, true));
      await Future<void>.delayed(Duration.zero);
      container.read(_testRepositoryProvider.notifier).state = replacement;
      old.getModeResult.complete(testMode);
      await stale;
      expect(container.read(systemModeProvider).isLoading, isTrue);

      final oldError = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = oldError;
      final staleError = notifier.loadMode();
      container.read(_testRepositoryProvider.notifier).state = replacement;
      oldError.getModeResult.completeError(StateError('stale mode failure'));
      await staleError;
      expect(container.read(systemModeProvider).isLoading, isTrue);

      final current = notifier.loadMode();
      replacement.getModeResult.completeError(
        StateError('current mode failure'),
      );
      await current;
      expect(container.read(systemModeProvider).error, isA<StateError>());

      final currentSuccess = ControllablePodRepository();
      container.read(_testRepositoryProvider.notifier).state = currentSuccess;
      final loaded = notifier.loadMode();
      currentSuccess.getModeResult.complete(testMode);
      await loaded;
      expect(container.read(systemModeProvider).value, same(testMode));

      subscription.close();
      container.dispose();
      await old.events.close();
      await oldError.events.close();
      await replacement.events.close();
      await currentSuccess.events.close();
    },
  );
}
