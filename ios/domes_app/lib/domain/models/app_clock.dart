/// Injectable time source used by app workflows.
library;

import 'dart:async';

abstract interface class AppTimer {
  bool get isActive;

  void cancel();
}

abstract interface class AppClock {
  DateTime now();

  AppTimer schedule(Duration delay, void Function() callback);
}

final class SystemAppClock implements AppClock {
  const SystemAppClock();

  @override
  DateTime now() => DateTime.now();

  @override
  AppTimer schedule(Duration delay, void Function() callback) =>
      _SystemAppTimer(Timer(delay, callback));
}

final class _SystemAppTimer implements AppTimer {
  _SystemAppTimer(this._timer);

  final Timer _timer;

  @override
  bool get isActive => _timer.isActive;

  @override
  void cancel() => _timer.cancel();
}

/// A manually advanced clock for deterministic app-model execution and tests.
final class DeterministicAppClock implements AppClock {
  DeterministicAppClock({DateTime? start})
    : _now = start ?? DateTime.utc(2026, 1, 1);

  DateTime _now;
  int _nextOrder = 0;
  final List<_ScheduledCallback> _callbacks = [];

  @override
  DateTime now() => _now;

  @override
  AppTimer schedule(Duration delay, void Function() callback) {
    if (delay < Duration.zero) {
      throw ArgumentError.value(delay, 'delay', 'must not be negative');
    }
    final scheduled = _ScheduledCallback(
      due: _now.add(delay),
      order: _nextOrder++,
      callback: callback,
    );
    _callbacks.add(scheduled);
    return scheduled;
  }

  int get pendingTimerCount =>
      _callbacks.where((timer) => timer.isActive).length;

  void advance(Duration elapsed) {
    if (elapsed < Duration.zero) {
      throw ArgumentError.value(elapsed, 'elapsed', 'must not be negative');
    }
    final target = _now.add(elapsed);
    while (true) {
      final ready =
          _callbacks
              .where((timer) => timer.isActive && !timer.due.isAfter(target))
              .toList()
            ..sort((left, right) {
              final byDue = left.due.compareTo(right.due);
              return byDue != 0 ? byDue : left.order.compareTo(right.order);
            });
      if (ready.isEmpty) break;
      final next = ready.first;
      _now = next.due;
      next.fire();
    }
    _now = target;
    _callbacks.removeWhere((timer) => !timer.isActive);
  }
}

final class _ScheduledCallback implements AppTimer {
  _ScheduledCallback({
    required this.due,
    required this.order,
    required this.callback,
  });

  final DateTime due;
  final int order;
  final void Function() callback;
  bool _active = true;

  @override
  bool get isActive => _active;

  void fire() {
    if (!_active) return;
    _active = false;
    callback();
  }

  @override
  void cancel() => _active = false;
}
