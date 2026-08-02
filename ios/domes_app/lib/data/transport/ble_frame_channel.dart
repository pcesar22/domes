/// Framed byte-stream I/O over a BLE characteristic.
library;

import 'dart:async';
import 'dart:collection';
import 'dart:typed_data';

import 'frame_codec.dart';

/// Writes one ATT-sized chunk to the BLE data characteristic.
typedef BleChunkWriter = Future<void> Function(Uint8List chunk);

/// Owns BLE frame fragmentation, decoding, and response delivery.
///
/// Decoded frames are retained until [receiveFrame] consumes them. This is
/// required because a peripheral can notify an ACK before a characteristic
/// write future completes.
final class BleFrameChannel {
  BleFrameChannel({
    required BleChunkWriter writeChunk,
    required int Function() maximumWriteSize,
    Set<int> unsolicitedMessageTypes = const {},
    void Function(Frame)? onUnsolicitedFrame,
  }) : _writeChunk = writeChunk,
       _maximumWriteSize = maximumWriteSize,
       _unsolicitedMessageTypes = unsolicitedMessageTypes,
       _onUnsolicitedFrame = onUnsolicitedFrame;

  final BleChunkWriter _writeChunk;
  final int Function() _maximumWriteSize;
  final Set<int> _unsolicitedMessageTypes;
  final void Function(Frame)? _onUnsolicitedFrame;
  final FrameDecoder _decoder = FrameDecoder();
  final Queue<_FrameResult> _pending = Queue<_FrameResult>();

  Completer<Frame>? _waiter;
  Timer? _waiterTimer;
  Future<void> _operationQueue = Future<void>.value();
  bool _commandPoisoned = false;
  Object? _poisonReason;

  /// Whether a prior ambiguous command failure requires a reconnect.
  bool get isPoisoned => _commandPoisoned;

  /// Encode and write a frame in negotiated ATT-sized chunks.
  Future<void> sendFrame(int msgType, Uint8List payload) {
    return _enqueueOperation(() async {
      _ensureNoBufferedResponse();
      await _writeFrame(msgType, payload);
    });
  }

  Future<void> _writeFrame(int msgType, Uint8List payload) async {
    final encoded = encodeFrame(msgType, payload);
    final writeSize = _maximumWriteSize();
    if (writeSize <= 0) {
      throw StateError('BLE negotiated an invalid write size: $writeSize');
    }

    for (var offset = 0; offset < encoded.length; offset += writeSize) {
      final end = (offset + writeSize < encoded.length)
          ? offset + writeSize
          : encoded.length;
      await _writeChunk(Uint8List.sublistView(encoded, offset, end));
    }
  }

  /// Atomically send a raw request and receive its response.
  Future<Frame> transactFrame(
    int msgType,
    Uint8List payload,
    Duration timeout, {
    void Function()? onFrameSent,
  }) {
    return _enqueueOperation(() async {
      _ensureNoBufferedResponse();
      await _writeFrame(msgType, payload);
      onFrameSent?.call();
      return _receiveFrame(timeout);
    });
  }

  Future<T> _enqueueOperation<T>(Future<T> Function() operation) {
    final result = Completer<T>();
    final previous = _operationQueue;

    _operationQueue = () async {
      await previous;
      try {
        _throwIfPoisoned();
        result.complete(await operation());
      } catch (error, stackTrace) {
        if (!_commandPoisoned) {
          _poison(error);
        }
        result.completeError(error, stackTrace);
      }
    }();

    return result.future;
  }

  /// Feed one notification fragment into the streaming frame decoder.
  void addNotification(List<int> data) {
    for (final byte in data) {
      switch (_decoder.feedByte(byte)) {
        case DecodeSuccess(:final frame):
          if (_unsolicitedMessageTypes.contains(frame.msgType)) {
            _onUnsolicitedFrame?.call(frame);
          } else {
            _emit(_FrameValue(frame));
          }
          _decoder.reset();
        case DecodeError(:final error):
          _emit(_FrameFailure(error, StackTrace.current));
          _decoder.reset();
        case DecodeNone():
          break;
      }
    }
  }

  /// Forward a BLE notification-stream error to the next receiver.
  void addError(Object error, [StackTrace? stackTrace]) {
    _emit(_FrameFailure(error, stackTrace ?? StackTrace.current));
  }

