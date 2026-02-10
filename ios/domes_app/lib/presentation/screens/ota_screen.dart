/// OTA firmware update screen.
library;

import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/ota_provider.dart';
import '../../application/providers/pod_connection_provider.dart';
import '../theme/app_theme.dart';

class OtaScreen extends ConsumerStatefulWidget {
  const OtaScreen({super.key});

  @override
  ConsumerState<OtaScreen> createState() => _OtaScreenState();
}

class _OtaScreenState extends ConsumerState<OtaScreen> {
  Uint8List? _firmwareBytes;
  String? _fileName;
  String _version = 'v1.0.0';

  @override
  Widget build(BuildContext context) {
    final otaState = ref.watch(otaProvider);
    final connection = ref.watch(podConnectionProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('OTA Update')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Connection check
          if (!connection.isConnected)
            Card(
              color: AppTheme.errorColor.withAlpha(30),
              child: const Padding(
                padding: EdgeInsets.all(16),
                child: Row(
                  children: [
                    Icon(Icons.warning, color: AppTheme.errorColor),
                    SizedBox(width: 12),
                    Expanded(
                      child: Text(
                        'Not connected to a pod. Connect first.',
                        style: TextStyle(color: AppTheme.errorColor),
                      ),
                    ),
                  ],
                ),
              ),
            ),

          // File picker
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Firmware File',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 12),
                  OutlinedButton.icon(
                    onPressed: otaState.phase == OtaPhase.transferring
                        ? null
                        : _pickFile,
                    icon: const Icon(Icons.file_open),
                    label: Text(_fileName ?? 'Select .bin file'),
                    style: OutlinedButton.styleFrom(
                      minimumSize: const Size(double.infinity, 48),
                    ),
                  ),
                  if (_firmwareBytes != null) ...[
                    const SizedBox(height: 8),
                    Text(
                      'Size: ${(_firmwareBytes!.length / 1024).toStringAsFixed(1)} KB',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Version input
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Version',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  TextField(
                    decoration: const InputDecoration(
                      hintText: 'e.g. v1.0.0',
                      border: OutlineInputBorder(),
                    ),
                    controller: TextEditingController(text: _version),
                    onChanged: (v) => _version = v,
                    enabled: otaState.phase != OtaPhase.transferring,
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Progress
          if (otaState.phase != OtaPhase.idle) ...[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Progress',
                        style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 12),
                    LinearProgressIndicator(
                      value: otaState.phase == OtaPhase.transferring
                          ? otaState.progress
                          : otaState.phase == OtaPhase.completed
                              ? 1.0
                              : null,
                    ),
                    const SizedBox(height: 8),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text(_phaseLabel(otaState.phase)),
                        if (otaState.totalBytes > 0)
                          Text(
                            '${(otaState.bytesSent / 1024).toStringAsFixed(0)} / ${(otaState.totalBytes / 1024).toStringAsFixed(0)} KB',
                          ),
                      ],
                    ),
                    if (otaState.message.isNotEmpty) ...[
                      const SizedBox(height: 4),
                      Text(
                        otaState.message,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                    if (otaState.error != null) ...[
                      const SizedBox(height: 8),
                      Text(
                        otaState.error!,
                        style: const TextStyle(color: AppTheme.errorColor),
                      ),
                    ],
                  ],
                ),
              ),
            ),
            const SizedBox(height: 12),
          ],

          // Flash button
          FilledButton.icon(
            onPressed: _canFlash(otaState, connection) ? _startOta : null,
            icon: const Icon(Icons.system_update),
            label: const Text('Flash Firmware'),
            style: FilledButton.styleFrom(
              minimumSize: const Size(double.infinity, 56),
            ),
          ),

          // Reset button (if error or completed)
          if (otaState.phase == OtaPhase.error ||
              otaState.phase == OtaPhase.completed) ...[
            const SizedBox(height: 12),
            OutlinedButton(
              onPressed: () => ref.read(otaProvider.notifier).reset(),
              child: const Text('Reset'),
            ),
          ],
        ],
      ),
    );
  }

  bool _canFlash(OtaUpdateState otaState, ConnectedPodState connection) {
    return _firmwareBytes != null &&
        connection.isConnected &&
        otaState.phase != OtaPhase.transferring &&
        otaState.phase != OtaPhase.verifying;
  }

  Future<void> _pickFile() async {
    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['bin'],
      withData: true,
    );

    if (result != null && result.files.single.bytes != null) {
      setState(() {
        _firmwareBytes = result.files.single.bytes!;
        _fileName = result.files.single.name;
      });
    }
  }

  void _startOta() {
    if (_firmwareBytes == null) return;
    ref.read(otaProvider.notifier).startOta(
          _firmwareBytes!,
          version: _version,
        );
  }

  String _phaseLabel(OtaPhase phase) {
    return switch (phase) {
      OtaPhase.idle => 'Idle',
      OtaPhase.preparing => 'Preparing...',
      OtaPhase.transferring => 'Transferring...',
      OtaPhase.verifying => 'Verifying...',
      OtaPhase.completed => 'Complete!',
      OtaPhase.error => 'Error',
    };
  }
}
