/// Drill setup screen for configuring and starting drills.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/drill_provider.dart';
import '../../application/providers/multi_pod_provider.dart';
import '../../domain/models/drill_config.dart';
import '../theme/app_theme.dart';
import 'drill_active_screen.dart';

class DrillSetupScreen extends ConsumerStatefulWidget {
  const DrillSetupScreen({super.key});

  @override
  ConsumerState<DrillSetupScreen> createState() => _DrillSetupScreenState();
}

class _DrillSetupScreenState extends ConsumerState<DrillSetupScreen> {
  DrillType _drillType = DrillType.reaction;
  int _roundCount = 10;
  double _timeoutSec = 3.0;
  double _minDelaySec = 0.5;
  double _maxDelaySec = 2.0;
  final Set<String> _selectedPods = {};

  @override
  Widget build(BuildContext context) {
    final multiPod = ref.watch(multiPodProvider);
    final connectedAddresses =
        multiPod.entries.where((e) => e.value.isConnected).map((e) => e.key).toList();

    return Scaffold(
      appBar: AppBar(title: const Text('Drill Setup')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Drill type selector
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Drill Type',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  SegmentedButton<DrillType>(
                    segments: DrillType.values
                        .map((t) => ButtonSegment(
                              value: t,
                              label: Text(t.label),
                            ))
                        .toList(),
                    selected: {_drillType},
                    onSelectionChanged: (s) =>
                        setState(() => _drillType = s.first),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    _drillType.description,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Pod selection
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Pods',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  if (connectedAddresses.isEmpty)
                    const Text(
                      'No pods connected. Connect pods from the home screen first.',
                      style: TextStyle(color: AppTheme.errorColor),
                    )
                  else
                    ...connectedAddresses.map((addr) {
                      final entry = multiPod[addr]!;
                      return CheckboxListTile(
                        title: Text(entry.device.name),
                        subtitle: Text(addr),
                        value: _selectedPods.contains(addr),
                        onChanged: (v) {
                          setState(() {
                            if (v == true) {
                              _selectedPods.add(addr);
                            } else {
                              _selectedPods.remove(addr);
                            }
                          });
                        },
                      );
                    }),
                  if (connectedAddresses.isNotEmpty)
                    TextButton(
                      onPressed: () {
                        setState(() {
                          if (_selectedPods.length == connectedAddresses.length) {
                            _selectedPods.clear();
                          } else {
                            _selectedPods.addAll(connectedAddresses);
                          }
                        });
                      },
                      child: Text(_selectedPods.length == connectedAddresses.length
                          ? 'Deselect All'
                          : 'Select All'),
                    ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Config sliders
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Configuration',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  _sliderRow('Rounds', _roundCount.toDouble(), 1, 50,
                      divisions: 49,
                      valueLabel: '$_roundCount',
                      onChanged: (v) =>
                          setState(() => _roundCount = v.round())),
                  _sliderRow('Timeout', _timeoutSec, 1, 10,
                      valueLabel: '${_timeoutSec.toStringAsFixed(1)}s',
                      onChanged: (v) => setState(() => _timeoutSec = v)),
                  _sliderRow('Min Delay', _minDelaySec, 0.1, 3,
                      valueLabel: '${_minDelaySec.toStringAsFixed(1)}s',
                      onChanged: (v) {
                    setState(() {
                      _minDelaySec = v;
                      if (_maxDelaySec < _minDelaySec) {
                        _maxDelaySec = _minDelaySec;
                      }
                    });
                  }),
                  _sliderRow('Max Delay', _maxDelaySec, _minDelaySec, 5,
                      valueLabel: '${_maxDelaySec.toStringAsFixed(1)}s',
                      onChanged: (v) => setState(() => _maxDelaySec = v)),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),

          // Start button
          FilledButton.icon(
            onPressed: _selectedPods.isEmpty ? null : _startDrill,
            icon: const Icon(Icons.play_arrow),
            label: const Text('Start Drill'),
            style: FilledButton.styleFrom(
              minimumSize: const Size(double.infinity, 56),
            ),
          ),

          // Simulate button (for testing without pods)
          if (connectedAddresses.isEmpty) ...[
            const SizedBox(height: 12),
            OutlinedButton.icon(
              onPressed: _startSimulatedDrill,
              icon: const Icon(Icons.science),
              label: const Text('Simulate Drill (no pods)'),
              style: OutlinedButton.styleFrom(
                minimumSize: const Size(double.infinity, 48),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _sliderRow(
    String label,
    double value,
    double min,
    double max, {
    int? divisions,
    String? valueLabel,
    required ValueChanged<double> onChanged,
  }) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(width: 80, child: Text(label)),
          Expanded(
            child: Slider(
              value: value,
              min: min,
              max: max,
              divisions: divisions,
              onChanged: onChanged,
            ),
          ),
          SizedBox(width: 48, child: Text(valueLabel ?? value.toString())),
        ],
      ),
    );
  }

  void _startDrill() {
    final config = DrillConfig(
      type: _drillType,
      roundCount: _roundCount,
      timeout: Duration(milliseconds: (_timeoutSec * 1000).round()),
      minDelay: Duration(milliseconds: (_minDelaySec * 1000).round()),
      maxDelay: Duration(milliseconds: (_maxDelaySec * 1000).round()),
      podAddresses: _selectedPods.toList(),
    );

    ref.read(drillProvider.notifier).startDrill(config);
    Navigator.of(context).pushReplacement(
      MaterialPageRoute(builder: (_) => const DrillActiveScreen()),
    );
  }

  void _startSimulatedDrill() {
    final config = DrillConfig(
      type: _drillType,
      roundCount: _roundCount,
      timeout: Duration(milliseconds: (_timeoutSec * 1000).round()),
      minDelay: Duration(milliseconds: (_minDelaySec * 1000).round()),
      maxDelay: Duration(milliseconds: (_maxDelaySec * 1000).round()),
      podAddresses: ['sim-pod-1', 'sim-pod-2', 'sim-pod-3'],
    );

    ref.read(drillProvider.notifier).startDrill(config);
    Navigator.of(context).pushReplacement(
      MaterialPageRoute(builder: (_) => const DrillActiveScreen()),
    );
  }
}