  /// Receive the oldest decoded frame, waiting up to [timeout] when empty.
  Future<Frame> receiveFrame(Duration timeout) {
    return _enqueueOperation(() => _receiveFrame(timeout));
  }

  Future<Frame> _receiveFrame(Duration timeout) {
    if (_pending.isNotEmpty) {
      return _pending.removeFirst().asFuture();
    }
    if (_waiter != null) {
      return Future<Frame>.error(
        StateError('Concurrent BLE frame receives are not supported'),
      );
    }

    final waiter = Completer<Frame>();
    _waiter = waiter;
    _waiterTimer = Timer(timeout, () {
      if (!identical(_waiter, waiter)) {
        return;
      }
      _waiter = null;
      _waiterTimer = null;
      waiter.completeError(TimeoutException('BLE response timeout', timeout));
    });
    return waiter.future;
  }

  /// Send one command at a time and consume only its expected response.
  Future<Frame> sendCommand(
    int msgType,
    Uint8List payload,
    Duration timeout,
    int expectedResponseType,
  ) {
    return _enqueueOperation(() async {
      _ensureNoBufferedResponse();
      try {
        await _writeFrame(msgType, payload);
      } catch (error, stackTrace) {
        final poisonedError = StateError(
          'BLE command write failed and may be partial. '
          'Disconnect and reconnect before sending another command: $error',
        );
        Error.throwWithStackTrace(poisonedError, stackTrace);
      }

      try {
        final frame = await _receiveFrame(timeout);
        if (frame.msgType != expectedResponseType) {
          throw StateError(
            'Unexpected BLE response type 0x${frame.msgType.toRadixString(16).padLeft(2, '0')}; '
            'expected 0x${expectedResponseType.toRadixString(16).padLeft(2, '0')}. '
            'Disconnect and reconnect before sending another command.',
          );
        }
        return frame;
      } on TimeoutException catch (error, stackTrace) {
        final poisonedError = StateError(
          'BLE command timed out and its response is ambiguous. '
          'Disconnect and reconnect before sending another command: $error',
        );
        Error.throwWithStackTrace(poisonedError, stackTrace);
      }
    });
  }

  /// Drop buffered data and fail any active receiver.
  void reset([Object? reason]) {
    final resetReason = reason ?? StateError('BLE frame channel reset');
    _commandPoisoned = true;
    _poisonReason = resetReason;
    _decoder.reset();
    _pending.clear();
    _waiterTimer?.cancel();
    _waiterTimer = null;

    final waiter = _waiter;
    _waiter = null;
    if (waiter != null && !waiter.isCompleted) {
      waiter.completeError(resetReason);
    }
  }

  void _poison(Object reason) => reset(reason);

  void _throwIfPoisoned() {
    if (!_commandPoisoned) {
      return;
    }
    throw StateError(
      'BLE frame channel requires reconnect after an ambiguous failure: '
      '$_poisonReason',
    );
  }

  void _ensureNoBufferedResponse() {
    if (_pending.isNotEmpty) {
      throw StateError(
        'BLE received an unexpected response without an active transaction. '
        'Disconnect and reconnect before sending another frame.',
      );
    }
  }

  void _emit(_FrameResult result) {
    final waiter = _waiter;
    if (waiter == null) {
      _pending.addLast(result);
      return;
    }

    _waiter = null;
    _waiterTimer?.cancel();
    _waiterTimer = null;
    result.complete(waiter);
  }
}

sealed class _FrameResult {
  const _FrameResult();

  Future<Frame> asFuture();
  void complete(Completer<Frame> waiter);
}

final class _FrameValue extends _FrameResult {
  const _FrameValue(this.frame);

  final Frame frame;

  @override
  Future<Frame> asFuture() => Future<Frame>.value(frame);

  @override
  void complete(Completer<Frame> waiter) => waiter.complete(frame);
}

final class _FrameFailure extends _FrameResult {
  const _FrameFailure(this.error, this.stackTrace);

  final Object error;
  final StackTrace stackTrace;

  @override
  Future<Frame> asFuture() => Future<Frame>.error(error, stackTrace);

  @override
  void complete(Completer<Frame> waiter) =>
      waiter.completeError(error, stackTrace);
}
