/// Settings screen.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/settings_provider.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final settings = ref.watch(settingsProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        children: [
          // Theme section
          _SectionHeader(title: 'Appearance'),
          ListTile(
            leading: const Icon(Icons.dark_mode),
            title: const Text('Theme'),
            subtitle: Text(_themeModeLabel(settings.themeMode)),
            onTap: () => _showThemePicker(context, ref, settings.themeMode),
          ),

          const Divider(),

          // Transport section
          _SectionHeader(title: 'Connection'),
          ListTile(
            leading: const Icon(Icons.bluetooth),
            title: const Text('Default Transport'),
            subtitle: Text(_transportLabel(settings.transportPreference)),
            onTap: () => _showTransportPicker(
                context, ref, settings.transportPreference),
          ),
          SwitchListTile(
            secondary: const Icon(Icons.link),
            title: const Text('Auto-Connect'),
            subtitle: const Text('Reconnect to last device on app start'),
            value: settings.autoConnect,
            onChanged: (value) {
              ref.read(settingsProvider.notifier).setAutoConnect(value);
            },
          ),

          const Divider(),

          // About section
          _SectionHeader(title: 'About'),
          const ListTile(
            leading: Icon(Icons.info_outline),
            title: Text('DOMES Controller'),
            subtitle: Text('v0.1.0'),
          ),
        ],
      ),
    );
  }

  String _themeModeLabel(ThemeMode mode) {
    return switch (mode) {
      ThemeMode.dark => 'Dark',
      ThemeMode.light => 'Light',
      ThemeMode.system => 'System',
    };
  }

  String _transportLabel(TransportPreference pref) {
    return switch (pref) {
      TransportPreference.ble => 'Bluetooth Low Energy',
      TransportPreference.wifi => 'WiFi',
      TransportPreference.serial => 'USB Serial',
    };
  }

  void _showThemePicker(
      BuildContext context, WidgetRef ref, ThemeMode current) {
    showDialog(
      context: context,
      builder: (context) => SimpleDialog(
        title: const Text('Theme'),
        children: [
          for (final mode in ThemeMode.values)
            SimpleDialogOption(
              onPressed: () {
                ref.read(settingsProvider.notifier).setThemeMode(mode);
                Navigator.pop(context);
              },
              child: Row(
                children: [
                  Icon(mode == current
                      ? Icons.radio_button_checked
                      : Icons.radio_button_unchecked),
                  const SizedBox(width: 12),
                  Text(_themeModeLabel(mode)),
                ],
              ),
            ),
        ],
      ),
    );
  }

  void _showTransportPicker(
      BuildContext context, WidgetRef ref, TransportPreference current) {
    showDialog(
      context: context,
      builder: (context) => SimpleDialog(
        title: const Text('Default Transport'),
        children: [
          for (final pref in TransportPreference.values)
            SimpleDialogOption(
              onPressed: () {
                ref
                    .read(settingsProvider.notifier)
                    .setTransportPreference(pref);
                Navigator.pop(context);
              },
              child: Row(
                children: [
                  Icon(pref == current
                      ? Icons.radio_button_checked
                      : Icons.radio_button_unchecked),
                  const SizedBox(width: 12),
                  Text(_transportLabel(pref)),
                ],
              ),
            ),
        ],
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  final String title;
  const _SectionHeader({required this.title});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 4),
      child: Text(
        title,
        style: Theme.of(context).textTheme.labelLarge?.copyWith(
              color: Theme.of(context).colorScheme.primary,
            ),
      ),
    );
  }
}
