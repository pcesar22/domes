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

import 'package:fixnum/fixnum.dart' as $fixnum;
import 'package:protobuf/protobuf.dart' as $pb;

import 'peer_drill.pbenum.dart';

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

export 'peer_drill.pbenum.dart';

class PeerHeader extends $pb.GeneratedMessage {
  factory PeerHeader({
    ContractVersion? version,
    $core.int? srcPodId,
    $core.int? dstPodId,
    PeerRole? senderRole,
    $fixnum.Int64? timestampUs,
    $core.int? sequence,
    $core.List<$core.int>? senderMac,
  }) {
    final result = create();
    if (version != null) result.version = version;
    if (srcPodId != null) result.srcPodId = srcPodId;
    if (dstPodId != null) result.dstPodId = dstPodId;
    if (senderRole != null) result.senderRole = senderRole;
    if (timestampUs != null) result.timestampUs = timestampUs;
    if (sequence != null) result.sequence = sequence;
    if (senderMac != null) result.senderMac = senderMac;
    return result;
  }

  PeerHeader._();

  factory PeerHeader.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory PeerHeader.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'PeerHeader',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aE<ContractVersion>(1, _omitFieldNames ? '' : 'version',
        enumValues: ContractVersion.values)
    ..aI(2, _omitFieldNames ? '' : 'srcPodId', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'dstPodId', fieldType: $pb.PbFieldType.OU3)
    ..aE<PeerRole>(4, _omitFieldNames ? '' : 'senderRole',
        enumValues: PeerRole.values)
    ..a<$fixnum.Int64>(
        5, _omitFieldNames ? '' : 'timestampUs', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aI(6, _omitFieldNames ? '' : 'sequence', fieldType: $pb.PbFieldType.OU3)
    ..a<$core.List<$core.int>>(
        7, _omitFieldNames ? '' : 'senderMac', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  PeerHeader clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  PeerHeader copyWith(void Function(PeerHeader) updates) =>
      super.copyWith((message) => updates(message as PeerHeader)) as PeerHeader;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static PeerHeader create() => PeerHeader._();
  @$core.override
  PeerHeader createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static PeerHeader getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<PeerHeader>(create);
  static PeerHeader? _defaultInstance;

  @$pb.TagNumber(1)
  ContractVersion get version => $_getN(0);
  @$pb.TagNumber(1)
  set version(ContractVersion value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasVersion() => $_has(0);
  @$pb.TagNumber(1)
  void clearVersion() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get srcPodId => $_getIZ(1);
  @$pb.TagNumber(2)
  set srcPodId($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasSrcPodId() => $_has(1);
  @$pb.TagNumber(2)
  void clearSrcPodId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get dstPodId => $_getIZ(2);
  @$pb.TagNumber(3)
  set dstPodId($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasDstPodId() => $_has(2);
  @$pb.TagNumber(3)
  void clearDstPodId() => $_clearField(3);

  @$pb.TagNumber(4)
  PeerRole get senderRole => $_getN(3);
  @$pb.TagNumber(4)
  set senderRole(PeerRole value) => $_setField(4, value);
  @$pb.TagNumber(4)
  $core.bool hasSenderRole() => $_has(3);
  @$pb.TagNumber(4)
  void clearSenderRole() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get timestampUs => $_getI64(4);
  @$pb.TagNumber(5)
  set timestampUs($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasTimestampUs() => $_has(4);
  @$pb.TagNumber(5)
  void clearTimestampUs() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.int get sequence => $_getIZ(5);
  @$pb.TagNumber(6)
  set sequence($core.int value) => $_setUnsignedInt32(5, value);
  @$pb.TagNumber(6)
  $core.bool hasSequence() => $_has(5);
  @$pb.TagNumber(6)
  void clearSequence() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.List<$core.int> get senderMac => $_getN(6);
  @$pb.TagNumber(7)
  set senderMac($core.List<$core.int> value) => $_setBytes(6, value);
  @$pb.TagNumber(7)
  $core.bool hasSenderMac() => $_has(6);
  @$pb.TagNumber(7)
  void clearSenderMac() => $_clearField(7);
}

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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
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
  factory JoinGame({
    PeerRole? assignedRole,
  }) {
    final result = create();
    if (assignedRole != null) result.assignedRole = assignedRole;
    return result;
  }

  JoinGame._();

  factory JoinGame.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory JoinGame.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'JoinGame',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aE<PeerRole>(1, _omitFieldNames ? '' : 'assignedRole',
        enumValues: PeerRole.values)
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

  @$pb.TagNumber(1)
  PeerRole get assignedRole => $_getN(0);
  @$pb.TagNumber(1)
  set assignedRole(PeerRole value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasAssignedRole() => $_has(0);
  @$pb.TagNumber(1)
  void clearAssignedRole() => $_clearField(1);
}

class ArmTouch extends $pb.GeneratedMessage {
  factory ArmTouch({
    $core.int? roundToken,
    $core.int? timeoutMs,
    $core.int? feedbackMode,
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'timeoutMs', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'feedbackMode',
        fieldType: $pb.PbFieldType.OU3)
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
  $core.int get feedbackMode => $_getIZ(2);
  @$pb.TagNumber(3)
  set feedbackMode($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasFeedbackMode() => $_has(2);
  @$pb.TagNumber(3)
  void clearFeedbackMode() => $_clearField(3);
}

class SetColor extends $pb.GeneratedMessage {
  factory SetColor({
    $core.int? r,
    $core.int? g,
    $core.int? b,
  }) {
    final result = create();
    if (r != null) result.r = r;
    if (g != null) result.g = g;
    if (b != null) result.b = b;
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'r', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'g', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'b', fieldType: $pb.PbFieldType.OU3)
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
  $core.int get r => $_getIZ(0);
  @$pb.TagNumber(1)
  set r($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasR() => $_has(0);
  @$pb.TagNumber(1)
  void clearR() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get g => $_getIZ(1);
  @$pb.TagNumber(2)
  set g($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasG() => $_has(1);
  @$pb.TagNumber(2)
  void clearG() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get b => $_getIZ(2);
  @$pb.TagNumber(3)
  set b($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasB() => $_has(2);
  @$pb.TagNumber(3)
  void clearB() => $_clearField(3);
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OU3)
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OU3)
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
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundToken', fieldType: $pb.PbFieldType.OU3)
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
    PeerHeader? header,
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
    if (header != null) result.header = header;
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
    16: PeerMessage_Payload.beacon,
    17: PeerMessage_Payload.ping,
    18: PeerMessage_Payload.pong,
    19: PeerMessage_Payload.joinGame,
    20: PeerMessage_Payload.armTouch,
    21: PeerMessage_Payload.setColor,
    22: PeerMessage_Payload.stopAll,
    23: PeerMessage_Payload.simulateTouch,
    24: PeerMessage_Payload.touchEvent,
    25: PeerMessage_Payload.timeoutEvent,
    0: PeerMessage_Payload.notSet
  };
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'PeerMessage',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.peer'),
      createEmptyInstance: create)
    ..oo(0, [16, 17, 18, 19, 20, 21, 22, 23, 24, 25])
    ..aOM<PeerHeader>(1, _omitFieldNames ? '' : 'header',
        subBuilder: PeerHeader.create)
    ..aOM<Beacon>(16, _omitFieldNames ? '' : 'beacon',
        subBuilder: Beacon.create)
    ..aOM<Ping>(17, _omitFieldNames ? '' : 'ping', subBuilder: Ping.create)
    ..aOM<Pong>(18, _omitFieldNames ? '' : 'pong', subBuilder: Pong.create)
    ..aOM<JoinGame>(19, _omitFieldNames ? '' : 'joinGame',
        subBuilder: JoinGame.create)
    ..aOM<ArmTouch>(20, _omitFieldNames ? '' : 'armTouch',
        subBuilder: ArmTouch.create)
    ..aOM<SetColor>(21, _omitFieldNames ? '' : 'setColor',
        subBuilder: SetColor.create)
    ..aOM<StopAll>(22, _omitFieldNames ? '' : 'stopAll',
        subBuilder: StopAll.create)
    ..aOM<SimulateTouch>(23, _omitFieldNames ? '' : 'simulateTouch',
        subBuilder: SimulateTouch.create)
    ..aOM<TouchEvent>(24, _omitFieldNames ? '' : 'touchEvent',
        subBuilder: TouchEvent.create)
    ..aOM<TimeoutEvent>(25, _omitFieldNames ? '' : 'timeoutEvent',
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

  @$pb.TagNumber(16)
  @$pb.TagNumber(17)
  @$pb.TagNumber(18)
  @$pb.TagNumber(19)
  @$pb.TagNumber(20)
  @$pb.TagNumber(21)
  @$pb.TagNumber(22)
  @$pb.TagNumber(23)
  @$pb.TagNumber(24)
  @$pb.TagNumber(25)
  PeerMessage_Payload whichPayload() =>
      _PeerMessage_PayloadByTag[$_whichOneof(0)]!;
  @$pb.TagNumber(16)
  @$pb.TagNumber(17)
  @$pb.TagNumber(18)
  @$pb.TagNumber(19)
  @$pb.TagNumber(20)
  @$pb.TagNumber(21)
  @$pb.TagNumber(22)
  @$pb.TagNumber(23)
  @$pb.TagNumber(24)
  @$pb.TagNumber(25)
  void clearPayload() => $_clearField($_whichOneof(0));

  @$pb.TagNumber(1)
  PeerHeader get header => $_getN(0);
  @$pb.TagNumber(1)
  set header(PeerHeader value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasHeader() => $_has(0);
  @$pb.TagNumber(1)
  void clearHeader() => $_clearField(1);
  @$pb.TagNumber(1)
  PeerHeader ensureHeader() => $_ensure(0);

  @$pb.TagNumber(16)
  Beacon get beacon => $_getN(1);
  @$pb.TagNumber(16)
  set beacon(Beacon value) => $_setField(16, value);
  @$pb.TagNumber(16)
  $core.bool hasBeacon() => $_has(1);
  @$pb.TagNumber(16)
  void clearBeacon() => $_clearField(16);
  @$pb.TagNumber(16)
  Beacon ensureBeacon() => $_ensure(1);

  @$pb.TagNumber(17)
  Ping get ping => $_getN(2);
  @$pb.TagNumber(17)
  set ping(Ping value) => $_setField(17, value);
  @$pb.TagNumber(17)
  $core.bool hasPing() => $_has(2);
  @$pb.TagNumber(17)
  void clearPing() => $_clearField(17);
  @$pb.TagNumber(17)
  Ping ensurePing() => $_ensure(2);

  @$pb.TagNumber(18)
  Pong get pong => $_getN(3);
  @$pb.TagNumber(18)
  set pong(Pong value) => $_setField(18, value);
  @$pb.TagNumber(18)
  $core.bool hasPong() => $_has(3);
  @$pb.TagNumber(18)
  void clearPong() => $_clearField(18);
  @$pb.TagNumber(18)
  Pong ensurePong() => $_ensure(3);

  @$pb.TagNumber(19)
  JoinGame get joinGame => $_getN(4);
  @$pb.TagNumber(19)
  set joinGame(JoinGame value) => $_setField(19, value);
  @$pb.TagNumber(19)
  $core.bool hasJoinGame() => $_has(4);
  @$pb.TagNumber(19)
  void clearJoinGame() => $_clearField(19);
  @$pb.TagNumber(19)
  JoinGame ensureJoinGame() => $_ensure(4);

  @$pb.TagNumber(20)
  ArmTouch get armTouch => $_getN(5);
  @$pb.TagNumber(20)
  set armTouch(ArmTouch value) => $_setField(20, value);
  @$pb.TagNumber(20)
  $core.bool hasArmTouch() => $_has(5);
  @$pb.TagNumber(20)
  void clearArmTouch() => $_clearField(20);
  @$pb.TagNumber(20)
  ArmTouch ensureArmTouch() => $_ensure(5);

  @$pb.TagNumber(21)
  SetColor get setColor => $_getN(6);
  @$pb.TagNumber(21)
  set setColor(SetColor value) => $_setField(21, value);
  @$pb.TagNumber(21)
  $core.bool hasSetColor() => $_has(6);
  @$pb.TagNumber(21)
  void clearSetColor() => $_clearField(21);
  @$pb.TagNumber(21)
  SetColor ensureSetColor() => $_ensure(6);

  @$pb.TagNumber(22)
  StopAll get stopAll => $_getN(7);
  @$pb.TagNumber(22)
  set stopAll(StopAll value) => $_setField(22, value);
  @$pb.TagNumber(22)
  $core.bool hasStopAll() => $_has(7);
  @$pb.TagNumber(22)
  void clearStopAll() => $_clearField(22);
  @$pb.TagNumber(22)
  StopAll ensureStopAll() => $_ensure(7);

  @$pb.TagNumber(23)
  SimulateTouch get simulateTouch => $_getN(8);
  @$pb.TagNumber(23)
  set simulateTouch(SimulateTouch value) => $_setField(23, value);
  @$pb.TagNumber(23)
  $core.bool hasSimulateTouch() => $_has(8);
  @$pb.TagNumber(23)
  void clearSimulateTouch() => $_clearField(23);
  @$pb.TagNumber(23)
  SimulateTouch ensureSimulateTouch() => $_ensure(8);

  @$pb.TagNumber(24)
  TouchEvent get touchEvent => $_getN(9);
  @$pb.TagNumber(24)
  set touchEvent(TouchEvent value) => $_setField(24, value);
  @$pb.TagNumber(24)
  $core.bool hasTouchEvent() => $_has(9);
  @$pb.TagNumber(24)
  void clearTouchEvent() => $_clearField(24);
  @$pb.TagNumber(24)
  TouchEvent ensureTouchEvent() => $_ensure(9);

  @$pb.TagNumber(25)
  TimeoutEvent get timeoutEvent => $_getN(10);
  @$pb.TagNumber(25)
  set timeoutEvent(TimeoutEvent value) => $_setField(25, value);
  @$pb.TagNumber(25)
  $core.bool hasTimeoutEvent() => $_has(10);
  @$pb.TagNumber(25)
  void clearTimeoutEvent() => $_clearField(25);
  @$pb.TagNumber(25)
  TimeoutEvent ensureTimeoutEvent() => $_ensure(10);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
