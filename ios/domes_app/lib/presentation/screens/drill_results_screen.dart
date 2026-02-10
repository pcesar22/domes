/// Drill results screen with summary stats and per-round breakdown.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/drill_provider.dart';
import '../../domain/models/drill_result.dart';
import '../theme/app_theme.dart';
import '../widgets/reaction_time_chart.dart';

class DrillResultsScreen extends ConsumerWidget {
  final DrillResult result;
  const DrillResultsScreen({super.key, required this.result});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Drill Results'),
        actions: [
          IconButton(
            icon: const Icon(Icons.replay),
            tooltip: 'New Drill',
            onPressed: () {
              ref.read(drillProvider.notifier).reset();
              Navigator.of(context).pop();
            },
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Summary card
          _SummaryCard(result: result),
          const SizedBox(height: 16),

          // Reaction time chart
          if (result.hitRounds.isNotEmpty) ...[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Reaction Times',
                        style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    SizedBox(
                      height: 200,
                      child: ReactionTimeChart(results: result.rounds),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 16),
          ],

          // Per-pod breakdown
          if (result.perPodResults.length > 1) ...[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Per-Pod Breakdown',
                        style: Theme.of(context).textTheme.titleMedium),
                    const Divider(),
                    ...result.perPodResults.entries.map((entry) {
                      final podRounds = entry.value;
                      final hits =
                          podRounds.where((r) => r.hit).length;
                      final hitRounds = podRounds
                          .where((r) => r.hit && r.reactionTime != null)
                          .toList();
                      final avgMs = hitRounds.isEmpty
                          ? null
                          : hitRounds
                                  .map((r) => r.reactionTime!.inMilliseconds)
                                  .reduce((a, b) => a + b) ~/
                              hitRounds.length;

                      return Padding(
                        padding: const EdgeInsets.symmetric(vertical: 4),
                        child: Row(
                          children: [
                            Expanded(
                              child: Text(
                                _shortAddress(entry.key),
                                style: const TextStyle(
                                    fontWeight: FontWeight.w500),
                              ),
                            ),
                            Text('$hits/${podRounds.length} hits'),
                            const SizedBox(width: 16),
                            if (avgMs != null) Text('avg ${avgMs}ms'),
                          ],
                        ),
                      );
                    }),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 16),
          ],

          // Round-by-round table
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Round Details',
                      style: Theme.of(context).textTheme.titleMedium),
                  const Divider(),
                  ...result.rounds.asMap().entries.map((entry) {
                    final i = entry.key;
                    final r = entry.value;
                    return Padding(
                      padding: const EdgeInsets.symmetric(vertical: 2),
                      child: Row(
                        children: [
                          SizedBox(
                            width: 32,
                            child: Text('${i + 1}',
                                style: const TextStyle(
                                    fontWeight: FontWeight.w500)),
                          ),
                          Icon(
                            r.hit ? Icons.check_circle : Icons.cancel,
                            size: 18,
                            color: r.hit
                                ? AppTheme.connectedColor
                                : AppTheme.errorColor,
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                              child: Text(_shortAddress(r.podAddress))),
                          if (r.reactionTime != null)
                            Text(
                              '${r.reactionTime!.inMilliseconds}ms',
                              style: TextStyle(
                                color: _reactionColor(r.reactionTime!),
                                fontWeight: FontWeight.bold,
                              ),
                            )
                          else
                            const Text('miss',
                                style: TextStyle(color: AppTheme.errorColor)),
                        ],
                      ),
                    );
                  }),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),

          // Done button
          FilledButton.icon(
            onPressed: () {
              ref.read(drillProvider.notifier).reset();
              Navigator.of(context).pop();
            },
            icon: const Icon(Icons.check),
            label: const Text('Done'),
            style: FilledButton.styleFrom(
              minimumSize: const Size(double.infinity, 48),
            ),
          ),
        ],
      ),
    );
  }

  String _shortAddress(String addr) {
    if (addr.startsWith('sim-')) return addr;
    if (addr.length > 8) return '...${addr.substring(addr.length - 5)}';
    return addr;
  }

  Color _reactionColor(Duration time) {
    final ms = time.inMilliseconds;
    if (ms < 300) return AppTheme.connectedColor;
    if (ms < 600) return AppTheme.connectingColor;
    return AppTheme.errorColor;
  }
}

class _SummaryCard extends StatelessWidget {
  final DrillResult result;
  const _SummaryCard({required this.result});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Summary',
                style: Theme.of(context).textTheme.titleMedium),
            const Divider(),
            _StatRow(
              'Hit Rate',
              '${(result.hitRate * 100).toStringAsFixed(0)}% (${result.hits}/${result.totalRounds})',
            ),
            if (result.avgReactionTime != null)
              _StatRow(
                'Avg Reaction',
                '${result.avgReactionTime!.inMilliseconds}ms',
              ),
            if (result.bestReactionTime != null)
              _StatRow(
                'Best',
                '${result.bestReactionTime!.inMilliseconds}ms',
              ),
            if (result.worstReactionTime != null)
              _StatRow(
                'Worst',
                '${result.worstReactionTime!.inMilliseconds}ms',
              ),
            _StatRow(
              'Duration',
              '${result.totalDuration.inSeconds}s',
            ),
            _StatRow('Pods Used', '${result.config.podAddresses.length}'),
            _StatRow('Drill Type', result.config.type.label),
          ],
        ),
      ),
    );
  }
}

class _StatRow extends StatelessWidget {
  final String label;
  final String value;
  const _StatRow(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontWeight: FontWeight.w500)),
          Text(value,
              style:
                  const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
        ],
      ),
    );
  }
}
