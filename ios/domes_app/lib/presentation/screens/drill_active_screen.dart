/// Active drill screen showing live state and reaction times.
library;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/providers/drill_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/reaction_time_chart.dart';
import 'drill_results_screen.dart';

class DrillActiveScreen extends ConsumerWidget {
  const DrillActiveScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final drillState = ref.watch(drillProvider);

    // Navigate to results when finished
    if (drillState.phase == DrillPhase.finished) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final result = ref.read(drillProvider.notifier).drillResult;
        if (result != null && context.mounted) {
          Navigator.of(context).pushReplacement(
            MaterialPageRoute(
              builder: (_) => DrillResultsScreen(result: result),
            ),
          );
        }
      });
    }

    return PopScope(
      canPop: !drillState.isRunning,
      onPopInvokedWithResult: (didPop, _) {
        if (!didPop && drillState.isRunning) {
          _showStopDialog(context, ref);
        }
      },
      child: Scaffold(
        appBar: AppBar(
          title: Text(
              'Round ${drillState.currentRound + 1}/${drillState.config?.roundCount ?? 0}'),
          leading: drillState.isRunning
              ? IconButton(
                  icon: const Icon(Icons.close),
                  onPressed: () => _showStopDialog(context, ref),
                )
              : null,
        ),
        body: Column(
          children: [
            // Phase indicator
            _PhaseBar(phase: drillState.phase),

            // Main content
            Expanded(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  children: [
                    // Active pod indicator
                    Expanded(
                      flex: 2,
                      child: _PodGrid(drillState: drillState),
                    ),

                    // Live reaction time
                    if (drillState.lastReactionTime != null)
                      Padding(
                        padding: const EdgeInsets.symmetric(vertical: 8),
                        child: Text(
                          '${drillState.lastReactionTime!.inMilliseconds}ms',
                          style: Theme.of(context)
                              .textTheme
                              .displayMedium
                              ?.copyWith(
                                color: _reactionTimeColor(
                                    drillState.lastReactionTime!),
                                fontWeight: FontWeight.bold,
                              ),
                        ),
                      ),

                    // Reaction time chart
                    if (drillState.results.isNotEmpty)
                      Expanded(
                        flex: 1,
                        child: Padding(
                          padding: const EdgeInsets.only(top: 8),
                          child: ReactionTimeChart(results: drillState.results),
                        ),
                      ),
                  ],
                ),
              ),
            ),

            // Simulate touch button (for testing)
            if (drillState.phase == DrillPhase.waitingTouch)
              Padding(
                padding: const EdgeInsets.all(16),
                child: OutlinedButton.icon(
                  onPressed: () =>
                      ref.read(drillProvider.notifier).simulateTouch(),
                  icon: const Icon(Icons.touch_app),
                  label: const Text('Simulate Touch'),
                  style: OutlinedButton.styleFrom(
                    minimumSize: const Size(double.infinity, 48),
                  ),
                ),
              ),

            // Stop button
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
              child: FilledButton.tonalIcon(
                onPressed: () {
                  ref.read(drillProvider.notifier).stopDrill();
                },
                icon: const Icon(Icons.stop),
                label: const Text('Stop Drill'),
                style: FilledButton.styleFrom(
                  minimumSize: const Size(double.infinity, 48),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _showStopDialog(BuildContext context, WidgetRef ref) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Stop Drill?'),
        content: const Text('The current drill will be stopped.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('Continue'),
          ),
          FilledButton(
            onPressed: () {
              ref.read(drillProvider.notifier).stopDrill();
              Navigator.of(ctx).pop();
              Navigator.of(context).pop();
            },
            child: const Text('Stop'),
          ),
        ],
      ),
    );
  }

  Color _reactionTimeColor(Duration time) {
    final ms = time.inMilliseconds;
    if (ms < 300) return AppTheme.connectedColor;
    if (ms < 600) return AppTheme.connectingColor;
    return AppTheme.errorColor;
  }
}

/// Phase indicator bar at the top.
class _PhaseBar extends StatelessWidget {
  final DrillPhase phase;
  const _PhaseBar({required this.phase});

  @override
  Widget build(BuildContext context) {
    final (label, color, icon) = switch (phase) {
      DrillPhase.idle => ('Idle', Colors.grey, Icons.pause),
      DrillPhase.preparing => ('Preparing...', AppTheme.connectingColor, Icons.settings),
      DrillPhase.waitingDelay => ('Get Ready...', AppTheme.connectingColor, Icons.timer),
      DrillPhase.armed => ('ARMED', AppTheme.errorColor, Icons.warning),
      DrillPhase.waitingTouch => ('GO!', AppTheme.connectedColor, Icons.touch_app),
      DrillPhase.roundComplete => ('Nice!', AppTheme.connectedColor, Icons.check),
      DrillPhase.finished => ('Done!', AppTheme.connectedColor, Icons.flag),
      DrillPhase.error => ('Error', AppTheme.errorColor, Icons.error),
    };

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
      color: color.withAlpha(40),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(icon, color: color),
          const SizedBox(width: 8),
          Text(
            label,
            style: Theme.of(context)
                .textTheme
                .titleLarge
                ?.copyWith(color: color, fontWeight: FontWeight.bold),
          ),
        ],
      ),
    );
  }
}

/// Grid showing pod states during drill.
class _PodGrid extends StatelessWidget {
  final DrillState drillState;
  const _PodGrid({required this.drillState});

  @override
  Widget build(BuildContext context) {
    final pods = drillState.config?.podAddresses ?? [];
    if (pods.isEmpty) {
      return const Center(child: Text('No pods'));
    }

    return GridView.builder(
      gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: pods.length <= 2 ? pods.length : 2,
        mainAxisSpacing: 12,
        crossAxisSpacing: 12,
      ),
      itemCount: pods.length,
      itemBuilder: (context, index) {
        final addr = pods[index];
        final isActive = addr == drillState.activePodAddress;
        final isWaiting = drillState.phase == DrillPhase.waitingTouch;

        return Card(
          color: isActive && isWaiting
              ? AppTheme.connectedColor.withAlpha(60)
              : null,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
            side: isActive
                ? BorderSide(color: AppTheme.connectedColor, width: 3)
                : BorderSide.none,
          ),
          child: Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(
                  Icons.sports_soccer,
                  size: 48,
                  color: isActive && isWaiting
                      ? AppTheme.connectedColor
                      : null,
                ),
                const SizedBox(height: 8),
                Text(
                  _shortAddress(addr),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                if (isActive && isWaiting)
                  const Padding(
                    padding: EdgeInsets.only(top: 4),
                    child: Text(
                      'TOUCH!',
                      style: TextStyle(
                        color: AppTheme.connectedColor,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
              ],
            ),
          ),
        );
      },
    );
  }

  String _shortAddress(String addr) {
    if (addr.startsWith('sim-')) return addr;
    if (addr.length > 8) return '...${addr.substring(addr.length - 5)}';
    return addr;
  }
}
