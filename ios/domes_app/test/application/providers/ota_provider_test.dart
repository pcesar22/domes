import 'dart:typed_data';

import 'package:domes_app/application/providers/ota_provider.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  late ProviderContainer container;
  late OtaNotifier notifier;

  setUp(() {
    container = ProviderContainer();
    notifier = container.read(otaProvider.notifier);
  });

  tearDown(() {
    container.dispose();
  });

  group('OtaNotifier initial state', () {
    test('starts in idle phase', () {
      expect(container.read(otaProvider).phase, OtaPhase.idle);
    });

    test('progress is 0', () {
      expect(container.read(otaProvider).progress, 0);
    });

    test('bytesSent is 0', () {
      expect(container.read(otaProvider).bytesSent, 0);
    });

    test('totalBytes is 0', () {
      expect(container.read(otaProvider).totalBytes, 0);
    });

    test('message is empty', () {
      expect(container.read(otaProvider).message, isEmpty);
    });

    test('error is null', () {
      expect(container.read(otaProvider).error, isNull);
    });
  });

  group('OtaNotifier.reset', () {
    test('resets to idle state', () {
      notifier.reset();
      final state = container.read(otaProvider);
      expect(state.phase, OtaPhase.idle);
      expect(state.bytesSent, 0);
      expect(state.totalBytes, 0);
      expect(state.message, isEmpty);
      expect(state.error, isNull);
    });
  });

  group('OtaNotifier.startOta', () {
    test('sets error when not connected', () async {
      final firmware = List.filled(1024, 0xFF);
      await notifier.startOta(
        Uint8List.fromList(firmware),
        version: 'v1.0.0',
      );

      final state = container.read(otaProvider);
      expect(state.phase, OtaPhase.error);
      expect(state.error, 'Not connected to a pod');
    });
  });

  group('OtaUpdateState', () {
    test('progress is 0 when totalBytes is 0', () {
      const state = OtaUpdateState(bytesSent: 100, totalBytes: 0);
      expect(state.progress, 0);
    });

    test('progress computes correctly', () {
      const state = OtaUpdateState(bytesSent: 500, totalBytes: 1000);
      expect(state.progress, 0.5);
    });

    test('progress is 1 when all bytes sent', () {
      const state = OtaUpdateState(bytesSent: 1000, totalBytes: 1000);
      expect(state.progress, 1.0);
    });
  });
}
