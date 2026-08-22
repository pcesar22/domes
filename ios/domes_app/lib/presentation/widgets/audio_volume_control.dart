import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class AudioVolumeControl extends StatelessWidget {
  const AudioVolumeControl({
    required this.volume,
    required this.onSet,
    super.key,
  });

  final AsyncValue<int> volume;
  final ValueChanged<int> onSet;

  @override
  Widget build(BuildContext context) {
    return volume.when(
      data: (value) => Column(
        children: [
          Slider(
            key: const Key('audio-volume-slider'),
            value: value.toDouble(),
            min: 0,
            max: 100,
            divisions: 100,
            label: '$value',
            onChanged: (_) {},
            onChangeEnd: (next) => onSet(next.round()),
          ),
          Text('$value/100 device-owned software gain'),
        ],
      ),
      loading: () => const Center(child: CircularProgressIndicator()),
      error: (error, _) => Text('Error: $error'),
    );
  }
}
