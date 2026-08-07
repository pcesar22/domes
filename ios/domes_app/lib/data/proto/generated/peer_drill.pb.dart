// This is a generated file - do not edit.
//
// Generated from peer_drill.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:core' as $core;

import 'package:protobuf/protobuf.dart' as $pb;

import 'peer_drill.pbenum.dart';

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

export 'peer_drill.pbenum.dart';

class Beacon extends $pb.GeneratedMessage {
  factory Beacon() => create();

  Beacon._();

  factory Beacon.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Beacon.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Beacon',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Beacon clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Beacon copyWith(void Function(Beacon) updates) =>
      super.copyWith((message) => updates(message as Beacon)) as Beacon;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Beacon create() => Beacon._();
  @$core.override
  Beacon createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Beacon getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Beacon>(create);
  static Beacon? _defaultInstance;
}

class Ping extends $pb.GeneratedMessage {
  factory Ping() => create();

  Ping._();

  factory Ping.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Ping.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Ping',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Ping clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Ping copyWith(void Function(Ping) updates) =>
      super.copyWith((message) => updates(message as Ping)) as Ping;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Ping create() => Ping._();
  @$core.override
  Ping createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Ping getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Ping>(create);
  static Ping? _defaultInstance;
}

class Pong extends $pb.GeneratedMessage {
  factory Pong() => create();

  Pong._();

  factory Pong.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Pong.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Pong',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Pong clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Pong copyWith(void Function(Pong) updates) =>
      super.copyWith((message) => updates(message as Pong)) as Pong;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Pong create() => Pong._();
  @$core.override
  Pong createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Pong getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Pong>(create);
  static Pong? _defaultInstance;
}

class JoinGame extends $pb.GeneratedMessage {
  factory JoinGame() => create();

  JoinGame._();

  factory JoinGame.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory JoinGame.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'JoinGame',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  JoinGame clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  JoinGame copyWith(void Function(JoinGame) updates) =>
      super.copyWith((message) => updates(message as JoinGame)) as JoinGame;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static JoinGame create() => JoinGame._();
  @$core.override
  JoinGame createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static JoinGame getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<JoinGame>(create);
  static JoinGame? _defaultInstance;
}

class StopAll extends $pb.GeneratedMessage {
  factory StopAll() => create();

  StopAll._();

  factory StopAll.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory StopAll.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'StopAll',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  StopAll clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  StopAll copyWith(void Function(StopAll) updates) =>
      super.copyWith((message) => updates(message as StopAll)) as StopAll;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static StopAll create() => StopAll._();
  @$core.override
  StopAll createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static StopAll getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StopAll>(create);
  static StopAll? _defaultInstance;
}

class ArmTouch extends $pb.GeneratedMessage {
  factory ArmTouch({
    $core.int? roundToken,
    $core.int? timeoutMs,
    FeedbackMode? feedbackMode,
  }) {
    final result = create();
    if (roundToken != null) result.roundToken = roundToken;
    if (timeoutMs != null) result.timeoutMs = timeoutMs;
    if (feedbackMode != null) result.feedbackMode = feedbackMode;
    return result;
  }

  ArmTouch._();

