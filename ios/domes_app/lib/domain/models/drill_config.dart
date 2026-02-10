/// Drill configuration model.
library;

enum DrillType {
  reaction('Reaction', 'Touch the lit pod as fast as possible'),
  sequence('Sequence', 'Touch pods in the correct order'),
  speed('Speed', 'Touch as many pods as possible in the time limit');

  final String label;
  final String description;
  const DrillType(this.label, this.description);
}

class DrillConfig {
  final DrillType type;
  final int roundCount;
  final Duration timeout;
  final Duration minDelay;
  final Duration maxDelay;
  final List<String> podAddresses;

  const DrillConfig({
    this.type = DrillType.reaction,
    this.roundCount = 10,
    this.timeout = const Duration(seconds: 3),
    this.minDelay = const Duration(milliseconds: 500),
    this.maxDelay = const Duration(milliseconds: 2000),
    this.podAddresses = const [],
  });

  DrillConfig copyWith({
    DrillType? type,
    int? roundCount,
    Duration? timeout,
    Duration? minDelay,
    Duration? maxDelay,
    List<String>? podAddresses,
  }) =>
      DrillConfig(
        type: type ?? this.type,
        roundCount: roundCount ?? this.roundCount,
        timeout: timeout ?? this.timeout,
        minDelay: minDelay ?? this.minDelay,
        maxDelay: maxDelay ?? this.maxDelay,
        podAddresses: podAddresses ?? this.podAddresses,
      );
}
