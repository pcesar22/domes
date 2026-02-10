/// LED pattern picker widget.
library;

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter/material.dart';

import '../../data/protocol/config_protocol.dart';

class LedPatternPicker extends StatefulWidget {
  final AppLedPattern pattern;
  final ValueChanged<AppLedPattern> onPatternChanged;

  const LedPatternPicker({
    super.key,
    required this.pattern,
    required this.onPatternChanged,
  });

  @override
  State<LedPatternPicker> createState() => _LedPatternPickerState();
}

class _LedPatternPickerState extends State<LedPatternPicker> {
  late LedPatternType _selectedType;
  double _red = 255;
  double _green = 0;
  double _blue = 0;
  double _brightness = 128;
  double _periodMs = 2000;

  @override
  void initState() {
    super.initState();
    _selectedType = widget.pattern.patternType;
    _brightness = widget.pattern.brightness.toDouble();
    _periodMs = widget.pattern.periodMs.toDouble();
    if (widget.pattern.color != null) {
      final (r, g, b, _) = widget.pattern.color!;
      _red = r.toDouble();
      _green = g.toDouble();
      _blue = b.toDouble();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Pattern type selector
        SegmentedButton<LedPatternType>(
          segments: const [
            ButtonSegment(
              value: LedPatternType.LED_PATTERN_OFF,
              label: Text('Off'),
              icon: Icon(Icons.power_settings_new),
            ),
            ButtonSegment(
              value: LedPatternType.LED_PATTERN_SOLID,
              label: Text('Solid'),
              icon: Icon(Icons.circle),
            ),
            ButtonSegment(
              value: LedPatternType.LED_PATTERN_BREATHING,
              label: Text('Breathe'),
              icon: Icon(Icons.air),
            ),
            ButtonSegment(
              value: LedPatternType.LED_PATTERN_COLOR_CYCLE,
              label: Text('Cycle'),
              icon: Icon(Icons.palette),
            ),
          ],
          selected: {_selectedType},
          onSelectionChanged: (types) {
            setState(() => _selectedType = types.first);
          },
        ),
        const SizedBox(height: 16),

        // Color preview
        if (_selectedType != LedPatternType.LED_PATTERN_OFF) ...[
          Container(
            height: 40,
            decoration: BoxDecoration(
              color: Color.fromRGBO(
                  _red.round(), _green.round(), _blue.round(), 1.0),
              borderRadius: BorderRadius.circular(8),
            ),
          ),
          const SizedBox(height: 12),

          // RGB sliders
          _colorSlider('R', _red, const Color.fromRGBO(255, 0, 0, 1),
              (v) => setState(() => _red = v)),
          _colorSlider('G', _green, const Color.fromRGBO(0, 255, 0, 1),
              (v) => setState(() => _green = v)),
          _colorSlider('B', _blue, const Color.fromRGBO(0, 0, 255, 1),
              (v) => setState(() => _blue = v)),

          // Brightness slider
          _labeledSlider('Brightness', _brightness, 0, 255,
              (v) => setState(() => _brightness = v)),
        ],

        // Period slider for animated patterns
        if (_selectedType == LedPatternType.LED_PATTERN_BREATHING ||
            _selectedType == LedPatternType.LED_PATTERN_COLOR_CYCLE)
          _labeledSlider('Period (ms)', _periodMs, 100, 10000,
              (v) => setState(() => _periodMs = v)),

        const SizedBox(height: 12),

        // Apply button
        SizedBox(
          width: double.infinity,
          child: FilledButton(
            onPressed: _apply,
            child: const Text('Apply'),
          ),
        ),
      ],
    );
  }

  Widget _colorSlider(
      String label, double value, Color color, ValueChanged<double> onChanged) {
    return Row(
      children: [
        SizedBox(width: 16, child: Text(label)),
        Expanded(
          child: Slider(
            value: value,
            min: 0,
            max: 255,
            activeColor: color,
            onChanged: onChanged,
          ),
        ),
        SizedBox(width: 36, child: Text('${value.round()}')),
      ],
    );
  }

  Widget _labeledSlider(String label, double value, double min, double max,
      ValueChanged<double> onChanged) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('$label: ${value.round()}'),
        Slider(value: value, min: min, max: max, onChanged: onChanged),
      ],
    );
  }

  void _apply() {
    late final AppLedPattern pattern;
    switch (_selectedType) {
      case LedPatternType.LED_PATTERN_OFF:
        pattern = AppLedPattern.off();
      case LedPatternType.LED_PATTERN_SOLID:
        pattern = AppLedPattern(
          patternType: LedPatternType.LED_PATTERN_SOLID,
          color: (_red.round(), _green.round(), _blue.round(), 0),
          brightness: _brightness.round(),
        );
      case LedPatternType.LED_PATTERN_BREATHING:
        pattern = AppLedPattern(
          patternType: LedPatternType.LED_PATTERN_BREATHING,
          color: (_red.round(), _green.round(), _blue.round(), 0),
          brightness: _brightness.round(),
          periodMs: _periodMs.round(),
        );
      case LedPatternType.LED_PATTERN_COLOR_CYCLE:
        pattern = AppLedPattern.colorCycle(
          [
            (255, 0, 0, 0),
            (0, 255, 0, 0),
            (0, 0, 255, 0),
          ],
          _periodMs.round(),
        );
    }
    widget.onPatternChanged(pattern);
  }
}
