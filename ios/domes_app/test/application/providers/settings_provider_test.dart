import 'package:domes_app/application/providers/settings_provider.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  late ProviderContainer container;
  late SettingsNotifier notifier;

  setUp(() {
    container = ProviderContainer();
    notifier = container.read(settingsProvider.notifier);
  });

  tearDown(() {
    container.dispose();
  });

  group('SettingsNotifier initial state', () {
    test('defaults to dark theme', () {
      expect(container.read(settingsProvider).themeMode, ThemeMode.dark);
    });

    test('defaults to BLE transport', () {
      expect(container.read(settingsProvider).transportPreference,
          TransportPreference.ble);
    });

    test('auto-connect is false by default', () {
      expect(container.read(settingsProvider).autoConnect, isFalse);
    });
  });

  group('SettingsNotifier.setThemeMode', () {
    test('changes theme mode to light', () {
      notifier.setThemeMode(ThemeMode.light);
      expect(container.read(settingsProvider).themeMode, ThemeMode.light);
    });

    test('changes theme mode to system', () {
      notifier.setThemeMode(ThemeMode.system);
      expect(container.read(settingsProvider).themeMode, ThemeMode.system);
    });
  });

  group('SettingsNotifier.setTransportPreference', () {
    test('changes transport to WiFi', () {
      notifier.setTransportPreference(TransportPreference.wifi);
      expect(container.read(settingsProvider).transportPreference,
          TransportPreference.wifi);
    });

    test('changes transport to serial', () {
      notifier.setTransportPreference(TransportPreference.serial);
      expect(container.read(settingsProvider).transportPreference,
          TransportPreference.serial);
    });
  });

  group('SettingsNotifier.setAutoConnect', () {
    test('enables auto-connect', () {
      notifier.setAutoConnect(true);
      expect(container.read(settingsProvider).autoConnect, isTrue);
    });

    test('disables auto-connect', () {
      notifier.setAutoConnect(true);
      notifier.setAutoConnect(false);
      expect(container.read(settingsProvider).autoConnect, isFalse);
    });
  });

  group('AppSettings.copyWith', () {
    test('preserves unchanged fields', () {
      const settings = AppSettings(
        themeMode: ThemeMode.light,
        transportPreference: TransportPreference.wifi,
        autoConnect: true,
      );
      final updated = settings.copyWith(themeMode: ThemeMode.dark);
      expect(updated.themeMode, ThemeMode.dark);
      expect(updated.transportPreference, TransportPreference.wifi);
      expect(updated.autoConnect, isTrue);
    });
  });
}
