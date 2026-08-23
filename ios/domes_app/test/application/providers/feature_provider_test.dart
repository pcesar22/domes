import 'package:domes_app/application/providers/feature_provider.dart';
import 'package:domes_app/application/providers/pod_connection_provider.dart';
import 'package:domes_app/data/proto/generated/config.pb.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/domain/repositories/pod_repository.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import '../../domain/repositories/generation_test_fakes.dart';

final _testRepositoryProvider = StateProvider<PodRepository?>((ref) => null);

ProviderContainer _container(PodRepository repository) {
  final container = ProviderContainer(
    overrides: [
      podRepositoryProvider.overrideWith(
        (ref) => ref.watch(_testRepositoryProvider),
      ),
    ],
  );
  container.read(_testRepositoryProvider.notifier).state = repository;
  return container;
}

void main() {
  test('superseded feature success and error cannot publish', () async {
    final old = ControllablePodRepository();
    final replacement = ControllablePodRepository();
    final container = _container(old);
    final subscription = container.listen(featureProvider, (_, _) {});
    final notifier = container.read(featureProvider.notifier);

    final staleSuccess = notifier.loadFeatures();
    container.read(_testRepositoryProvider.notifier).state = replacement;
    old.listFeaturesResult.complete(const [
      AppFeatureState(feature: Feature.FEATURE_AUDIO, enabled: true),
    ]);
    await staleSuccess;
    expect(container.read(featureProvider).isLoading, isTrue);

    final oldError = ControllablePodRepository();
    container.read(_testRepositoryProvider.notifier).state = oldError;
    final staleError = notifier.loadFeatures();
    container.read(_testRepositoryProvider.notifier).state = replacement;
    oldError.listFeaturesResult.completeError(StateError('stale failure'));
    await staleError;
    expect(container.read(featureProvider).isLoading, isTrue);

    final current = notifier.loadFeatures();
    replacement.listFeaturesResult.complete(const []);
    await current;
    expect(container.read(featureProvider).value, isEmpty);

    final currentError = ControllablePodRepository();
    container.read(_testRepositoryProvider.notifier).state = currentError;
    final failed = notifier.loadFeatures();
    currentError.listFeaturesResult.completeError(
      StateError('current failure'),
    );
    await failed;
    expect(container.read(featureProvider).error, isA<StateError>());

    subscription.close();
    container.dispose();
    await old.events.close();
    await oldError.events.close();
    await replacement.events.close();
    await currentError.events.close();
  });

  test(
    'feature follow-up read stays bound to its starting repository',
    () async {
      final old = ControllablePodRepository();
      final replacement = ControllablePodRepository();
      final container = _container(old);
      final subscription = container.listen(featureProvider, (_, _) {});
      final notifier = container.read(featureProvider.notifier);

      final toggle = notifier.toggleFeature(Feature.FEATURE_AUDIO, true);
      old.setFeatureResult.complete(
        const AppFeatureState(feature: Feature.FEATURE_AUDIO, enabled: true),
      );
      await Future<void>.delayed(Duration.zero);
      container.read(_testRepositoryProvider.notifier).state = replacement;
      old.listFeaturesResult.complete(const []);
      await toggle;

      expect(container.read(featureProvider).isLoading, isTrue);
      subscription.close();
      container.dispose();
      await old.events.close();
      await replacement.events.close();
    },
  );
}
