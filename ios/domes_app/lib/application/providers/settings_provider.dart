/// App settings provider.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Transport preference.
enum TransportPreference { ble, wifi, serial }

/// App settings state.
class AppSettings {
  final ThemeMode themeMode;
  final TransportPreference transportPreference;
  final bool autoConnect;

  const AppSettings({
    this.themeMode = ThemeMode.dark,
    this.transportPreference = TransportPreference.ble,
    this.autoConnect = false,
  });

  AppSettings copyWith({
    ThemeMode? themeMode,
    TransportPreference? transportPreference,
    bool? autoConnect,
  }) {
    return AppSettings(
      themeMode: themeMode ?? this.themeMode,
      transportPreference: transportPreference ?? this.transportPreference,
      autoConnect: autoConnect ?? this.autoConnect,
    );
  }
}

/// Settings notifier.
class SettingsNotifier extends StateNotifier<AppSettings> {
  SettingsNotifier() : super(const AppSettings());

  void setThemeMode(ThemeMode mode) {
    state = state.copyWith(themeMode: mode);
  }

  void setTransportPreference(TransportPreference pref) {
    state = state.copyWith(transportPreference: pref);
  }

  void setAutoConnect(bool value) {
    state = state.copyWith(autoConnect: value);
  }
}

final settingsProvider =
    StateNotifierProvider<SettingsNotifier, AppSettings>((ref) {
  return SettingsNotifier();
});
