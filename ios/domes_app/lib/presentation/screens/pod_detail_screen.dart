/// Pod detail screen showing system info, feature toggles, LED control.
library;

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/feature_provider.dart';
import '../../application/providers/led_provider.dart';
import '../../application/providers/pod_connection_provider.dart';
import '../../application/providers/system_provider.dart';
import '../../data/protocol/config_protocol.dart';
import '../../presentation/theme/app_theme.dart';
import '../widgets/feature_toggle.dart';
import '../widgets/led_pattern_picker.dart';

class PodDetailScreen extends ConsumerStatefulWidget {
  const PodDetailScreen({super.key});

  @override
  ConsumerState<PodDetailScreen> createState() => _PodDetailScreenState();
}

class _PodDetailScreenState extends ConsumerState<PodDetailScreen> {
  @override
  void initState() {
    super.initState();
    // Load all data on entry
    Future.microtask(() {
      ref.read(featureProvider.notifier).loadFeatures();
      ref.read(systemInfoProvider.notifier).loadSystemInfo();
      ref.read(systemModeProvider.notifier).loadMode();
      ref.read(ledProvider.notifier).loadPattern();
    });
  }

  @override
  Widget build(BuildContext context) {
    final connection = ref.watch(podConnectionProvider);
    final features = ref.watch(featureProvider);
    final systemInfo = ref.watch(systemInfoProvider);
    final systemMode = ref.watch(systemModeProvider);
    final ledPattern = ref.watch(ledProvider);

    if (!connection.isConnected) {
      return Scaffold(
        appBar: AppBar(title: const Text('Pod')),
        body: const Center(child: Text('Disconnected')),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: Text(connection.device?.name ?? 'Pod'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () {
              ref.read(featureProvider.notifier).loadFeatures();
              ref.read(systemInfoProvider.notifier).loadSystemInfo();
              ref.read(systemModeProvider.notifier).loadMode();
              ref.read(ledProvider.notifier).loadPattern();
            },
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: () async {
          await Future.wait([
            ref.read(featureProvider.notifier).loadFeatures(),
            ref.read(systemInfoProvider.notifier).loadSystemInfo(),
            ref.read(systemModeProvider.notifier).loadMode(),
            ref.read(ledProvider.notifier).loadPattern(),
          ]);
        },
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            // System Info Card
            _buildSystemInfoCard(context, systemInfo, systemMode),
            const SizedBox(height: 16),

            // Features Card
            _buildFeaturesCard(context, features),
            const SizedBox(height: 16),

            // LED Control Card
            _buildLedCard(context, ledPattern),
          ],
        ),
      ),
    );
  }

  Widget _buildSystemInfoCard(
    BuildContext context,
    AsyncValue<AppSystemInfo> systemInfo,
    AsyncValue<AppModeInfo> systemMode,
  ) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('System Info',
                style: Theme.of(context).textTheme.titleMedium),
            const Divider(),
            systemInfo.when(
              data: (info) => Column(
                children: [
                  _infoRow('Firmware', info.firmwareVersion),
                  _infoRow('Uptime',
                      '${info.uptimeS ~/ 3600}h ${(info.uptimeS % 3600) ~/ 60}m ${info.uptimeS % 60}s'),
                  _infoRow('Free Heap',
                      '${(info.freeHeap / 1024).toStringAsFixed(1)} KB'),
                  _infoRow('Boot Count', '${info.bootCount}'),
                  _infoRow('Mode', _modeName(info.mode)),
                ],
              ),
              loading: () =>
                  const Center(child: CircularProgressIndicator()),
              error: (e, _) => Text('Error: $e',
                  style: const TextStyle(color: AppTheme.errorColor)),
            ),
            // Mode selector
            systemMode.when(
              data: (modeInfo) => Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Row(
                  children: [
                    const Text('Mode: '),
                    const SizedBox(width: 8),
                    DropdownButton<SystemMode>(
                      value: modeInfo.mode,
                      items: [
                        SystemMode.SYSTEM_MODE_IDLE,
                        SystemMode.SYSTEM_MODE_TRIAGE,
                        SystemMode.SYSTEM_MODE_CONNECTED,
                        SystemMode.SYSTEM_MODE_GAME,
                      ]
                          .map((m) => DropdownMenuItem(
                                value: m,
                                child: Text(_modeName(m)),
                              ))
                          .toList(),
                      onChanged: (mode) {
                        if (mode != null) {
                          ref
                              .read(systemModeProvider.notifier)
                              .setMode(mode);
                        }
                      },
                    ),
                    const SizedBox(width: 8),
                    Text(
                        '(${(modeInfo.timeInModeMs / 1000).toStringAsFixed(1)}s)'),
                  ],
                ),
              ),
              loading: () => const SizedBox.shrink(),
              error: (_, _) => const SizedBox.shrink(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildFeaturesCard(
    BuildContext context,
    AsyncValue<List<AppFeatureState>> features,
  ) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Features',
                style: Theme.of(context).textTheme.titleMedium),
            const Divider(),
            features.when(
              data: (featureList) => Column(
                children: featureList
                    .map((fs) => FeatureToggle(
                          featureState: fs,
                          onToggle: (enabled) {
                            ref
                                .read(featureProvider.notifier)
                                .toggleFeature(fs.feature, enabled);
                          },
                        ))
                    .toList(),
              ),
              loading: () =>
                  const Center(child: CircularProgressIndicator()),
              error: (e, _) => Text('Error: $e',
                  style: const TextStyle(color: AppTheme.errorColor)),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildLedCard(
    BuildContext context,
    AsyncValue<AppLedPattern> ledPattern,
  ) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('LED Control',
                style: Theme.of(context).textTheme.titleMedium),
            const Divider(),
            ledPattern.when(
              data: (pattern) => LedPatternPicker(
                pattern: pattern,
                onPatternChanged: (newPattern) {
                  ref.read(ledProvider.notifier).setPattern(newPattern);
                },
              ),
              loading: () =>
                  const Center(child: CircularProgressIndicator()),
              error: (e, _) => Text('Error: $e',
                  style: const TextStyle(color: AppTheme.errorColor)),
            ),
          ],
        ),
      ),
    );
  }

  Widget _infoRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontWeight: FontWeight.w500)),
          Text(value),
        ],
      ),
    );
  }

  String _modeName(SystemMode mode) {
    return switch (mode) {
      SystemMode.SYSTEM_MODE_BOOTING => 'Booting',
      SystemMode.SYSTEM_MODE_IDLE => 'Idle',
      SystemMode.SYSTEM_MODE_TRIAGE => 'Triage',
      SystemMode.SYSTEM_MODE_CONNECTED => 'Connected',
      SystemMode.SYSTEM_MODE_GAME => 'Game',
      SystemMode.SYSTEM_MODE_ERROR => 'Error',
      _ => 'Unknown',
    };
  }
}
