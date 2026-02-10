/// Game events from pods.
library;

enum GameEventType {
  hit,
  miss,
  timeout,
}

class GameEvent {
  final GameEventType type;
  final String podAddress;
  final Duration? reactionTime;
  final DateTime timestamp;

  const GameEvent({
    required this.type,
    required this.podAddress,
    this.reactionTime,
    required this.timestamp,
  });
}