  factory ArmTouch.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ArmTouch.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ArmTouch',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OF3)
    ..aI(2, _omitFieldNames ? '' : 'timeoutMs', fieldType: $pb.PbFieldType.OU3)
    ..aE<FeedbackMode>(3, _omitFieldNames ? '' : 'feedbackMode',
        enumValues: FeedbackMode.values)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ArmTouch clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ArmTouch copyWith(void Function(ArmTouch) updates) =>
      super.copyWith((message) => updates(message as ArmTouch)) as ArmTouch;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ArmTouch create() => ArmTouch._();
  @$core.override
  ArmTouch createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ArmTouch getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ArmTouch>(create);
  static ArmTouch? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get roundToken => $_getIZ(0);
  @$pb.TagNumber(1)
  set roundToken($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRoundToken() => $_has(0);
  @$pb.TagNumber(1)
  void clearRoundToken() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get timeoutMs => $_getIZ(1);
  @$pb.TagNumber(2)
  set timeoutMs($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTimeoutMs() => $_has(1);
  @$pb.TagNumber(2)
  void clearTimeoutMs() => $_clearField(2);

  @$pb.TagNumber(3)
  FeedbackMode get feedbackMode => $_getN(2);
  @$pb.TagNumber(3)
  set feedbackMode(FeedbackMode value) => $_setField(3, value);
  @$pb.TagNumber(3)
  $core.bool hasFeedbackMode() => $_has(2);
  @$pb.TagNumber(3)
  void clearFeedbackMode() => $_clearField(3);
}

class SetColor extends $pb.GeneratedMessage {
  factory SetColor({
    $core.int? red,
    $core.int? green,
    $core.int? blue,
  }) {
    final result = create();
    if (red != null) result.red = red;
    if (green != null) result.green = green;
    if (blue != null) result.blue = blue;
    return result;
  }

  SetColor._();

  factory SetColor.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetColor.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetColor',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'red', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'green', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'blue', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetColor clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetColor copyWith(void Function(SetColor) updates) =>
      super.copyWith((message) => updates(message as SetColor)) as SetColor;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetColor create() => SetColor._();
  @$core.override
  SetColor createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetColor getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SetColor>(create);
  static SetColor? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get red => $_getIZ(0);
  @$pb.TagNumber(1)
  set red($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRed() => $_has(0);
  @$pb.TagNumber(1)
  void clearRed() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get green => $_getIZ(1);
  @$pb.TagNumber(2)
  set green($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasGreen() => $_has(1);
  @$pb.TagNumber(2)
  void clearGreen() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get blue => $_getIZ(2);
  @$pb.TagNumber(3)
  set blue($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasBlue() => $_has(2);
  @$pb.TagNumber(3)
  void clearBlue() => $_clearField(3);
}

class SimulateTouch extends $pb.GeneratedMessage {
  factory SimulateTouch({
    $core.int? roundToken,
    $core.int? padIndex,
  }) {
    final result = create();
    if (roundToken != null) result.roundToken = roundToken;
    if (padIndex != null) result.padIndex = padIndex;
    return result;
  }

  SimulateTouch._();

  factory SimulateTouch.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SimulateTouch.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SimulateTouch',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OF3)
    ..aI(2, _omitFieldNames ? '' : 'padIndex', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouch clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouch copyWith(void Function(SimulateTouch) updates) =>
      super.copyWith((message) => updates(message as SimulateTouch))
          as SimulateTouch;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SimulateTouch create() => SimulateTouch._();
  @$core.override
  SimulateTouch createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SimulateTouch getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SimulateTouch>(create);
  static SimulateTouch? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get roundToken => $_getIZ(0);
  @$pb.TagNumber(1)
  set roundToken($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRoundToken() => $_has(0);
  @$pb.TagNumber(1)
  void clearRoundToken() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get padIndex => $_getIZ(1);
  @$pb.TagNumber(2)
  set padIndex($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasPadIndex() => $_has(1);
  @$pb.TagNumber(2)
  void clearPadIndex() => $_clearField(2);
}

class TouchEvent extends $pb.GeneratedMessage {
  factory TouchEvent({
    $core.int? roundToken,
    $core.int? reactionTimeUs,
    $core.int? padIndex,
  }) {
    final result = create();
    if (roundToken != null) result.roundToken = roundToken;
    if (reactionTimeUs != null) result.reactionTimeUs = reactionTimeUs;
    if (padIndex != null) result.padIndex = padIndex;
    return result;
  }

  TouchEvent._();

  factory TouchEvent.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TouchEvent.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TouchEvent',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OF3)
    ..aI(2, _omitFieldNames ? '' : 'reactionTimeUs',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'padIndex', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TouchEvent clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TouchEvent copyWith(void Function(TouchEvent) updates) =>
      super.copyWith((message) => updates(message as TouchEvent)) as TouchEvent;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TouchEvent create() => TouchEvent._();
  @$core.override
  TouchEvent createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TouchEvent getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TouchEvent>(create);
  static TouchEvent? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get roundToken => $_getIZ(0);
  @$pb.TagNumber(1)
  set roundToken($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRoundToken() => $_has(0);
  @$pb.TagNumber(1)
  void clearRoundToken() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get reactionTimeUs => $_getIZ(1);
  @$pb.TagNumber(2)
  set reactionTimeUs($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasReactionTimeUs() => $_has(1);
  @$pb.TagNumber(2)
  void clearReactionTimeUs() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get padIndex => $_getIZ(2);
  @$pb.TagNumber(3)
  set padIndex($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasPadIndex() => $_has(2);
  @$pb.TagNumber(3)
  void clearPadIndex() => $_clearField(3);
}

class TimeoutEvent extends $pb.GeneratedMessage {
  factory TimeoutEvent({
    $core.int? roundToken,
  }) {
    final result = create();
    if (roundToken != null) result.roundToken = roundToken;
    return result;
  }

  TimeoutEvent._();

  factory TimeoutEvent.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TimeoutEvent.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TimeoutEvent',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OF3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TimeoutEvent clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TimeoutEvent copyWith(void Function(TimeoutEvent) updates) =>
      super.copyWith((message) => updates(message as TimeoutEvent))
          as TimeoutEvent;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TimeoutEvent create() => TimeoutEvent._();
  @$core.override
  TimeoutEvent createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TimeoutEvent getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TimeoutEvent>(create);
  static TimeoutEvent? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get roundToken => $_getIZ(0);
  @$pb.TagNumber(1)
  set roundToken($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRoundToken() => $_has(0);
  @$pb.TagNumber(1)
  void clearRoundToken() => $_clearField(1);
}

enum PeerMessage_Payload {
  beacon,
  ping,
  pong,
  joinGame,
  armTouch,
  setColor,
  stopAll,
  simulateTouch,
  touchEvent,
  timeoutEvent,
  notSet
}

class PeerMessage extends $pb.GeneratedMessage {
  factory PeerMessage({
    $core.int? protocolVersion,
    $core.List<$core.int>? senderMac,
    $core.int? senderTimestampUs,
    Beacon? beacon,
    Ping? ping,
    Pong? pong,
    JoinGame? joinGame,
    ArmTouch? armTouch,
    SetColor? setColor,
    StopAll? stopAll,
    SimulateTouch? simulateTouch,
    TouchEvent? touchEvent,
    TimeoutEvent? timeoutEvent,
  }) {
    final result = create();
    if (protocolVersion != null) result.protocolVersion = protocolVersion;
    if (senderMac != null) result.senderMac = senderMac;
    if (senderTimestampUs != null) result.senderTimestampUs = senderTimestampUs;
    if (beacon != null) result.beacon = beacon;
    if (ping != null) result.ping = ping;
    if (pong != null) result.pong = pong;
    if (joinGame != null) result.joinGame = joinGame;
    if (armTouch != null) result.armTouch = armTouch;
    if (setColor != null) result.setColor = setColor;
    if (stopAll != null) result.stopAll = stopAll;
    if (simulateTouch != null) result.simulateTouch = simulateTouch;
    if (touchEvent != null) result.touchEvent = touchEvent;
    if (timeoutEvent != null) result.timeoutEvent = timeoutEvent;
    return result;
  }

  PeerMessage._();

  factory PeerMessage.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory PeerMessage.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static const $core.Map<$core.int, PeerMessage_Payload>
      _PeerMessage_PayloadByTag = {
    10: PeerMessage_Payload.beacon,
    11: PeerMessage_Payload.ping,
    12: PeerMessage_Payload.pong,
    13: PeerMessage_Payload.joinGame,
    14: PeerMessage_Payload.armTouch,
    15: PeerMessage_Payload.setColor,
    16: PeerMessage_Payload.stopAll,
    17: PeerMessage_Payload.simulateTouch,
    18: PeerMessage_Payload.touchEvent,
    19: PeerMessage_Payload.timeoutEvent,
    0: PeerMessage_Payload.notSet
  };
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'PeerMessage',
      package:
          const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer_drill'),
      createEmptyInstance: create)
    ..oo(0, [10, 11, 12, 13, 14, 15, 16, 17, 18, 19])
    ..aI(1, _omitFieldNames ? '' : 'protocolVersion',
        fieldType: $pb.PbFieldType.OU3)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'senderMac', $pb.PbFieldType.OY)
    ..aI(3, _omitFieldNames ? '' : 'senderTimestampUs',
        fieldType: $pb.PbFieldType.OF3)
    ..aOM<Beacon>(10, _omitFieldNames ? '' : 'beacon',
        subBuilder: Beacon.create)
    ..aOM<Ping>(11, _omitFieldNames ? '' : 'ping', subBuilder: Ping.create)
    ..aOM<Pong>(12, _omitFieldNames ? '' : 'pong', subBuilder: Pong.create)
    ..aOM<JoinGame>(13, _omitFieldNames ? '' : 'joinGame',
        subBuilder: JoinGame.create)
    ..aOM<ArmTouch>(14, _omitFieldNames ? '' : 'armTouch',
        subBuilder: ArmTouch.create)
    ..aOM<SetColor>(15, _omitFieldNames ? '' : 'setColor',
        subBuilder: SetColor.create)
    ..aOM<StopAll>(16, _omitFieldNames ? '' : 'stopAll',
        subBuilder: StopAll.create)
    ..aOM<SimulateTouch>(17, _omitFieldNames ? '' : 'simulateTouch',
        subBuilder: SimulateTouch.create)
    ..aOM<TouchEvent>(18, _omitFieldNames ? '' : 'touchEvent',
        subBuilder: TouchEvent.create)
    ..aOM<TimeoutEvent>(19, _omitFieldNames ? '' : 'timeoutEvent',
        subBuilder: TimeoutEvent.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  PeerMessage clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  PeerMessage copyWith(void Function(PeerMessage) updates) =>
      super.copyWith((message) => updates(message as PeerMessage))
          as PeerMessage;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static PeerMessage create() => PeerMessage._();
  @$core.override
  PeerMessage createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static PeerMessage getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<PeerMessage>(create);
  static PeerMessage? _defaultInstance;

  @$pb.TagNumber(10)
  @$pb.TagNumber(11)
  @$pb.TagNumber(12)
  @$pb.TagNumber(13)
  @$pb.TagNumber(14)
  @$pb.TagNumber(15)
  @$pb.TagNumber(16)
  @$pb.TagNumber(17)
  @$pb.TagNumber(18)
  @$pb.TagNumber(19)
  PeerMessage_Payload whichPayload() =>
      _PeerMessage_PayloadByTag[$_whichOneof(0)]!;
  @$pb.TagNumber(10)
  @$pb.TagNumber(11)
  @$pb.TagNumber(12)
  @$pb.TagNumber(13)
  @$pb.TagNumber(14)
  @$pb.TagNumber(15)
  @$pb.TagNumber(16)
  @$pb.TagNumber(17)
  @$pb.TagNumber(18)
  @$pb.TagNumber(19)
  void clearPayload() => $_clearField($_whichOneof(0));

  /// The compatibility codec accepts exactly 1 without adding a byte to the
  /// unchanged Legacy-V1 ESP-NOW packet.
  @$pb.TagNumber(1)
  $core.int get protocolVersion => $_getIZ(0);
  @$pb.TagNumber(1)
  set protocolVersion($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasProtocolVersion() => $_has(0);
  @$pb.TagNumber(1)
  void clearProtocolVersion() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get senderMac => $_getN(1);
  @$pb.TagNumber(2)
  set senderMac($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasSenderMac() => $_has(1);
  @$pb.TagNumber(2)
  void clearSenderMac() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get senderTimestampUs => $_getIZ(2);
  @$pb.TagNumber(3)
  set senderTimestampUs($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasSenderTimestampUs() => $_has(2);
  @$pb.TagNumber(3)
  void clearSenderTimestampUs() => $_clearField(3);

  @$pb.TagNumber(10)
  Beacon get beacon => $_getN(3);
  @$pb.TagNumber(10)
  set beacon(Beacon value) => $_setField(10, value);
  @$pb.TagNumber(10)
  $core.bool hasBeacon() => $_has(3);
  @$pb.TagNumber(10)
  void clearBeacon() => $_clearField(10);
  @$pb.TagNumber(10)
  Beacon ensureBeacon() => $_ensure(3);

  @$pb.TagNumber(11)
  Ping get ping => $_getN(4);
  @$pb.TagNumber(11)
  set ping(Ping value) => $_setField(11, value);
  @$pb.TagNumber(11)
  $core.bool hasPing() => $_has(4);
  @$pb.TagNumber(11)
  void clearPing() => $_clearField(11);
  @$pb.TagNumber(11)
  Ping ensurePing() => $_ensure(4);

  @$pb.TagNumber(12)
  Pong get pong => $_getN(5);
  @$pb.TagNumber(12)
  set pong(Pong value) => $_setField(12, value);
  @$pb.TagNumber(12)
  $core.bool hasPong() => $_has(5);
  @$pb.TagNumber(12)
  void clearPong() => $_clearField(12);
  @$pb.TagNumber(12)
  Pong ensurePong() => $_ensure(5);

  @$pb.TagNumber(13)
  JoinGame get joinGame => $_getN(6);
  @$pb.TagNumber(13)
  set joinGame(JoinGame value) => $_setField(13, value);
  @$pb.TagNumber(13)
  $core.bool hasJoinGame() => $_has(6);
  @$pb.TagNumber(13)
  void clearJoinGame() => $_clearField(13);
  @$pb.TagNumber(13)
  JoinGame ensureJoinGame() => $_ensure(6);

  @$pb.TagNumber(14)
  ArmTouch get armTouch => $_getN(7);
  @$pb.TagNumber(14)
  set armTouch(ArmTouch value) => $_setField(14, value);
  @$pb.TagNumber(14)
  $core.bool hasArmTouch() => $_has(7);
  @$pb.TagNumber(14)
  void clearArmTouch() => $_clearField(14);
  @$pb.TagNumber(14)
  ArmTouch ensureArmTouch() => $_ensure(7);

  @$pb.TagNumber(15)
  SetColor get setColor => $_getN(8);
  @$pb.TagNumber(15)
  set setColor(SetColor value) => $_setField(15, value);
  @$pb.TagNumber(15)
  $core.bool hasSetColor() => $_has(8);
  @$pb.TagNumber(15)
  void clearSetColor() => $_clearField(15);
  @$pb.TagNumber(15)
  SetColor ensureSetColor() => $_ensure(8);

  @$pb.TagNumber(16)
  StopAll get stopAll => $_getN(9);
  @$pb.TagNumber(16)
  set stopAll(StopAll value) => $_setField(16, value);
  @$pb.TagNumber(16)
  $core.bool hasStopAll() => $_has(9);
  @$pb.TagNumber(16)
  void clearStopAll() => $_clearField(16);
  @$pb.TagNumber(16)
  StopAll ensureStopAll() => $_ensure(9);

  @$pb.TagNumber(17)
  SimulateTouch get simulateTouch => $_getN(10);
  @$pb.TagNumber(17)
  set simulateTouch(SimulateTouch value) => $_setField(17, value);
  @$pb.TagNumber(17)
  $core.bool hasSimulateTouch() => $_has(10);
  @$pb.TagNumber(17)
  void clearSimulateTouch() => $_clearField(17);
  @$pb.TagNumber(17)
  SimulateTouch ensureSimulateTouch() => $_ensure(10);

  @$pb.TagNumber(18)
  TouchEvent get touchEvent => $_getN(11);
  @$pb.TagNumber(18)
  set touchEvent(TouchEvent value) => $_setField(18, value);
  @$pb.TagNumber(18)
  $core.bool hasTouchEvent() => $_has(11);
  @$pb.TagNumber(18)
  void clearTouchEvent() => $_clearField(18);
  @$pb.TagNumber(18)
  TouchEvent ensureTouchEvent() => $_ensure(11);

  @$pb.TagNumber(19)
  TimeoutEvent get timeoutEvent => $_getN(12);
  @$pb.TagNumber(19)
  set timeoutEvent(TimeoutEvent value) => $_setField(19, value);
  @$pb.TagNumber(19)
  $core.bool hasTimeoutEvent() => $_has(12);
  @$pb.TagNumber(19)
  void clearTimeoutEvent() => $_clearField(19);
  @$pb.TagNumber(19)
  TimeoutEvent ensureTimeoutEvent() => $_ensure(12);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
