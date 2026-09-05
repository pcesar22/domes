import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:domes_app/data/protocol/config_protocol.dart';
import 'package:domes_app/data/transport/virtual_pod_transport.dart';
import 'package:domes_app/domain/models/app_clock.dart';
import 'package:domes_app/domain/repositories/pod_repository_impl.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test(
    'routes current commands and pod-owned touch through protobuf',
    () async {
      final clock = DeterministicAppClock();
      final transport = VirtualPodTransport(
        address: 'app-virtual-pod-01',
        podId: 1,
        clock: clock,
      );
      final repository = PodRepositoryImpl(transport);

      final touch = repository.touchEvents.first;
      expect(await repository.setSystemMode(SystemMode.SYSTEM_MODE_GAME), (
        SystemMode.SYSTEM_MODE_GAME,
        true,
      ));
      final requested = AppLedPattern.solid(2, 4, 8);
      expect(
        (await repository.setLedPattern(requested)).matchesApplied(requested),
        isTrue,
      );
      expect(await repository.setAudioVolume(45), 45);
      expect(await repository.getAudioVolume(), 45);
      expect(
        await repository.triggerFeedback(
          FeedbackProbe.FEEDBACK_PROBE_FIXED_HAPTIC,
        ),
        isTrue,
      );

      clock.advance(const Duration(milliseconds: 37));
      transport.emitTouch(padIndex: 2);
      final event = await touch;
      expect((event.podId, event.padIndex), (1, 2));
      expect(event.timestampUs, clock.now().microsecondsSinceEpoch);
      expect(
        transport.commands.map((command) => command.requestType),
        containsAllInOrder([
          MsgType.MSG_TYPE_SET_MODE_REQ.value,
          MsgType.MSG_TYPE_SET_LED_PATTERN_REQ.value,
          MsgType.MSG_TYPE_SET_AUDIO_VOLUME_REQ.value,
          MsgType.MSG_TYPE_GET_AUDIO_VOLUME_REQ.value,
          MsgType.MSG_TYPE_TRIGGER_FEEDBACK_REQ.value,
        ]),
      );

      await transport.disconnect();
      expect(transport.isConnected, isFalse);
      expect(() => transport.emitTouch(), throwsStateError);
    },
  );

  test('same virtual-time input reproduces the command sequence', () async {
    Future<List<String>> execute() async {
      final clock = DeterministicAppClock();
      final transport = VirtualPodTransport(
        address: 'app-virtual-pod-04',
        podId: 4,
        clock: clock,
      );
      final repository = PodRepositoryImpl(transport);
      await repository.setSystemMode(SystemMode.SYSTEM_MODE_GAME);
      clock.advance(const Duration(milliseconds: 25));
      await repository.setLedPattern(AppLedPattern.solid(0, 255, 0));
      clock.advance(const Duration(milliseconds: 75));
      await repository.setLedPattern(AppLedPattern.off());
      final signatures = transport.commands
          .map((command) => command.signature)
          .toList();
      await transport.disconnect();
      return signatures;
    }

    expect(await execute(), await execute());
  });
}
