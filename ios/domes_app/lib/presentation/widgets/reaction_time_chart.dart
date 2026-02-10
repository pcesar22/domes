/// Reaction time chart widget using fl_chart.
library;

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';

import '../../domain/models/drill_result.dart';
import '../theme/app_theme.dart';

class ReactionTimeChart extends StatelessWidget {
  final List<RoundResult> results;
  const ReactionTimeChart({super.key, required this.results});

  @override
  Widget build(BuildContext context) {
    final hitResults = results
        .where((r) => r.hit && r.reactionTime != null)
        .toList();

    if (hitResults.isEmpty) {
      return const Center(child: Text('No reaction times to display'));
    }

    final maxMs = hitResults
        .map((r) => r.reactionTime!.inMilliseconds.toDouble())
        .reduce((a, b) => a > b ? a : b);

    final spots = <FlSpot>[];
    for (var i = 0; i < hitResults.length; i++) {
      spots.add(FlSpot(
        i.toDouble(),
        hitResults[i].reactionTime!.inMilliseconds.toDouble(),
      ));
    }

    return LineChart(
      LineChartData(
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: (maxMs / 4).clamp(50, 500),
        ),
        titlesData: FlTitlesData(
          topTitles: const AxisTitles(
              sideTitles: SideTitles(showTitles: false)),
          rightTitles: const AxisTitles(
              sideTitles: SideTitles(showTitles: false)),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              getTitlesWidget: (value, meta) {
                if (value.toInt() >= hitResults.length) {
                  return const SizedBox.shrink();
                }
                return Text(
                  '${value.toInt() + 1}',
                  style: const TextStyle(fontSize: 10),
                );
              },
              interval: hitResults.length > 10 ? 5 : 1,
            ),
          ),
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 48,
              getTitlesWidget: (value, meta) {
                return Text(
                  '${value.toInt()}ms',
                  style: const TextStyle(fontSize: 10),
                );
              },
            ),
          ),
        ),
        borderData: FlBorderData(show: false),
        minY: 0,
        maxY: maxMs * 1.1,
        lineBarsData: [
          LineChartBarData(
            spots: spots,
            isCurved: true,
            curveSmoothness: 0.2,
            color: AppTheme.connectedColor,
            barWidth: 2,
            dotData: FlDotData(
              show: true,
              getDotPainter: (spot, percent, barData, index) {
                final ms = spot.y;
                final color = ms < 300
                    ? AppTheme.connectedColor
                    : ms < 600
                        ? AppTheme.connectingColor
                        : AppTheme.errorColor;
                return FlDotCirclePainter(
                  radius: 4,
                  color: color,
                  strokeWidth: 0,
                );
              },
            ),
            belowBarData: BarAreaData(
              show: true,
              color: AppTheme.connectedColor.withAlpha(30),
            ),
          ),
        ],
        lineTouchData: LineTouchData(
          touchTooltipData: LineTouchTooltipData(
            getTooltipItems: (spots) {
              return spots.map((spot) {
                return LineTooltipItem(
                  '${spot.y.toInt()}ms',
                  const TextStyle(
                      color: Colors.white, fontWeight: FontWeight.bold),
                );
              }).toList();
            },
          ),
        ),
      ),
    );
  }
}
