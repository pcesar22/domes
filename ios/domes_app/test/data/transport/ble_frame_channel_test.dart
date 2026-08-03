import 'dart:async';
import 'dart:typed_data';

import 'package:domes_app/data/transport/ble_frame_channel.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('BleFrameChannel', () {
    test('retains an immediate response emitted during the write', () async {
      final responsePayload = Uint8List.fromList([0, 4, 0, 0, 0]);
      final responseBytes = encodeFrame(0x04, responsePayload);
      late BleFrameChannel channel;

      channel = BleFrameChannel(
        maximumWriteSize: () => 512,
        writeChunk: (chunk) async {
          channel.addNotification(responseBytes);
        },
      );

      final response = await channel.sendCommand(
        0x20,
        Uint8List.fromList([1, 2, 3]),
        const Duration(milliseconds: 50),
        0x04,
      );

      expect(response.msgType, 0x04);
      expect(response.payload, responsePayload);
    });

    test(
      'routes an unsolicited touch event around a command response',
      () async {
        final touchPayload = Uint8List.fromList([8, 2, 16, 1]);
        final responsePayload = Uint8List.fromList([0]);
        final events = <Frame>[];
        late BleFrameChannel channel;

        channel = BleFrameChannel(
          maximumWriteSize: () => 512,
          unsolicitedMessageTypes: const {0x50},
          onUnsolicitedFrame: events.add,
          writeChunk: (chunk) async {
            channel.addNotification(encodeFrame(0x50, touchPayload));
            channel.addNotification(encodeFrame(0x23, responsePayload));
          },
        );

        final response = await channel.sendCommand(
          0x22,
          Uint8List(0),
          const Duration(milliseconds: 50),
          0x23,
        );

        expect(response.msgType, 0x23);
        expect(response.payload, responsePayload);
        expect(events, hasLength(1));
        expect(events.single.msgType, 0x50);
        expect(events.single.payload, touchPayload);
        expect(channel.isPoisoned, isFalse);
      },
    );

    test('serializes commands until each expected response arrives', () async {
      final writes = <Uint8List>[];
      late BleFrameChannel channel;
      channel = BleFrameChannel(
        maximumWriteSize: () => 512,
        writeChunk: (chunk) async {
          writes.add(Uint8List.fromList(chunk));
        },
      );

      final first = channel.sendCommand(
        0x20,
        Uint8List(0),
        const Duration(milliseconds: 100),
        0x21,
      );
      final second = channel.sendCommand(
        0x22,
        Uint8List(0),
        const Duration(milliseconds: 100),
        0x23,
      );
      await Future<void>.delayed(Duration.zero);
      expect(writes, hasLength(1));

      channel.addNotification(encodeFrame(0x21, Uint8List(0)));
      expect((await first).msgType, 0x21);
      await Future<void>.delayed(Duration.zero);
      expect(writes, hasLength(2));

      channel.addNotification(encodeFrame(0x23, Uint8List(0)));
      expect((await second).msgType, 0x23);
    });

    test('poisons the channel after an ambiguous command timeout', () async {
      final channel = BleFrameChannel(
        maximumWriteSize: () => 512,
        writeChunk: (chunk) async {},
      );

      await expectLater(
        channel.sendCommand(
          0x20,
          Uint8List(0),
          const Duration(milliseconds: 5),
          0x21,
        ),
        throwsA(
          isA<StateError>().having(
            (error) => error.message,
            'message',
            contains('Disconnect and reconnect'),
          ),
        ),
      );

      expect(channel.isPoisoned, isTrue);
      await expectLater(
        channel.sendFrame(0x20, Uint8List(0)),
        throwsA(isA<StateError>()),
      );
    });

    test('poisons the channel after a mismatched response type', () async {
      late BleFrameChannel channel;
      channel = BleFrameChannel(
        maximumWriteSize: () => 512,
        writeChunk: (chunk) async {
          channel.addNotification(encodeFrame(0x25, Uint8List(0)));
        },
      );

      await expectLater(
        channel.sendCommand(
          0x22,
          Uint8List(0),
          const Duration(milliseconds: 50),
          0x23,
        ),
        throwsA(
          isA<StateError>().having(
            (error) => error.message,
            'message',
            contains('Unexpected BLE response type'),
          ),
        ),
      );
      expect(channel.isPoisoned, isTrue);
    });

    test('poisons the channel after a command write failure', () async {
      final channel = BleFrameChannel(
        maximumWriteSize: () => 4,
        writeChunk: (chunk) async {
          throw StateError('ATT write failed');
        },
      );

      await expectLater(
        channel.sendCommand(
          0x22,
          Uint8List.fromList([1, 2, 3, 4]),
          const Duration(milliseconds: 50),
          0x23,
        ),
        throwsA(
          isA<StateError>().having(
            (error) => error.message,
            'message',
            contains('may be partial'),
          ),
        ),
      );
      expect(channel.isPoisoned, isTrue);
    });

    test('fragments a frame at the negotiated write size', () async {
      final writes = <Uint8List>[];
      final payload = Uint8List.fromList(List<int>.generate(17, (i) => i));
      final expected = encodeFrame(0x20, payload);
      final channel = BleFrameChannel(
        maximumWriteSize: () => 7,
        writeChunk: (chunk) async {
          writes.add(Uint8List.fromList(chunk));
        },
      );

      await channel.sendFrame(0x20, payload);

      expect(writes, hasLength((expected.length / 7).ceil()));
      expect(writes.every((chunk) => chunk.length <= 7), isTrue);
      expect(writes.expand((chunk) => chunk).toList(), expected);
    });

    test(
      'holds a fragmented raw transaction ahead of a queued command',
      () async {
        final writes = <Uint8List>[];
        final releaseFirstChunk = Completer<void>();
        final rawPayload = Uint8List.fromList(List<int>.generate(11, (i) => i));
        final commandPayload = Uint8List.fromList([21, 22, 23, 24, 25]);
        final rawFrame = encodeFrame(0x01, rawPayload);
        final commandFrame = encodeFrame(0x22, commandPayload);
        var rawResponseSent = false;
        var commandResponseSent = false;
        late BleFrameChannel channel;

        channel = BleFrameChannel(
          maximumWriteSize: () => 4,
          writeChunk: (chunk) async {
            writes.add(Uint8List.fromList(chunk));
            if (writes.length == 1) {
              await releaseFirstChunk.future;
            }
            final bytesWritten = writes.fold<int>(
              0,
              (total, written) => total + written.length,
            );
            if (!rawResponseSent && bytesWritten == rawFrame.length) {
              rawResponseSent = true;
              channel.addNotification(encodeFrame(0x04, Uint8List(0)));
            } else if (!commandResponseSent &&
                bytesWritten == rawFrame.length + commandFrame.length) {
              commandResponseSent = true;
              channel.addNotification(encodeFrame(0x23, Uint8List(0)));
            }
          },
        );

        final rawTransaction = channel.transactFrame(
          0x01,
          rawPayload,
          const Duration(milliseconds: 100),
        );
        final command = channel.sendCommand(
          0x22,
          commandPayload,
          const Duration(milliseconds: 100),
          0x23,
        );
        await Future<void>.delayed(Duration.zero);
        expect(writes, hasLength(1));

        releaseFirstChunk.complete();
        expect((await rawTransaction).msgType, 0x04);
        expect((await command).msgType, 0x23);

        expect(writes.expand((chunk) => chunk).toList(), <int>[
          ...rawFrame,
          ...commandFrame,
        ]);
      },
    );

    test(
      'holds a command response wait ahead of a queued raw transaction',
      () async {
        final writes = <Uint8List>[];
        final commandFrame = encodeFrame(0x22, Uint8List.fromList([1]));
        final rawFrame = encodeFrame(0x01, Uint8List.fromList([2]));
        late BleFrameChannel channel;
        channel = BleFrameChannel(
          maximumWriteSize: () => 512,
          writeChunk: (chunk) async {
            writes.add(Uint8List.fromList(chunk));
            final bytesWritten = writes.fold<int>(
              0,
              (total, written) => total + written.length,
            );
            if (bytesWritten == commandFrame.length + rawFrame.length) {
              channel.addNotification(encodeFrame(0x04, Uint8List(0)));
            }
          },
        );

        final command = channel.sendCommand(
          0x22,
          Uint8List.fromList([1]),
          const Duration(milliseconds: 100),
          0x23,
        );
        final rawTransaction = channel.transactFrame(
          0x01,
          Uint8List.fromList([2]),
          const Duration(milliseconds: 100),
        );
        await Future<void>.delayed(Duration.zero);
        expect(writes.expand((chunk) => chunk).toList(), commandFrame);

        channel.addNotification(encodeFrame(0x23, Uint8List(0)));
        expect((await command).msgType, 0x23);
        expect((await rawTransaction).msgType, 0x04);
        expect(writes.expand((chunk) => chunk).toList(), <int>[
          ...commandFrame,
          ...rawFrame,
        ]);
      },
    );

    test('reset fails an active and queued operation', () async {
      final channel = BleFrameChannel(
        maximumWriteSize: () => 512,
        writeChunk: (chunk) async {},
      );
      final active = channel.sendCommand(
        0x20,
        Uint8List(0),
        const Duration(seconds: 1),
        0x21,
      );
      final queued = channel.sendCommand(
        0x22,
        Uint8List(0),
        const Duration(seconds: 1),
        0x23,
      );
      await Future<void>.delayed(Duration.zero);

      channel.reset(StateError('BLE disconnected'));

      await expectLater(active, throwsA(isA<StateError>()));
      await expectLater(queued, throwsA(isA<StateError>()));
    });
  });
}
