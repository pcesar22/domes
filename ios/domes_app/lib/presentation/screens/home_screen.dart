/// Home screen with BLE scan and device list.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/ble_scanner_provider.dart';
import '../../application/providers/pod_connection_provider.dart';
import '../../domain/models/pod_device.dart';
import '../../presentation/theme/app_theme.dart';
import 'drill_setup_screen.dart';
import 'ota_screen.dart';
import 'pod_detail_screen.dart';

class HomeScreen extends ConsumerWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final pods = ref.watch(bleScannerProvider);
    final isScanning = ref.watch(isScanningProvider);
    final connectionState = ref.watch(podConnectionProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('DOMES'),
        actions: [
          if (connectionState.isConnected)
            Padding(
              padding: const EdgeInsets.only(right: 8),
              child: Chip(
                avatar: const Icon(Icons.bluetooth_connected,
                    color: AppTheme.connectedColor, size: 18),
                label: Text(connectionState.device?.name ?? 'Connected'),
              ),
            ),
          PopupMenuButton<String>(
            onSelected: (value) {
              switch (value) {
                case 'drill':
                  Navigator.of(context).push(
                    MaterialPageRoute(builder: (_) => const DrillSetupScreen()),
                  );
                case 'ota':
                  Navigator.of(context).push(
                    MaterialPageRoute(builder: (_) => const OtaScreen()),
                  );
              }
            },
            itemBuilder: (_) => [
              const PopupMenuItem(
                value: 'drill',
                child: ListTile(
                  leading: Icon(Icons.timer),
                  title: Text('Drill'),
                  contentPadding: EdgeInsets.zero,
                ),
              ),
              const PopupMenuItem(
                value: 'ota',
                child: ListTile(
                  leading: Icon(Icons.system_update),
                  title: Text('OTA Update'),
                  contentPadding: EdgeInsets.zero,
                ),
              ),
            ],
          ),
        ],
      ),
      body: Column(
        children: [
          // Connection status banner
          if (connectionState.isConnected)
            MaterialBanner(
              content: Text(
                  'Connected to ${connectionState.device?.name ?? "device"}'),
              leading:
                  const Icon(Icons.check_circle, color: AppTheme.connectedColor),
              actions: [
                TextButton(
                  onPressed: () {
                    Navigator.of(context).push(
                      MaterialPageRoute(
                          builder: (_) => const PodDetailScreen()),
                    );
                  },
                  child: const Text('OPEN'),
                ),
                TextButton(
                  onPressed: () =>
                      ref.read(podConnectionProvider.notifier).disconnect(),
                  child: const Text('DISCONNECT'),
                ),
              ],
            ),

          // Error banner
          if (connectionState.error != null)
            MaterialBanner(
              content: Text(connectionState.error!),
              leading: const Icon(Icons.error, color: AppTheme.errorColor),
              backgroundColor: AppTheme.errorColor.withAlpha(30),
              actions: [
                TextButton(
                  onPressed: () {
                    // Clear error by disconnecting
                    ref.read(podConnectionProvider.notifier).disconnect();
                  },
                  child: const Text('DISMISS'),
                ),
              ],
            ),

          // Device list
          Expanded(
            child: pods.isEmpty
                ? Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(Icons.bluetooth_searching,
                            size: 64,
                            color: Theme.of(context)
                                .colorScheme
                                .onSurface
                                .withAlpha(100)),
                        const SizedBox(height: 16),
                        Text(
                          isScanning.value == true
                              ? 'Scanning for DOMES pods...'
                              : 'Tap scan to find DOMES pods',
                          style: Theme.of(context).textTheme.bodyLarge,
                        ),
                      ],
                    ),
                  )
                : ListView.builder(
                    itemCount: pods.length,
                    itemBuilder: (context, index) {
                      final pod = pods[index];
                      return _PodListTile(pod: pod);
                    },
                  ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () {
          if (isScanning.value == true) {
            ref.read(bleScannerProvider.notifier).stopScan();
          } else {
            ref.read(bleScannerProvider.notifier).startScan();
          }
        },
        icon: Icon(
            isScanning.value == true ? Icons.stop : Icons.bluetooth_searching),
        label: Text(isScanning.value == true ? 'Stop' : 'Scan'),
      ),
    );
  }
}

class _PodListTile extends ConsumerWidget {
  final PodDevice pod;

  const _PodListTile({required this.pod});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final connectionState = ref.watch(podConnectionProvider);
    final isThisConnected =
        connectionState.isConnected && connectionState.device?.address == pod.address;

    return ListTile(
      leading: Icon(
        Icons.sports_soccer,
        color: isThisConnected ? AppTheme.connectedColor : null,
      ),
      title: Text(pod.name),
      subtitle: Text('${pod.address}  RSSI: ${pod.rssi}'),
      trailing: isThisConnected
          ? const Icon(Icons.check_circle, color: AppTheme.connectedColor)
          : const Icon(Icons.chevron_right),
      onTap: () async {
        if (isThisConnected) {
          Navigator.of(context).push(
            MaterialPageRoute(builder: (_) => const PodDetailScreen()),
          );
        } else {
          await ref.read(podConnectionProvider.notifier).connect(pod);
          if (ref.read(podConnectionProvider).isConnected &&
              context.mounted) {
            Navigator.of(context).push(
              MaterialPageRoute(builder: (_) => const PodDetailScreen()),
            );
          }
        }
      },
    );
  }
}
