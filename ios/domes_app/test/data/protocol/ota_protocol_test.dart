import 'dart:async';
import 'dart:collection';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:domes_app/data/protocol/msg_types.dart';
import 'package:domes_app/data/protocol/ota_protocol.dart';
import 'package:domes_app/data/transport/frame_codec.dart';
import 'package:domes_app/data/transport/transport.dart';
import 'package:flutter_test/flutter_test.dart';

typedef _ReceiveStep = Future<Frame> Function(Duration timeout);
typedef _SentFrame = ({int msgType, Uint8List payload});

final class _ScriptedTransport extends Transport {
  _ScriptedTransport({
    required Iterable<_ReceiveStep> responses,
    this.chunkSize = 2,
    this.failAbortSend = false,
  }) : _responses = Queue.of(responses);

  final Queue<_ReceiveStep> _responses;
  final List<_SentFrame> sent = [];
  final int chunkSize;
  final bool failAbortSend;
  bool disconnected = false;

  @override
  Future<void> sendFrame(int msgType, Uint8List payload) async {
    sent.add((msgType: msgType, payload: Uint8List.fromList(payload)));
    if (failAbortSend && msgType == kOtaAbort) {
      throw StateError('abort send failed');
    }
  }

  @override
  Future<Frame> receiveFrame(Duration timeout) {
    if (_responses.isEmpty) {
      throw StateError('No scripted OTA response');
    }
    return _responses.removeFirst()(timeout);
  }

  @override
  Future<Frame> transactFrame(
    int msgType,
    Uint8List payload,
    Duration timeout, {
    void Function()? onFrameSent,
  }) async {
    await sendFrame(msgType, payload);
    onFrameSent?.call();
    return receiveFrame(timeout);
  }

  @override
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload, {
    required int expectedResponseType,
  }) async {
    throw UnsupportedError('sendCommand is not used by OTA transfers');
  }

  @override
  Stream<Frame> get unsolicitedFrames => const Stream.empty();

  @override
  int get maxOtaChunkSize => chunkSize;

  @override
  Future<void> disconnect() async => disconnected = true;

  @override
  bool get isConnected => !disconnected;
}

_ReceiveStep _reply(Frame frame) =>
    (_) async => frame;

_ReceiveStep _fail(Object error) =>
    (_) async => throw error;

Frame _ack(OtaStatus status, int nextOffset) {
  return _rawAck(status.value, nextOffset);
}

Frame _rawAck(int status, int nextOffset) {
  final payload = ByteData(5)
    ..setUint8(0, status)
    ..setUint32(1, nextOffset, Endian.little);
  return Frame(msgType: kOtaAck, payload: payload.buffer.asUint8List());
}

Matcher _throwsMessage(String message) {
  return throwsA(
    predicate<Object>(
      (error) => error.toString().contains(message),
      'error containing "$message"',
    ),
  );
}

void _expectDataPayload(
  Uint8List payload, {
  required int offset,
  required List<int> data,
}) {
  final bytes = ByteData.sublistView(payload);
  expect(bytes.getUint32(0, Endian.little), offset);
  expect(bytes.getUint16(4, Endian.little), data.length);
  expect(payload.sublist(6), data);
}

Future<void> _flash(
  Transport transport,
  Uint8List firmware, {
  String version = 'v1.2.3',
}) => otaFlash(transport, firmware, version: version);

