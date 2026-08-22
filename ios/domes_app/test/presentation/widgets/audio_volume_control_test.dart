import 'package:domes_app/presentation/widgets/audio_volume_control.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('shows bounded device-owned gain and commits on drag end', (
    tester,
  ) async {
    int? applied;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: AudioVolumeControl(
            volume: const AsyncValue.data(40),
            onSet: (value) => applied = value,
          ),
        ),
      ),
    );

    expect(find.text('40/100 device-owned software gain'), findsOneWidget);
    await tester.drag(
      find.byKey(const Key('audio-volume-slider')),
      const Offset(100, 0),
    );
    await tester.pump();
    expect(applied, isNotNull);
    expect(applied, inInclusiveRange(0, 100));
  });
}