void main() {
  group('otaFlash wire protocol', () {
    test('rejects versions that occupy the full fixed-width field', () async {
      final transport = _ScriptedTransport(responses: const []);

      await expectLater(
        _flash(
          transport,
          Uint8List.fromList([1]),
          version: List.filled(32, 'x').join(),
        ),
        throwsArgumentError,
      );

      expect(transport.sent, isEmpty);
    });

    test('rejects embedded NUL in a version', () async {
      final transport = _ScriptedTransport(responses: const []);

      await expectLater(
        _flash(transport, Uint8List.fromList([1]), version: 'v1\u0000bad'),
        throwsArgumentError,
      );

      expect(transport.sent, isEmpty);
    });

    test(
      'encodes a parser-valid declared version without truncation',
      () async {
        const version = 'v1.2.3-4-gabcdef';
        final transport = _ScriptedTransport(
          responses: [
            _reply(_ack(OtaStatus.ok, 0)),
            _reply(_ack(OtaStatus.ok, 1)),
            _reply(_ack(OtaStatus.ok, 1)),
          ],
        );

        await _flash(transport, Uint8List.fromList([1]), version: version);

        final encoded = version.codeUnits;
        final versionField = transport.sent.first.payload.sublist(36, 68);
        expect(versionField.sublist(0, encoded.length), encoded);
        expect(versionField.sublist(encoded.length), everyElement(0));
      },
    );

    test('rejects non-ASCII and parser-invalid versions', () async {
      final transport = _ScriptedTransport(responses: const []);

      await expectLater(
        _flash(transport, Uint8List.fromList([1]), version: 'v1.2.é'),
        throwsArgumentError,
      );
      await expectLater(
        _flash(transport, Uint8List.fromList([1]), version: 'release'),
        throwsArgumentError,
      );
      await expectLater(
        _flash(transport, Uint8List.fromList([1]), version: ''),
        throwsArgumentError,
      );

      expect(transport.sent, isEmpty);
    });

    test('rejects an empty firmware image before sending', () async {
      final transport = _ScriptedTransport(responses: const []);

      await expectLater(_flash(transport, Uint8List(0)), throwsArgumentError);

      expect(transport.sent, isEmpty);
    });

    test('validates every ACK and sends correctly encoded chunks', () async {
      final firmware = Uint8List.fromList([1, 2, 3, 4, 5]);
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _reply(_ack(OtaStatus.ok, 2)),
          _reply(_ack(OtaStatus.ok, 4)),
          _reply(_ack(OtaStatus.ok, 5)),
          _reply(_ack(OtaStatus.ok, 5)),
        ],
      );

      await _flash(transport, firmware, version: 'v1.2.3');

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaData,
        kOtaData,
        kOtaData,
        kOtaEnd,
      ]);

      final beginPayload = transport.sent.first.payload;
      final beginBytes = ByteData.sublistView(beginPayload);
      expect(beginPayload, hasLength(68));
      expect(beginBytes.getUint32(0, Endian.little), firmware.length);
      expect(beginPayload.sublist(4, 36), sha256.convert(firmware).bytes);
      expect(beginPayload.sublist(36, 42), 'v1.2.3'.codeUnits);
      expect(beginPayload.sublist(42), everyElement(0));

      _expectDataPayload(transport.sent[1].payload, offset: 0, data: [1, 2]);
      _expectDataPayload(transport.sent[2].payload, offset: 2, data: [3, 4]);
      _expectDataPayload(transport.sent[3].payload, offset: 4, data: [5]);
      expect(transport.sent.last.payload, isEmpty);
    });

    test('aborts when OTA_BEGIN acknowledges a nonzero offset', () async {
      final transport = _ScriptedTransport(
        responses: [_reply(_ack(OtaStatus.ok, 1))],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('OTA_BEGIN acknowledged next offset 1, expected 0'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaAbort,
      ]);
      expect(transport.sent.last.payload, [OtaStatus.aborted.value]);
    });

    test('aborts when receiving the OTA_BEGIN ACK fails', () async {
      final transport = _ScriptedTransport(
        responses: [_fail(TimeoutException('begin ACK timed out'))],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('begin ACK timed out'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaAbort,
      ]);
    });

    test('aborts when the OTA_BEGIN ACK is malformed', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(Frame(msgType: kOtaAck, payload: Uint8List.fromList([0]))),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('OTA_ACK payload has 1 bytes, expected exactly 5'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaAbort,
      ]);
    });

    test('aborts when OTA_DATA acknowledges the wrong offset', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _reply(_ack(OtaStatus.ok, 1)),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2, 3])),
        _throwsMessage('OTA_DATA acknowledged next offset 1, expected 2'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaData,
        kOtaAbort,
      ]);
    });

    test('aborts when OTA_END acknowledges the wrong offset', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _reply(_ack(OtaStatus.ok, 2)),
          _reply(_ack(OtaStatus.ok, 1)),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('OTA_END acknowledged next offset 1, expected 2'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaData,
        kOtaEnd,
        kOtaAbort,
      ]);
    });

    test('aborts after a host-side receive failure', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _fail(TimeoutException('data ACK timed out')),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('data ACK timed out'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaData,
        kOtaAbort,
      ]);
    });

    test('aborts after a device rejects a data chunk', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _reply(_ack(OtaStatus.offsetMismatch, 0)),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('Device rejected chunk at offset 0: Offset mismatch'),
      );

      expect(transport.sent.last.msgType, kOtaAbort);
    });

    test('preserves the transfer error when sending OTA_ABORT fails', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(_ack(OtaStatus.ok, 0)),
          _reply(_ack(OtaStatus.ok, 1)),
        ],
        failAbortSend: true,
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('OTA_DATA acknowledged next offset 1, expected 2'),
      );

      expect(transport.sent.last.msgType, kOtaAbort);
      expect(transport.disconnected, isTrue);
    });

    test('does not abort when the device rejects OTA_BEGIN', () async {
      final transport = _ScriptedTransport(
        responses: [_reply(_ack(OtaStatus.busy, 0))],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('Device rejected OTA_BEGIN: Busy'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [kOtaBegin]);
    });

    test('aborts instead of looping when the chunk size is zero', () async {
      final transport = _ScriptedTransport(
        responses: [_reply(_ack(OtaStatus.ok, 0))],
        chunkSize: 0,
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('Transport reported invalid OTA chunk size 0'),
      );

      expect(transport.sent.map((frame) => frame.msgType), [
        kOtaBegin,
        kOtaAbort,
      ]);
    });

    test('rejects unknown ACK status codes and aborts the session', () async {
      final transport = _ScriptedTransport(
        responses: [_reply(_ack(OtaStatus.ok, 0)), _reply(_rawAck(0xFF, 2))],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('Unknown OTA status code: 255'),
      );

      expect(transport.sent.last.msgType, kOtaAbort);
    });

    test('rejects oversized ACK payloads and aborts the session', () async {
      final transport = _ScriptedTransport(
        responses: [
          _reply(
            Frame(
              msgType: kOtaAck,
              payload: Uint8List.fromList([0, 0, 0, 0, 0, 0]),
            ),
          ),
        ],
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('OTA_ACK payload has 6 bytes, expected exactly 5'),
      );

      expect(transport.sent.last.msgType, kOtaAbort);
    });

    test('requires exact OTA_ABORT payload size and known status', () async {
      final oversized = _ScriptedTransport(
        responses: [
          _reply(
            Frame(
              msgType: kOtaAbort,
              payload: Uint8List.fromList([OtaStatus.aborted.value, 0]),
            ),
          ),
        ],
      );
      await expectLater(
        _flash(oversized, Uint8List.fromList([1])),
        _throwsMessage('OTA_ABORT payload has 2 bytes, expected exactly 1'),
      );

      final unknown = _ScriptedTransport(
        responses: [
          _reply(
            Frame(msgType: kOtaAbort, payload: Uint8List.fromList([0xff])),
          ),
        ],
      );
      await expectLater(
        _flash(unknown, Uint8List.fromList([1])),
        _throwsMessage('Unknown OTA status code: 255'),
      );
    });

    test('rejects transport chunks above the wire maximum', () async {
      final transport = _ScriptedTransport(
        responses: [_reply(_ack(OtaStatus.ok, 0))],
        chunkSize: 1017,
      );

      await expectLater(
        _flash(transport, Uint8List.fromList([1, 2])),
        _throwsMessage('invalid OTA chunk size 1017'),
      );

      expect(transport.sent.last.msgType, kOtaAbort);
    });
  });
}
