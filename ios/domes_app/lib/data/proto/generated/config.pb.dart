// This is a generated file - do not edit.
//
// Generated from config.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:core' as $core;

import 'package:protobuf/protobuf.dart' as $pb;

import 'config.pbenum.dart';

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

export 'config.pbenum.dart';

/// RGBW color (0-255 per channel)
class Color extends $pb.GeneratedMessage {
  factory Color({
    $core.int? r,
    $core.int? g,
    $core.int? b,
    $core.int? w,
  }) {
    final result = create();
    if (r != null) result.r = r;
    if (g != null) result.g = g;
    if (b != null) result.b = b;
    if (w != null) result.w = w;
    return result;
  }

  Color._();

  factory Color.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Color.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Color',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'r', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'g', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'b', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'w', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Color clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Color copyWith(void Function(Color) updates) =>
      super.copyWith((message) => updates(message as Color)) as Color;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Color create() => Color._();
  @$core.override
  Color createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Color getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Color>(create);
  static Color? _defaultInstance;

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

  @$pb.TagNumber(4)
  $core.int get w => $_getIZ(3);
  @$pb.TagNumber(4)
  set w($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasW() => $_has(3);
  @$pb.TagNumber(4)
  void clearW() => $_clearField(4);
}

/// Feature with its current state
class FeatureState extends $pb.GeneratedMessage {
  factory FeatureState({
    Feature? feature,
    $core.bool? enabled,
  }) {
    final result = create();
    if (feature != null) result.feature = feature;
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  FeatureState._();

  factory FeatureState.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory FeatureState.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'FeatureState',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<Feature>(1, _omitFieldNames ? '' : 'feature',
        enumValues: Feature.values)
    ..aOB(2, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  FeatureState clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  FeatureState copyWith(void Function(FeatureState) updates) =>
      super.copyWith((message) => updates(message as FeatureState))
          as FeatureState;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static FeatureState create() => FeatureState._();
  @$core.override
  FeatureState createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static FeatureState getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<FeatureState>(create);
  static FeatureState? _defaultInstance;

  @$pb.TagNumber(1)
  Feature get feature => $_getN(0);
  @$pb.TagNumber(1)
  set feature(Feature value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasFeature() => $_has(0);
  @$pb.TagNumber(1)
  void clearFeature() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get enabled => $_getBF(1);
  @$pb.TagNumber(2)
  set enabled($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasEnabled() => $_has(1);
  @$pb.TagNumber(2)
  void clearEnabled() => $_clearField(2);
}

/// Request messages
class ListFeaturesRequest extends $pb.GeneratedMessage {
  factory ListFeaturesRequest() => create();

  ListFeaturesRequest._();

  factory ListFeaturesRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ListFeaturesRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ListFeaturesRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ListFeaturesRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ListFeaturesRequest copyWith(void Function(ListFeaturesRequest) updates) =>
      super.copyWith((message) => updates(message as ListFeaturesRequest))
          as ListFeaturesRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ListFeaturesRequest create() => ListFeaturesRequest._();
  @$core.override
  ListFeaturesRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ListFeaturesRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ListFeaturesRequest>(create);
  static ListFeaturesRequest? _defaultInstance;
}

class SetFeatureRequest extends $pb.GeneratedMessage {
  factory SetFeatureRequest({
    Feature? feature,
    $core.bool? enabled,
  }) {
    final result = create();
    if (feature != null) result.feature = feature;
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  SetFeatureRequest._();

  factory SetFeatureRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetFeatureRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetFeatureRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<Feature>(1, _omitFieldNames ? '' : 'feature',
        enumValues: Feature.values)
    ..aOB(2, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetFeatureRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetFeatureRequest copyWith(void Function(SetFeatureRequest) updates) =>
      super.copyWith((message) => updates(message as SetFeatureRequest))
          as SetFeatureRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetFeatureRequest create() => SetFeatureRequest._();
  @$core.override
  SetFeatureRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetFeatureRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetFeatureRequest>(create);
  static SetFeatureRequest? _defaultInstance;

  @$pb.TagNumber(1)
  Feature get feature => $_getN(0);
  @$pb.TagNumber(1)
  set feature(Feature value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasFeature() => $_has(0);
  @$pb.TagNumber(1)
  void clearFeature() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get enabled => $_getBF(1);
  @$pb.TagNumber(2)
  set enabled($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasEnabled() => $_has(1);
  @$pb.TagNumber(2)
  void clearEnabled() => $_clearField(2);
}

/// LED pattern with parameters
class LedPattern extends $pb.GeneratedMessage {
  factory LedPattern({
    LedPatternType? type,
    Color? color,
    $core.Iterable<Color>? colors,
    $core.int? periodMs,
    $core.int? brightness,
  }) {
    final result = create();
    if (type != null) result.type = type;
    if (color != null) result.color = color;
    if (colors != null) result.colors.addAll(colors);
    if (periodMs != null) result.periodMs = periodMs;
    if (brightness != null) result.brightness = brightness;
    return result;
  }

  LedPattern._();

  factory LedPattern.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory LedPattern.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'LedPattern',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<LedPatternType>(1, _omitFieldNames ? '' : 'type',
        enumValues: LedPatternType.values)
    ..aOM<Color>(2, _omitFieldNames ? '' : 'color', subBuilder: Color.create)
    ..pPM<Color>(3, _omitFieldNames ? '' : 'colors', subBuilder: Color.create)
    ..aI(4, _omitFieldNames ? '' : 'periodMs', fieldType: $pb.PbFieldType.OU3)
    ..aI(5, _omitFieldNames ? '' : 'brightness', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  LedPattern clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  LedPattern copyWith(void Function(LedPattern) updates) =>
      super.copyWith((message) => updates(message as LedPattern)) as LedPattern;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static LedPattern create() => LedPattern._();
  @$core.override
  LedPattern createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static LedPattern getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<LedPattern>(create);
  static LedPattern? _defaultInstance;

  @$pb.TagNumber(1)
  LedPatternType get type => $_getN(0);
  @$pb.TagNumber(1)
  set type(LedPatternType value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasType() => $_has(0);
  @$pb.TagNumber(1)
  void clearType() => $_clearField(1);

  @$pb.TagNumber(2)
  Color get color => $_getN(1);
  @$pb.TagNumber(2)
  set color(Color value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasColor() => $_has(1);
  @$pb.TagNumber(2)
  void clearColor() => $_clearField(2);
  @$pb.TagNumber(2)
  Color ensureColor() => $_ensure(1);

  @$pb.TagNumber(3)
  $pb.PbList<Color> get colors => $_getList(2);

  @$pb.TagNumber(4)
  $core.int get periodMs => $_getIZ(3);
  @$pb.TagNumber(4)
  set periodMs($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasPeriodMs() => $_has(3);
  @$pb.TagNumber(4)
  void clearPeriodMs() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get brightness => $_getIZ(4);
  @$pb.TagNumber(5)
  set brightness($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasBrightness() => $_has(4);
  @$pb.TagNumber(5)
  void clearBrightness() => $_clearField(5);
}

class SetLedPatternRequest extends $pb.GeneratedMessage {
  factory SetLedPatternRequest({
    LedPattern? pattern,
  }) {
    final result = create();
    if (pattern != null) result.pattern = pattern;
    return result;
  }

  SetLedPatternRequest._();

  factory SetLedPatternRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetLedPatternRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetLedPatternRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOM<LedPattern>(1, _omitFieldNames ? '' : 'pattern',
        subBuilder: LedPattern.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetLedPatternRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetLedPatternRequest copyWith(void Function(SetLedPatternRequest) updates) =>
      super.copyWith((message) => updates(message as SetLedPatternRequest))
          as SetLedPatternRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetLedPatternRequest create() => SetLedPatternRequest._();
  @$core.override
  SetLedPatternRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetLedPatternRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetLedPatternRequest>(create);
  static SetLedPatternRequest? _defaultInstance;

  @$pb.TagNumber(1)
  LedPattern get pattern => $_getN(0);
  @$pb.TagNumber(1)
  set pattern(LedPattern value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasPattern() => $_has(0);
  @$pb.TagNumber(1)
  void clearPattern() => $_clearField(1);
  @$pb.TagNumber(1)
  LedPattern ensurePattern() => $_ensure(0);
}

class GetLedPatternRequest extends $pb.GeneratedMessage {
  factory GetLedPatternRequest() => create();

  GetLedPatternRequest._();

  factory GetLedPatternRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetLedPatternRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetLedPatternRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetLedPatternRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetLedPatternRequest copyWith(void Function(GetLedPatternRequest) updates) =>
      super.copyWith((message) => updates(message as GetLedPatternRequest))
          as GetLedPatternRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetLedPatternRequest create() => GetLedPatternRequest._();
  @$core.override
  GetLedPatternRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetLedPatternRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetLedPatternRequest>(create);
  static GetLedPatternRequest? _defaultInstance;
}

/// Response messages
class ListFeaturesResponse extends $pb.GeneratedMessage {
  factory ListFeaturesResponse({
    $core.Iterable<FeatureState>? features,
    $core.int? podId,
  }) {
    final result = create();
    if (features != null) result.features.addAll(features);
    if (podId != null) result.podId = podId;
    return result;
  }

  ListFeaturesResponse._();

  factory ListFeaturesResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ListFeaturesResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ListFeaturesResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..pPM<FeatureState>(1, _omitFieldNames ? '' : 'features',
        subBuilder: FeatureState.create)
    ..aI(2, _omitFieldNames ? '' : 'podId', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ListFeaturesResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ListFeaturesResponse copyWith(void Function(ListFeaturesResponse) updates) =>
      super.copyWith((message) => updates(message as ListFeaturesResponse))
          as ListFeaturesResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ListFeaturesResponse create() => ListFeaturesResponse._();
  @$core.override
  ListFeaturesResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ListFeaturesResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ListFeaturesResponse>(create);
  static ListFeaturesResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $pb.PbList<FeatureState> get features => $_getList(0);

  @$pb.TagNumber(2)
  $core.int get podId => $_getIZ(1);
  @$pb.TagNumber(2)
  set podId($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasPodId() => $_has(1);
  @$pb.TagNumber(2)
  void clearPodId() => $_clearField(2);
}

class SetFeatureResponse extends $pb.GeneratedMessage {
  factory SetFeatureResponse({
    FeatureState? feature,
  }) {
    final result = create();
    if (feature != null) result.feature = feature;
    return result;
  }

  SetFeatureResponse._();

  factory SetFeatureResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetFeatureResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetFeatureResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOM<FeatureState>(1, _omitFieldNames ? '' : 'feature',
        subBuilder: FeatureState.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetFeatureResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetFeatureResponse copyWith(void Function(SetFeatureResponse) updates) =>
      super.copyWith((message) => updates(message as SetFeatureResponse))
          as SetFeatureResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetFeatureResponse create() => SetFeatureResponse._();
  @$core.override
  SetFeatureResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetFeatureResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetFeatureResponse>(create);
  static SetFeatureResponse? _defaultInstance;

  @$pb.TagNumber(1)
  FeatureState get feature => $_getN(0);
  @$pb.TagNumber(1)
  set feature(FeatureState value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasFeature() => $_has(0);
  @$pb.TagNumber(1)
  void clearFeature() => $_clearField(1);
  @$pb.TagNumber(1)
  FeatureState ensureFeature() => $_ensure(0);
}

class SetLedPatternResponse extends $pb.GeneratedMessage {
  factory SetLedPatternResponse({
    LedPattern? pattern,
  }) {
    final result = create();
    if (pattern != null) result.pattern = pattern;
    return result;
  }

  SetLedPatternResponse._();

  factory SetLedPatternResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetLedPatternResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetLedPatternResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOM<LedPattern>(1, _omitFieldNames ? '' : 'pattern',
        subBuilder: LedPattern.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetLedPatternResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetLedPatternResponse copyWith(
          void Function(SetLedPatternResponse) updates) =>
      super.copyWith((message) => updates(message as SetLedPatternResponse))
          as SetLedPatternResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetLedPatternResponse create() => SetLedPatternResponse._();
  @$core.override
  SetLedPatternResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetLedPatternResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetLedPatternResponse>(create);
  static SetLedPatternResponse? _defaultInstance;

  @$pb.TagNumber(1)
  LedPattern get pattern => $_getN(0);
  @$pb.TagNumber(1)
  set pattern(LedPattern value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasPattern() => $_has(0);
  @$pb.TagNumber(1)
  void clearPattern() => $_clearField(1);
  @$pb.TagNumber(1)
  LedPattern ensurePattern() => $_ensure(0);
}

class GetLedPatternResponse extends $pb.GeneratedMessage {
  factory GetLedPatternResponse({
    LedPattern? pattern,
  }) {
    final result = create();
    if (pattern != null) result.pattern = pattern;
    return result;
  }

  GetLedPatternResponse._();

  factory GetLedPatternResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetLedPatternResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetLedPatternResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOM<LedPattern>(1, _omitFieldNames ? '' : 'pattern',
        subBuilder: LedPattern.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetLedPatternResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetLedPatternResponse copyWith(
          void Function(GetLedPatternResponse) updates) =>
      super.copyWith((message) => updates(message as GetLedPatternResponse))
          as GetLedPatternResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetLedPatternResponse create() => GetLedPatternResponse._();
  @$core.override
  GetLedPatternResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetLedPatternResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetLedPatternResponse>(create);
  static GetLedPatternResponse? _defaultInstance;

  @$pb.TagNumber(1)
  LedPattern get pattern => $_getN(0);
  @$pb.TagNumber(1)
  set pattern(LedPattern value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasPattern() => $_has(0);
  @$pb.TagNumber(1)
  void clearPattern() => $_clearField(1);
  @$pb.TagNumber(1)
  LedPattern ensurePattern() => $_ensure(0);
}

/// IMU triage mode messages
class SetImuTriageRequest extends $pb.GeneratedMessage {
  factory SetImuTriageRequest({
    $core.bool? enabled,
  }) {
    final result = create();
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  SetImuTriageRequest._();

  factory SetImuTriageRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetImuTriageRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetImuTriageRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetImuTriageRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetImuTriageRequest copyWith(void Function(SetImuTriageRequest) updates) =>
      super.copyWith((message) => updates(message as SetImuTriageRequest))
          as SetImuTriageRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetImuTriageRequest create() => SetImuTriageRequest._();
  @$core.override
  SetImuTriageRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetImuTriageRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetImuTriageRequest>(create);
  static SetImuTriageRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get enabled => $_getBF(0);
  @$pb.TagNumber(1)
  set enabled($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasEnabled() => $_has(0);
  @$pb.TagNumber(1)
  void clearEnabled() => $_clearField(1);
}

class SetImuTriageResponse extends $pb.GeneratedMessage {
  factory SetImuTriageResponse({
    $core.bool? enabled,
  }) {
    final result = create();
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  SetImuTriageResponse._();

  factory SetImuTriageResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetImuTriageResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetImuTriageResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetImuTriageResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetImuTriageResponse copyWith(void Function(SetImuTriageResponse) updates) =>
      super.copyWith((message) => updates(message as SetImuTriageResponse))
          as SetImuTriageResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetImuTriageResponse create() => SetImuTriageResponse._();
  @$core.override
  SetImuTriageResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetImuTriageResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetImuTriageResponse>(create);
  static SetImuTriageResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get enabled => $_getBF(0);
  @$pb.TagNumber(1)
  set enabled($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasEnabled() => $_has(0);
  @$pb.TagNumber(1)
  void clearEnabled() => $_clearField(1);
}

/// System mode messages
class GetModeRequest extends $pb.GeneratedMessage {
  factory GetModeRequest() => create();

  GetModeRequest._();

  factory GetModeRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetModeRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetModeRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetModeRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetModeRequest copyWith(void Function(GetModeRequest) updates) =>
      super.copyWith((message) => updates(message as GetModeRequest))
          as GetModeRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetModeRequest create() => GetModeRequest._();
  @$core.override
  GetModeRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetModeRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetModeRequest>(create);
  static GetModeRequest? _defaultInstance;
}

class GetModeResponse extends $pb.GeneratedMessage {
  factory GetModeResponse({
    SystemMode? mode,
    $core.int? timeInModeMs,
  }) {
    final result = create();
    if (mode != null) result.mode = mode;
    if (timeInModeMs != null) result.timeInModeMs = timeInModeMs;
    return result;
  }

  GetModeResponse._();

  factory GetModeResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetModeResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetModeResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<SystemMode>(1, _omitFieldNames ? '' : 'mode',
        enumValues: SystemMode.values)
    ..aI(2, _omitFieldNames ? '' : 'timeInModeMs',
        fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetModeResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetModeResponse copyWith(void Function(GetModeResponse) updates) =>
      super.copyWith((message) => updates(message as GetModeResponse))
          as GetModeResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetModeResponse create() => GetModeResponse._();
  @$core.override
  GetModeResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetModeResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetModeResponse>(create);
  static GetModeResponse? _defaultInstance;

  @$pb.TagNumber(1)
  SystemMode get mode => $_getN(0);
  @$pb.TagNumber(1)
  set mode(SystemMode value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasMode() => $_has(0);
  @$pb.TagNumber(1)
  void clearMode() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get timeInModeMs => $_getIZ(1);
  @$pb.TagNumber(2)
  set timeInModeMs($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTimeInModeMs() => $_has(1);
  @$pb.TagNumber(2)
  void clearTimeInModeMs() => $_clearField(2);
}

class SetModeRequest extends $pb.GeneratedMessage {
  factory SetModeRequest({
    SystemMode? mode,
  }) {
    final result = create();
    if (mode != null) result.mode = mode;
    return result;
  }

  SetModeRequest._();

  factory SetModeRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetModeRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetModeRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<SystemMode>(1, _omitFieldNames ? '' : 'mode',
        enumValues: SystemMode.values)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetModeRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetModeRequest copyWith(void Function(SetModeRequest) updates) =>
      super.copyWith((message) => updates(message as SetModeRequest))
          as SetModeRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetModeRequest create() => SetModeRequest._();
  @$core.override
  SetModeRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetModeRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetModeRequest>(create);
  static SetModeRequest? _defaultInstance;

  @$pb.TagNumber(1)
  SystemMode get mode => $_getN(0);
  @$pb.TagNumber(1)
  set mode(SystemMode value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasMode() => $_has(0);
  @$pb.TagNumber(1)
  void clearMode() => $_clearField(1);
}

class SetModeResponse extends $pb.GeneratedMessage {
  factory SetModeResponse({
    SystemMode? mode,
    $core.bool? transitionOk,
  }) {
    final result = create();
    if (mode != null) result.mode = mode;
    if (transitionOk != null) result.transitionOk = transitionOk;
    return result;
  }

  SetModeResponse._();

  factory SetModeResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetModeResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetModeResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<SystemMode>(1, _omitFieldNames ? '' : 'mode',
        enumValues: SystemMode.values)
    ..aOB(2, _omitFieldNames ? '' : 'transitionOk')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetModeResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetModeResponse copyWith(void Function(SetModeResponse) updates) =>
      super.copyWith((message) => updates(message as SetModeResponse))
          as SetModeResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetModeResponse create() => SetModeResponse._();
  @$core.override
  SetModeResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetModeResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetModeResponse>(create);
  static SetModeResponse? _defaultInstance;

  @$pb.TagNumber(1)
  SystemMode get mode => $_getN(0);
  @$pb.TagNumber(1)
  set mode(SystemMode value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasMode() => $_has(0);
  @$pb.TagNumber(1)
  void clearMode() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get transitionOk => $_getBF(1);
  @$pb.TagNumber(2)
  set transitionOk($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTransitionOk() => $_has(1);
  @$pb.TagNumber(2)
  void clearTransitionOk() => $_clearField(2);
}

class GetSystemInfoRequest extends $pb.GeneratedMessage {
  factory GetSystemInfoRequest() => create();

  GetSystemInfoRequest._();

  factory GetSystemInfoRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetSystemInfoRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetSystemInfoRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetSystemInfoRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetSystemInfoRequest copyWith(void Function(GetSystemInfoRequest) updates) =>
      super.copyWith((message) => updates(message as GetSystemInfoRequest))
          as GetSystemInfoRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetSystemInfoRequest create() => GetSystemInfoRequest._();
  @$core.override
  GetSystemInfoRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetSystemInfoRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetSystemInfoRequest>(create);
  static GetSystemInfoRequest? _defaultInstance;
}

class GetSystemInfoResponse extends $pb.GeneratedMessage {
  factory GetSystemInfoResponse({
    $core.String? firmwareVersion,
    $core.int? uptimeS,
    $core.int? freeHeap,
    $core.int? bootCount,
    SystemMode? mode,
    $core.int? featureMask,
    $core.int? podId,
  }) {
    final result = create();
    if (firmwareVersion != null) result.firmwareVersion = firmwareVersion;
    if (uptimeS != null) result.uptimeS = uptimeS;
    if (freeHeap != null) result.freeHeap = freeHeap;
    if (bootCount != null) result.bootCount = bootCount;
    if (mode != null) result.mode = mode;
    if (featureMask != null) result.featureMask = featureMask;
    if (podId != null) result.podId = podId;
    return result;
  }

  GetSystemInfoResponse._();

  factory GetSystemInfoResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetSystemInfoResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetSystemInfoResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'firmwareVersion')
    ..aI(2, _omitFieldNames ? '' : 'uptimeS', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'freeHeap', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'bootCount', fieldType: $pb.PbFieldType.OU3)
    ..aE<SystemMode>(5, _omitFieldNames ? '' : 'mode',
        enumValues: SystemMode.values)
    ..aI(6, _omitFieldNames ? '' : 'featureMask',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(7, _omitFieldNames ? '' : 'podId', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetSystemInfoResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetSystemInfoResponse copyWith(
          void Function(GetSystemInfoResponse) updates) =>
      super.copyWith((message) => updates(message as GetSystemInfoResponse))
          as GetSystemInfoResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetSystemInfoResponse create() => GetSystemInfoResponse._();
  @$core.override
  GetSystemInfoResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetSystemInfoResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetSystemInfoResponse>(create);
  static GetSystemInfoResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get firmwareVersion => $_getSZ(0);
  @$pb.TagNumber(1)
  set firmwareVersion($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasFirmwareVersion() => $_has(0);
  @$pb.TagNumber(1)
  void clearFirmwareVersion() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get uptimeS => $_getIZ(1);
  @$pb.TagNumber(2)
  set uptimeS($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasUptimeS() => $_has(1);
  @$pb.TagNumber(2)
  void clearUptimeS() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get freeHeap => $_getIZ(2);
  @$pb.TagNumber(3)
  set freeHeap($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasFreeHeap() => $_has(2);
  @$pb.TagNumber(3)
  void clearFreeHeap() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get bootCount => $_getIZ(3);
  @$pb.TagNumber(4)
  set bootCount($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasBootCount() => $_has(3);
  @$pb.TagNumber(4)
  void clearBootCount() => $_clearField(4);

  @$pb.TagNumber(5)
  SystemMode get mode => $_getN(4);
  @$pb.TagNumber(5)
  set mode(SystemMode value) => $_setField(5, value);
  @$pb.TagNumber(5)
  $core.bool hasMode() => $_has(4);
  @$pb.TagNumber(5)
  void clearMode() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.int get featureMask => $_getIZ(5);
  @$pb.TagNumber(6)
  set featureMask($core.int value) => $_setUnsignedInt32(5, value);
  @$pb.TagNumber(6)
  $core.bool hasFeatureMask() => $_has(5);
  @$pb.TagNumber(6)
  void clearFeatureMask() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.int get podId => $_getIZ(6);
  @$pb.TagNumber(7)
  set podId($core.int value) => $_setUnsignedInt32(6, value);
  @$pb.TagNumber(7)
  $core.bool hasPodId() => $_has(6);
  @$pb.TagNumber(7)
  void clearPodId() => $_clearField(7);
}

/// Set pod ID (persisted to NVS)
class SetPodIdRequest extends $pb.GeneratedMessage {
  factory SetPodIdRequest({
    $core.int? podId,
  }) {
    final result = create();
    if (podId != null) result.podId = podId;
    return result;
  }

  SetPodIdRequest._();

  factory SetPodIdRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetPodIdRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetPodIdRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'podId', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetPodIdRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetPodIdRequest copyWith(void Function(SetPodIdRequest) updates) =>
      super.copyWith((message) => updates(message as SetPodIdRequest))
          as SetPodIdRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetPodIdRequest create() => SetPodIdRequest._();
  @$core.override
  SetPodIdRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetPodIdRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetPodIdRequest>(create);
  static SetPodIdRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get podId => $_getIZ(0);
  @$pb.TagNumber(1)
  set podId($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasPodId() => $_has(0);
  @$pb.TagNumber(1)
  void clearPodId() => $_clearField(1);
}

class SetPodIdResponse extends $pb.GeneratedMessage {
  factory SetPodIdResponse({
    $core.int? podId,
  }) {
    final result = create();
    if (podId != null) result.podId = podId;
    return result;
  }

  SetPodIdResponse._();

  factory SetPodIdResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetPodIdResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetPodIdResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'podId', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetPodIdResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetPodIdResponse copyWith(void Function(SetPodIdResponse) updates) =>
      super.copyWith((message) => updates(message as SetPodIdResponse))
          as SetPodIdResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetPodIdResponse create() => SetPodIdResponse._();
  @$core.override
  SetPodIdResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetPodIdResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetPodIdResponse>(create);
  static SetPodIdResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get podId => $_getIZ(0);
  @$pb.TagNumber(1)
  set podId($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasPodId() => $_has(0);
  @$pb.TagNumber(1)
  void clearPodId() => $_clearField(1);
}

/// FreeRTOS task health snapshot
class TaskHealth extends $pb.GeneratedMessage {
  factory TaskHealth({
    $core.String? name,
    $core.int? stackHighWater,
    $core.int? priority,
    $core.int? core,
  }) {
    final result = create();
    if (name != null) result.name = name;
    if (stackHighWater != null) result.stackHighWater = stackHighWater;
    if (priority != null) result.priority = priority;
    if (core != null) result.core = core;
    return result;
  }

  TaskHealth._();

  factory TaskHealth.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TaskHealth.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TaskHealth',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'name')
    ..aI(2, _omitFieldNames ? '' : 'stackHighWater',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'priority', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'core', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TaskHealth clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TaskHealth copyWith(void Function(TaskHealth) updates) =>
      super.copyWith((message) => updates(message as TaskHealth)) as TaskHealth;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TaskHealth create() => TaskHealth._();
  @$core.override
  TaskHealth createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TaskHealth getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TaskHealth>(create);
  static TaskHealth? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get stackHighWater => $_getIZ(1);
  @$pb.TagNumber(2)
  set stackHighWater($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasStackHighWater() => $_has(1);
  @$pb.TagNumber(2)
  void clearStackHighWater() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get priority => $_getIZ(2);
  @$pb.TagNumber(3)
  set priority($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasPriority() => $_has(2);
  @$pb.TagNumber(3)
  void clearPriority() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get core => $_getIZ(3);
  @$pb.TagNumber(4)
  set core($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasCore() => $_has(3);
  @$pb.TagNumber(4)
  void clearCore() => $_clearField(4);
}

/// System health diagnostics
class GetHealthRequest extends $pb.GeneratedMessage {
  factory GetHealthRequest() => create();

  GetHealthRequest._();

  factory GetHealthRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetHealthRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetHealthRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetHealthRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetHealthRequest copyWith(void Function(GetHealthRequest) updates) =>
      super.copyWith((message) => updates(message as GetHealthRequest))
          as GetHealthRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetHealthRequest create() => GetHealthRequest._();
  @$core.override
  GetHealthRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetHealthRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetHealthRequest>(create);
  static GetHealthRequest? _defaultInstance;
}

class GetHealthResponse extends $pb.GeneratedMessage {
  factory GetHealthResponse({
    $core.int? freeHeap,
    $core.int? minFreeHeap,
    $core.int? uptimeSeconds,
    $core.int? wifiRssi,
    $core.Iterable<TaskHealth>? tasks,
  }) {
    final result = create();
    if (freeHeap != null) result.freeHeap = freeHeap;
    if (minFreeHeap != null) result.minFreeHeap = minFreeHeap;
    if (uptimeSeconds != null) result.uptimeSeconds = uptimeSeconds;
    if (wifiRssi != null) result.wifiRssi = wifiRssi;
    if (tasks != null) result.tasks.addAll(tasks);
    return result;
  }

  GetHealthResponse._();

  factory GetHealthResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetHealthResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetHealthResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'freeHeap', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'minFreeHeap',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'uptimeSeconds',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'wifiRssi')
    ..pPM<TaskHealth>(5, _omitFieldNames ? '' : 'tasks',
        subBuilder: TaskHealth.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetHealthResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetHealthResponse copyWith(void Function(GetHealthResponse) updates) =>
      super.copyWith((message) => updates(message as GetHealthResponse))
          as GetHealthResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetHealthResponse create() => GetHealthResponse._();
  @$core.override
  GetHealthResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetHealthResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetHealthResponse>(create);
  static GetHealthResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get freeHeap => $_getIZ(0);
  @$pb.TagNumber(1)
  set freeHeap($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasFreeHeap() => $_has(0);
  @$pb.TagNumber(1)
  void clearFreeHeap() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get minFreeHeap => $_getIZ(1);
  @$pb.TagNumber(2)
  set minFreeHeap($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasMinFreeHeap() => $_has(1);
  @$pb.TagNumber(2)
  void clearMinFreeHeap() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get uptimeSeconds => $_getIZ(2);
  @$pb.TagNumber(3)
  set uptimeSeconds($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasUptimeSeconds() => $_has(2);
  @$pb.TagNumber(3)
  void clearUptimeSeconds() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get wifiRssi => $_getIZ(3);
  @$pb.TagNumber(4)
  set wifiRssi($core.int value) => $_setSignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasWifiRssi() => $_has(3);
  @$pb.TagNumber(4)
  void clearWifiRssi() => $_clearField(4);

  @$pb.TagNumber(5)
  $pb.PbList<TaskHealth> get tasks => $_getList(4);
}

/// ESP-NOW peer info
class EspNowPeer extends $pb.GeneratedMessage {
  factory EspNowPeer({
    $core.List<$core.int>? mac,
    $core.int? rssi,
    $core.int? lastSeenMs,
  }) {
    final result = create();
    if (mac != null) result.mac = mac;
    if (rssi != null) result.rssi = rssi;
    if (lastSeenMs != null) result.lastSeenMs = lastSeenMs;
    return result;
  }

  EspNowPeer._();

  factory EspNowPeer.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory EspNowPeer.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'EspNowPeer',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'mac', $pb.PbFieldType.OY)
    ..aI(2, _omitFieldNames ? '' : 'rssi')
    ..aI(3, _omitFieldNames ? '' : 'lastSeenMs', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowPeer clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowPeer copyWith(void Function(EspNowPeer) updates) =>
      super.copyWith((message) => updates(message as EspNowPeer)) as EspNowPeer;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static EspNowPeer create() => EspNowPeer._();
  @$core.override
  EspNowPeer createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static EspNowPeer getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<EspNowPeer>(create);
  static EspNowPeer? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get mac => $_getN(0);
  @$pb.TagNumber(1)
  set mac($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasMac() => $_has(0);
  @$pb.TagNumber(1)
  void clearMac() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get rssi => $_getIZ(1);
  @$pb.TagNumber(2)
  set rssi($core.int value) => $_setSignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasRssi() => $_has(1);
  @$pb.TagNumber(2)
  void clearRssi() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get lastSeenMs => $_getIZ(2);
  @$pb.TagNumber(3)
  set lastSeenMs($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasLastSeenMs() => $_has(2);
  @$pb.TagNumber(3)
  void clearLastSeenMs() => $_clearField(3);
}

/// ESP-NOW subsystem status
class GetEspNowStatusRequest extends $pb.GeneratedMessage {
  factory GetEspNowStatusRequest() => create();

  GetEspNowStatusRequest._();

  factory GetEspNowStatusRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetEspNowStatusRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetEspNowStatusRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetEspNowStatusRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetEspNowStatusRequest copyWith(
          void Function(GetEspNowStatusRequest) updates) =>
      super.copyWith((message) => updates(message as GetEspNowStatusRequest))
          as GetEspNowStatusRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetEspNowStatusRequest create() => GetEspNowStatusRequest._();
  @$core.override
  GetEspNowStatusRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetEspNowStatusRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetEspNowStatusRequest>(create);
  static GetEspNowStatusRequest? _defaultInstance;
}

class GetEspNowStatusResponse extends $pb.GeneratedMessage {
  factory GetEspNowStatusResponse({
    $core.int? peerCount,
    $core.int? channel,
    $core.int? txCount,
    $core.int? rxCount,
    $core.int? txFailCount,
    $core.int? lastRttUs,
    $core.String? discoveryState,
    $core.Iterable<EspNowPeer>? peers,
  }) {
    final result = create();
    if (peerCount != null) result.peerCount = peerCount;
    if (channel != null) result.channel = channel;
    if (txCount != null) result.txCount = txCount;
    if (rxCount != null) result.rxCount = rxCount;
    if (txFailCount != null) result.txFailCount = txFailCount;
    if (lastRttUs != null) result.lastRttUs = lastRttUs;
    if (discoveryState != null) result.discoveryState = discoveryState;
    if (peers != null) result.peers.addAll(peers);
    return result;
  }

  GetEspNowStatusResponse._();

  factory GetEspNowStatusResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetEspNowStatusResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetEspNowStatusResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'peerCount', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'channel', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'txCount', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'rxCount', fieldType: $pb.PbFieldType.OU3)
    ..aI(5, _omitFieldNames ? '' : 'txFailCount',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(6, _omitFieldNames ? '' : 'lastRttUs', fieldType: $pb.PbFieldType.OU3)
    ..aOS(7, _omitFieldNames ? '' : 'discoveryState')
    ..pPM<EspNowPeer>(8, _omitFieldNames ? '' : 'peers',
        subBuilder: EspNowPeer.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetEspNowStatusResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetEspNowStatusResponse copyWith(
          void Function(GetEspNowStatusResponse) updates) =>
      super.copyWith((message) => updates(message as GetEspNowStatusResponse))
          as GetEspNowStatusResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetEspNowStatusResponse create() => GetEspNowStatusResponse._();
  @$core.override
  GetEspNowStatusResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetEspNowStatusResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetEspNowStatusResponse>(create);
  static GetEspNowStatusResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get peerCount => $_getIZ(0);
  @$pb.TagNumber(1)
  set peerCount($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasPeerCount() => $_has(0);
  @$pb.TagNumber(1)
  void clearPeerCount() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get channel => $_getIZ(1);
  @$pb.TagNumber(2)
  set channel($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasChannel() => $_has(1);
  @$pb.TagNumber(2)
  void clearChannel() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get txCount => $_getIZ(2);
  @$pb.TagNumber(3)
  set txCount($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasTxCount() => $_has(2);
  @$pb.TagNumber(3)
  void clearTxCount() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get rxCount => $_getIZ(3);
  @$pb.TagNumber(4)
  set rxCount($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasRxCount() => $_has(3);
  @$pb.TagNumber(4)
  void clearRxCount() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get txFailCount => $_getIZ(4);
  @$pb.TagNumber(5)
  set txFailCount($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasTxFailCount() => $_has(4);
  @$pb.TagNumber(5)
  void clearTxFailCount() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.int get lastRttUs => $_getIZ(5);
  @$pb.TagNumber(6)
  set lastRttUs($core.int value) => $_setUnsignedInt32(5, value);
  @$pb.TagNumber(6)
  $core.bool hasLastRttUs() => $_has(5);
  @$pb.TagNumber(6)
  void clearLastRttUs() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.String get discoveryState => $_getSZ(6);
  @$pb.TagNumber(7)
  set discoveryState($core.String value) => $_setString(6, value);
  @$pb.TagNumber(7)
  $core.bool hasDiscoveryState() => $_has(6);
  @$pb.TagNumber(7)
  void clearDiscoveryState() => $_clearField(7);

  @$pb.TagNumber(8)
  $pb.PbList<EspNowPeer> get peers => $_getList(7);
}

/// ESP-NOW latency benchmark
class EspNowBenchRequest extends $pb.GeneratedMessage {
  factory EspNowBenchRequest({
    $core.int? rounds,
  }) {
    final result = create();
    if (rounds != null) result.rounds = rounds;
    return result;
  }

  EspNowBenchRequest._();

  factory EspNowBenchRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory EspNowBenchRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'EspNowBenchRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'rounds', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowBenchRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowBenchRequest copyWith(void Function(EspNowBenchRequest) updates) =>
      super.copyWith((message) => updates(message as EspNowBenchRequest))
          as EspNowBenchRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static EspNowBenchRequest create() => EspNowBenchRequest._();
  @$core.override
  EspNowBenchRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static EspNowBenchRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<EspNowBenchRequest>(create);
  static EspNowBenchRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get rounds => $_getIZ(0);
  @$pb.TagNumber(1)
  set rounds($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRounds() => $_has(0);
  @$pb.TagNumber(1)
  void clearRounds() => $_clearField(1);
}

class EspNowBenchResponse extends $pb.GeneratedMessage {
  factory EspNowBenchResponse({
    $core.int? roundsCompleted,
    $core.int? roundsFailed,
    $core.int? minRttUs,
    $core.int? maxRttUs,
    $core.int? meanRttUs,
    $core.int? p50RttUs,
    $core.int? p95RttUs,
    $core.int? p99RttUs,
  }) {
    final result = create();
    if (roundsCompleted != null) result.roundsCompleted = roundsCompleted;
    if (roundsFailed != null) result.roundsFailed = roundsFailed;
    if (minRttUs != null) result.minRttUs = minRttUs;
    if (maxRttUs != null) result.maxRttUs = maxRttUs;
    if (meanRttUs != null) result.meanRttUs = meanRttUs;
    if (p50RttUs != null) result.p50RttUs = p50RttUs;
    if (p95RttUs != null) result.p95RttUs = p95RttUs;
    if (p99RttUs != null) result.p99RttUs = p99RttUs;
    return result;
  }

  EspNowBenchResponse._();

  factory EspNowBenchResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory EspNowBenchResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'EspNowBenchResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'roundsCompleted',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'roundsFailed',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'minRttUs', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'maxRttUs', fieldType: $pb.PbFieldType.OU3)
    ..aI(5, _omitFieldNames ? '' : 'meanRttUs', fieldType: $pb.PbFieldType.OU3)
    ..aI(6, _omitFieldNames ? '' : 'p50RttUs', fieldType: $pb.PbFieldType.OU3)
    ..aI(7, _omitFieldNames ? '' : 'p95RttUs', fieldType: $pb.PbFieldType.OU3)
    ..aI(8, _omitFieldNames ? '' : 'p99RttUs', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowBenchResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  EspNowBenchResponse copyWith(void Function(EspNowBenchResponse) updates) =>
      super.copyWith((message) => updates(message as EspNowBenchResponse))
          as EspNowBenchResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static EspNowBenchResponse create() => EspNowBenchResponse._();
  @$core.override
  EspNowBenchResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static EspNowBenchResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<EspNowBenchResponse>(create);
  static EspNowBenchResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get roundsCompleted => $_getIZ(0);
  @$pb.TagNumber(1)
  set roundsCompleted($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRoundsCompleted() => $_has(0);
  @$pb.TagNumber(1)
  void clearRoundsCompleted() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get roundsFailed => $_getIZ(1);
  @$pb.TagNumber(2)
  set roundsFailed($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasRoundsFailed() => $_has(1);
  @$pb.TagNumber(2)
  void clearRoundsFailed() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get minRttUs => $_getIZ(2);
  @$pb.TagNumber(3)
  set minRttUs($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasMinRttUs() => $_has(2);
  @$pb.TagNumber(3)
  void clearMinRttUs() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get maxRttUs => $_getIZ(3);
  @$pb.TagNumber(4)
  set maxRttUs($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasMaxRttUs() => $_has(3);
  @$pb.TagNumber(4)
  void clearMaxRttUs() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get meanRttUs => $_getIZ(4);
  @$pb.TagNumber(5)
  set meanRttUs($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasMeanRttUs() => $_has(4);
  @$pb.TagNumber(5)
  void clearMeanRttUs() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.int get p50RttUs => $_getIZ(5);
  @$pb.TagNumber(6)
  set p50RttUs($core.int value) => $_setUnsignedInt32(5, value);
  @$pb.TagNumber(6)
  $core.bool hasP50RttUs() => $_has(5);
  @$pb.TagNumber(6)
  void clearP50RttUs() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.int get p95RttUs => $_getIZ(6);
  @$pb.TagNumber(7)
  set p95RttUs($core.int value) => $_setUnsignedInt32(6, value);
  @$pb.TagNumber(7)
  $core.bool hasP95RttUs() => $_has(6);
  @$pb.TagNumber(7)
  void clearP95RttUs() => $_clearField(7);

  @$pb.TagNumber(8)
  $core.int get p99RttUs => $_getIZ(7);
  @$pb.TagNumber(8)
  set p99RttUs($core.int value) => $_setUnsignedInt32(7, value);
  @$pb.TagNumber(8)
  $core.bool hasP99RttUs() => $_has(7);
  @$pb.TagNumber(8)
  void clearP99RttUs() => $_clearField(8);
}

/// Crash dump stored in NVS after a panic
class GetCrashDumpRequest extends $pb.GeneratedMessage {
  factory GetCrashDumpRequest() => create();

  GetCrashDumpRequest._();

  factory GetCrashDumpRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetCrashDumpRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetCrashDumpRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetCrashDumpRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetCrashDumpRequest copyWith(void Function(GetCrashDumpRequest) updates) =>
      super.copyWith((message) => updates(message as GetCrashDumpRequest))
          as GetCrashDumpRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetCrashDumpRequest create() => GetCrashDumpRequest._();
  @$core.override
  GetCrashDumpRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetCrashDumpRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetCrashDumpRequest>(create);
  static GetCrashDumpRequest? _defaultInstance;
}

class CrashDumpResponse extends $pb.GeneratedMessage {
  factory CrashDumpResponse({
    $core.bool? hasDump,
    $core.String? reason,
    $core.String? taskName,
    $core.int? uptimeS,
    $core.int? freeHeap,
    $core.Iterable<$core.int>? backtrace,
    $core.int? timestamp,
  }) {
    final result = create();
    if (hasDump != null) result.hasDump = hasDump;
    if (reason != null) result.reason = reason;
    if (taskName != null) result.taskName = taskName;
    if (uptimeS != null) result.uptimeS = uptimeS;
    if (freeHeap != null) result.freeHeap = freeHeap;
    if (backtrace != null) result.backtrace.addAll(backtrace);
    if (timestamp != null) result.timestamp = timestamp;
    return result;
  }

  CrashDumpResponse._();

  factory CrashDumpResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CrashDumpResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CrashDumpResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'hasDump')
    ..aOS(2, _omitFieldNames ? '' : 'reason')
    ..aOS(3, _omitFieldNames ? '' : 'taskName')
    ..aI(4, _omitFieldNames ? '' : 'uptimeS', fieldType: $pb.PbFieldType.OU3)
    ..aI(5, _omitFieldNames ? '' : 'freeHeap', fieldType: $pb.PbFieldType.OU3)
    ..p<$core.int>(6, _omitFieldNames ? '' : 'backtrace', $pb.PbFieldType.KU3)
    ..aI(7, _omitFieldNames ? '' : 'timestamp', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CrashDumpResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CrashDumpResponse copyWith(void Function(CrashDumpResponse) updates) =>
      super.copyWith((message) => updates(message as CrashDumpResponse))
          as CrashDumpResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CrashDumpResponse create() => CrashDumpResponse._();
  @$core.override
  CrashDumpResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CrashDumpResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CrashDumpResponse>(create);
  static CrashDumpResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get hasDump => $_getBF(0);
  @$pb.TagNumber(1)
  set hasDump($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasHasDump() => $_has(0);
  @$pb.TagNumber(1)
  void clearHasDump() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get reason => $_getSZ(1);
  @$pb.TagNumber(2)
  set reason($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasReason() => $_has(1);
  @$pb.TagNumber(2)
  void clearReason() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get taskName => $_getSZ(2);
  @$pb.TagNumber(3)
  set taskName($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasTaskName() => $_has(2);
  @$pb.TagNumber(3)
  void clearTaskName() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get uptimeS => $_getIZ(3);
  @$pb.TagNumber(4)
  set uptimeS($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasUptimeS() => $_has(3);
  @$pb.TagNumber(4)
  void clearUptimeS() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get freeHeap => $_getIZ(4);
  @$pb.TagNumber(5)
  set freeHeap($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasFreeHeap() => $_has(4);
  @$pb.TagNumber(5)
  void clearFreeHeap() => $_clearField(5);

  @$pb.TagNumber(6)
  $pb.PbList<$core.int> get backtrace => $_getList(5);

  @$pb.TagNumber(7)
  $core.int get timestamp => $_getIZ(6);
  @$pb.TagNumber(7)
  set timestamp($core.int value) => $_setUnsignedInt32(6, value);
  @$pb.TagNumber(7)
  $core.bool hasTimestamp() => $_has(6);
  @$pb.TagNumber(7)
  void clearTimestamp() => $_clearField(7);
}

class ClearCrashDumpRequest extends $pb.GeneratedMessage {
  factory ClearCrashDumpRequest() => create();

  ClearCrashDumpRequest._();

  factory ClearCrashDumpRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ClearCrashDumpRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ClearCrashDumpRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ClearCrashDumpRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ClearCrashDumpRequest copyWith(
          void Function(ClearCrashDumpRequest) updates) =>
      super.copyWith((message) => updates(message as ClearCrashDumpRequest))
          as ClearCrashDumpRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ClearCrashDumpRequest create() => ClearCrashDumpRequest._();
  @$core.override
  ClearCrashDumpRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ClearCrashDumpRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ClearCrashDumpRequest>(create);
  static ClearCrashDumpRequest? _defaultInstance;
}

class ClearCrashDumpResponse extends $pb.GeneratedMessage {
  factory ClearCrashDumpResponse({
    $core.bool? cleared,
  }) {
    final result = create();
    if (cleared != null) result.cleared = cleared;
    return result;
  }

  ClearCrashDumpResponse._();

  factory ClearCrashDumpResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ClearCrashDumpResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ClearCrashDumpResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'cleared')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ClearCrashDumpResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ClearCrashDumpResponse copyWith(
          void Function(ClearCrashDumpResponse) updates) =>
      super.copyWith((message) => updates(message as ClearCrashDumpResponse))
          as ClearCrashDumpResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ClearCrashDumpResponse create() => ClearCrashDumpResponse._();
  @$core.override
  ClearCrashDumpResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ClearCrashDumpResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ClearCrashDumpResponse>(create);
  static ClearCrashDumpResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get cleared => $_getBF(0);
  @$pb.TagNumber(1)
  set cleared($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasCleared() => $_has(0);
  @$pb.TagNumber(1)
  void clearCleared() => $_clearField(1);
}

/// Single heap sample
class HeapSample extends $pb.GeneratedMessage {
  factory HeapSample({
    $core.int? timestampS,
    $core.int? freeHeap,
    $core.int? largestBlock,
    $core.int? minFreeHeap,
  }) {
    final result = create();
    if (timestampS != null) result.timestampS = timestampS;
    if (freeHeap != null) result.freeHeap = freeHeap;
    if (largestBlock != null) result.largestBlock = largestBlock;
    if (minFreeHeap != null) result.minFreeHeap = minFreeHeap;
    return result;
  }

  HeapSample._();

  factory HeapSample.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory HeapSample.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'HeapSample',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'timestampS', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'freeHeap', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'largestBlock',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'minFreeHeap',
        fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  HeapSample clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  HeapSample copyWith(void Function(HeapSample) updates) =>
      super.copyWith((message) => updates(message as HeapSample)) as HeapSample;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static HeapSample create() => HeapSample._();
  @$core.override
  HeapSample createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static HeapSample getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<HeapSample>(create);
  static HeapSample? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get timestampS => $_getIZ(0);
  @$pb.TagNumber(1)
  set timestampS($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasTimestampS() => $_has(0);
  @$pb.TagNumber(1)
  void clearTimestampS() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get freeHeap => $_getIZ(1);
  @$pb.TagNumber(2)
  set freeHeap($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasFreeHeap() => $_has(1);
  @$pb.TagNumber(2)
  void clearFreeHeap() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get largestBlock => $_getIZ(2);
  @$pb.TagNumber(3)
  set largestBlock($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasLargestBlock() => $_has(2);
  @$pb.TagNumber(3)
  void clearLargestBlock() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get minFreeHeap => $_getIZ(3);
  @$pb.TagNumber(4)
  set minFreeHeap($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasMinFreeHeap() => $_has(3);
  @$pb.TagNumber(4)
  void clearMinFreeHeap() => $_clearField(4);
}

class GetMemoryProfileRequest extends $pb.GeneratedMessage {
  factory GetMemoryProfileRequest() => create();

  GetMemoryProfileRequest._();

  factory GetMemoryProfileRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetMemoryProfileRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetMemoryProfileRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetMemoryProfileRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetMemoryProfileRequest copyWith(
          void Function(GetMemoryProfileRequest) updates) =>
      super.copyWith((message) => updates(message as GetMemoryProfileRequest))
          as GetMemoryProfileRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetMemoryProfileRequest create() => GetMemoryProfileRequest._();
  @$core.override
  GetMemoryProfileRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetMemoryProfileRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetMemoryProfileRequest>(create);
  static GetMemoryProfileRequest? _defaultInstance;
}

class GetMemoryProfileResponse extends $pb.GeneratedMessage {
  factory GetMemoryProfileResponse({
    $core.int? currentFreeHeap,
    $core.int? currentMinFreeHeap,
    $core.int? currentLargestBlock,
    $core.int? totalHeap,
    $core.Iterable<HeapSample>? samples,
  }) {
    final result = create();
    if (currentFreeHeap != null) result.currentFreeHeap = currentFreeHeap;
    if (currentMinFreeHeap != null)
      result.currentMinFreeHeap = currentMinFreeHeap;
    if (currentLargestBlock != null)
      result.currentLargestBlock = currentLargestBlock;
    if (totalHeap != null) result.totalHeap = totalHeap;
    if (samples != null) result.samples.addAll(samples);
    return result;
  }

  GetMemoryProfileResponse._();

  factory GetMemoryProfileResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory GetMemoryProfileResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'GetMemoryProfileResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'currentFreeHeap',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'currentMinFreeHeap',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'currentLargestBlock',
        fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'totalHeap', fieldType: $pb.PbFieldType.OU3)
    ..pPM<HeapSample>(5, _omitFieldNames ? '' : 'samples',
        subBuilder: HeapSample.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetMemoryProfileResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  GetMemoryProfileResponse copyWith(
          void Function(GetMemoryProfileResponse) updates) =>
      super.copyWith((message) => updates(message as GetMemoryProfileResponse))
          as GetMemoryProfileResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static GetMemoryProfileResponse create() => GetMemoryProfileResponse._();
  @$core.override
  GetMemoryProfileResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static GetMemoryProfileResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<GetMemoryProfileResponse>(create);
  static GetMemoryProfileResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get currentFreeHeap => $_getIZ(0);
  @$pb.TagNumber(1)
  set currentFreeHeap($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasCurrentFreeHeap() => $_has(0);
  @$pb.TagNumber(1)
  void clearCurrentFreeHeap() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get currentMinFreeHeap => $_getIZ(1);
  @$pb.TagNumber(2)
  set currentMinFreeHeap($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasCurrentMinFreeHeap() => $_has(1);
  @$pb.TagNumber(2)
  void clearCurrentMinFreeHeap() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get currentLargestBlock => $_getIZ(2);
  @$pb.TagNumber(3)
  set currentLargestBlock($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasCurrentLargestBlock() => $_has(2);
  @$pb.TagNumber(3)
  void clearCurrentLargestBlock() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get totalHeap => $_getIZ(3);
  @$pb.TagNumber(4)
  set totalHeap($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasTotalHeap() => $_has(3);
  @$pb.TagNumber(4)
  void clearTotalHeap() => $_clearField(4);

  @$pb.TagNumber(5)
  $pb.PbList<HeapSample> get samples => $_getList(4);
}

/// Individual test result
class SelfTestResult extends $pb.GeneratedMessage {
  factory SelfTestResult({
    $core.String? name,
    $core.bool? passed,
    $core.String? message,
  }) {
    final result = create();
    if (name != null) result.name = name;
    if (passed != null) result.passed = passed;
    if (message != null) result.message = message;
    return result;
  }

  SelfTestResult._();

  factory SelfTestResult.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SelfTestResult.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SelfTestResult',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'name')
    ..aOB(2, _omitFieldNames ? '' : 'passed')
    ..aOS(3, _omitFieldNames ? '' : 'message')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestResult clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestResult copyWith(void Function(SelfTestResult) updates) =>
      super.copyWith((message) => updates(message as SelfTestResult))
          as SelfTestResult;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SelfTestResult create() => SelfTestResult._();
  @$core.override
  SelfTestResult createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SelfTestResult getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SelfTestResult>(create);
  static SelfTestResult? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get passed => $_getBF(1);
  @$pb.TagNumber(2)
  set passed($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasPassed() => $_has(1);
  @$pb.TagNumber(2)
  void clearPassed() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get message => $_getSZ(2);
  @$pb.TagNumber(3)
  set message($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasMessage() => $_has(2);
  @$pb.TagNumber(3)
  void clearMessage() => $_clearField(3);
}

/// Self-test request (trigger on-device smoke tests)
class SelfTestRequest extends $pb.GeneratedMessage {
  factory SelfTestRequest() => create();

  SelfTestRequest._();

  factory SelfTestRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SelfTestRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SelfTestRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestRequest copyWith(void Function(SelfTestRequest) updates) =>
      super.copyWith((message) => updates(message as SelfTestRequest))
          as SelfTestRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SelfTestRequest create() => SelfTestRequest._();
  @$core.override
  SelfTestRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SelfTestRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SelfTestRequest>(create);
  static SelfTestRequest? _defaultInstance;
}

/// Self-test response with per-test results
class SelfTestResponse extends $pb.GeneratedMessage {
  factory SelfTestResponse({
    $core.int? testsRun,
    $core.int? testsPassed,
    $core.Iterable<SelfTestResult>? results,
  }) {
    final result = create();
    if (testsRun != null) result.testsRun = testsRun;
    if (testsPassed != null) result.testsPassed = testsPassed;
    if (results != null) result.results.addAll(results);
    return result;
  }

  SelfTestResponse._();

  factory SelfTestResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SelfTestResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SelfTestResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'testsRun', fieldType: $pb.PbFieldType.OU3)
    ..aI(2, _omitFieldNames ? '' : 'testsPassed',
        fieldType: $pb.PbFieldType.OU3)
    ..pPM<SelfTestResult>(3, _omitFieldNames ? '' : 'results',
        subBuilder: SelfTestResult.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelfTestResponse copyWith(void Function(SelfTestResponse) updates) =>
      super.copyWith((message) => updates(message as SelfTestResponse))
          as SelfTestResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SelfTestResponse create() => SelfTestResponse._();
  @$core.override
  SelfTestResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SelfTestResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SelfTestResponse>(create);
  static SelfTestResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get testsRun => $_getIZ(0);
  @$pb.TagNumber(1)
  set testsRun($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasTestsRun() => $_has(0);
  @$pb.TagNumber(1)
  void clearTestsRun() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get testsPassed => $_getIZ(1);
  @$pb.TagNumber(2)
  set testsPassed($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTestsPassed() => $_has(1);
  @$pb.TagNumber(2)
  void clearTestsPassed() => $_clearField(2);

  @$pb.TagNumber(3)
  $pb.PbList<SelfTestResult> get results => $_getList(2);
}

/// Check for firmware updates (triggers GitHub API query)
class CheckUpdateRequest extends $pb.GeneratedMessage {
  factory CheckUpdateRequest() => create();

  CheckUpdateRequest._();

  factory CheckUpdateRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CheckUpdateRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CheckUpdateRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CheckUpdateRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CheckUpdateRequest copyWith(void Function(CheckUpdateRequest) updates) =>
      super.copyWith((message) => updates(message as CheckUpdateRequest))
          as CheckUpdateRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CheckUpdateRequest create() => CheckUpdateRequest._();
  @$core.override
  CheckUpdateRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CheckUpdateRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CheckUpdateRequest>(create);
  static CheckUpdateRequest? _defaultInstance;
}

class CheckUpdateResponse extends $pb.GeneratedMessage {
  factory CheckUpdateResponse({
    $core.bool? updateAvailable,
    $core.String? currentVersion,
    $core.String? availableVersion,
    $core.int? firmwareSize,
    $core.bool? autoUpdateEnabled,
  }) {
    final result = create();
    if (updateAvailable != null) result.updateAvailable = updateAvailable;
    if (currentVersion != null) result.currentVersion = currentVersion;
    if (availableVersion != null) result.availableVersion = availableVersion;
    if (firmwareSize != null) result.firmwareSize = firmwareSize;
    if (autoUpdateEnabled != null) result.autoUpdateEnabled = autoUpdateEnabled;
    return result;
  }

  CheckUpdateResponse._();

  factory CheckUpdateResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CheckUpdateResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CheckUpdateResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'updateAvailable')
    ..aOS(2, _omitFieldNames ? '' : 'currentVersion')
    ..aOS(3, _omitFieldNames ? '' : 'availableVersion')
    ..aI(4, _omitFieldNames ? '' : 'firmwareSize',
        fieldType: $pb.PbFieldType.OU3)
    ..aOB(5, _omitFieldNames ? '' : 'autoUpdateEnabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CheckUpdateResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CheckUpdateResponse copyWith(void Function(CheckUpdateResponse) updates) =>
      super.copyWith((message) => updates(message as CheckUpdateResponse))
          as CheckUpdateResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CheckUpdateResponse create() => CheckUpdateResponse._();
  @$core.override
  CheckUpdateResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CheckUpdateResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CheckUpdateResponse>(create);
  static CheckUpdateResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get updateAvailable => $_getBF(0);
  @$pb.TagNumber(1)
  set updateAvailable($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasUpdateAvailable() => $_has(0);
  @$pb.TagNumber(1)
  void clearUpdateAvailable() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get currentVersion => $_getSZ(1);
  @$pb.TagNumber(2)
  set currentVersion($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasCurrentVersion() => $_has(1);
  @$pb.TagNumber(2)
  void clearCurrentVersion() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get availableVersion => $_getSZ(2);
  @$pb.TagNumber(3)
  set availableVersion($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasAvailableVersion() => $_has(2);
  @$pb.TagNumber(3)
  void clearAvailableVersion() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get firmwareSize => $_getIZ(3);
  @$pb.TagNumber(4)
  set firmwareSize($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasFirmwareSize() => $_has(3);
  @$pb.TagNumber(4)
  void clearFirmwareSize() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.bool get autoUpdateEnabled => $_getBF(4);
  @$pb.TagNumber(5)
  set autoUpdateEnabled($core.bool value) => $_setBool(4, value);
  @$pb.TagNumber(5)
  $core.bool hasAutoUpdateEnabled() => $_has(4);
  @$pb.TagNumber(5)
  void clearAutoUpdateEnabled() => $_clearField(5);
}

/// Enable/disable automatic OTA updates on WiFi connect
class SetAutoUpdateRequest extends $pb.GeneratedMessage {
  factory SetAutoUpdateRequest({
    $core.bool? enabled,
  }) {
    final result = create();
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  SetAutoUpdateRequest._();

  factory SetAutoUpdateRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetAutoUpdateRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetAutoUpdateRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetAutoUpdateRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetAutoUpdateRequest copyWith(void Function(SetAutoUpdateRequest) updates) =>
      super.copyWith((message) => updates(message as SetAutoUpdateRequest))
          as SetAutoUpdateRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetAutoUpdateRequest create() => SetAutoUpdateRequest._();
  @$core.override
  SetAutoUpdateRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetAutoUpdateRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetAutoUpdateRequest>(create);
  static SetAutoUpdateRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get enabled => $_getBF(0);
  @$pb.TagNumber(1)
  set enabled($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasEnabled() => $_has(0);
  @$pb.TagNumber(1)
  void clearEnabled() => $_clearField(1);
}

class SetAutoUpdateResponse extends $pb.GeneratedMessage {
  factory SetAutoUpdateResponse({
    $core.bool? enabled,
  }) {
    final result = create();
    if (enabled != null) result.enabled = enabled;
    return result;
  }

  SetAutoUpdateResponse._();

  factory SetAutoUpdateResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetAutoUpdateResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetAutoUpdateResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'enabled')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetAutoUpdateResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetAutoUpdateResponse copyWith(
          void Function(SetAutoUpdateResponse) updates) =>
      super.copyWith((message) => updates(message as SetAutoUpdateResponse))
          as SetAutoUpdateResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetAutoUpdateResponse create() => SetAutoUpdateResponse._();
  @$core.override
  SetAutoUpdateResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetAutoUpdateResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetAutoUpdateResponse>(create);
  static SetAutoUpdateResponse? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get enabled => $_getBF(0);
  @$pb.TagNumber(1)
  set enabled($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasEnabled() => $_has(0);
  @$pb.TagNumber(1)
  void clearEnabled() => $_clearField(1);
}

/// Inject a simulated touch on a specific pad (host -> device)
class SimulateTouchRequest extends $pb.GeneratedMessage {
  factory SimulateTouchRequest({
    $core.int? padIndex,
  }) {
    final result = create();
    if (padIndex != null) result.padIndex = padIndex;
    return result;
  }

  SimulateTouchRequest._();

  factory SimulateTouchRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SimulateTouchRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SimulateTouchRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aI(1, _omitFieldNames ? '' : 'padIndex', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouchRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouchRequest copyWith(void Function(SimulateTouchRequest) updates) =>
      super.copyWith((message) => updates(message as SimulateTouchRequest))
          as SimulateTouchRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SimulateTouchRequest create() => SimulateTouchRequest._();
  @$core.override
  SimulateTouchRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SimulateTouchRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SimulateTouchRequest>(create);
  static SimulateTouchRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get padIndex => $_getIZ(0);
  @$pb.TagNumber(1)
  set padIndex($core.int value) => $_setUnsignedInt32(0, value);
  @$pb.TagNumber(1)
  $core.bool hasPadIndex() => $_has(0);
  @$pb.TagNumber(1)
  void clearPadIndex() => $_clearField(1);
}

class SimulateTouchResponse extends $pb.GeneratedMessage {
  factory SimulateTouchResponse({
    Status? status,
  }) {
    final result = create();
    if (status != null) result.status = status;
    return result;
  }

  SimulateTouchResponse._();

  factory SimulateTouchResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SimulateTouchResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SimulateTouchResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<Status>(1, _omitFieldNames ? '' : 'status', enumValues: Status.values)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouchResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SimulateTouchResponse copyWith(
          void Function(SimulateTouchResponse) updates) =>
      super.copyWith((message) => updates(message as SimulateTouchResponse))
          as SimulateTouchResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SimulateTouchResponse create() => SimulateTouchResponse._();
  @$core.override
  SimulateTouchResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SimulateTouchResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SimulateTouchResponse>(create);
  static SimulateTouchResponse? _defaultInstance;

  @$pb.TagNumber(1)
  Status get status => $_getN(0);
  @$pb.TagNumber(1)
  set status(Status value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasStatus() => $_has(0);
  @$pb.TagNumber(1)
  void clearStatus() => $_clearField(1);
}

/// Enable/disable sim drill mode (auto-inject during ESP-NOW drills)
class SetSimModeRequest extends $pb.GeneratedMessage {
  factory SetSimModeRequest({
    $core.bool? enabled,
    $core.int? delayMs,
    $core.int? padIndex,
  }) {
    final result = create();
    if (enabled != null) result.enabled = enabled;
    if (delayMs != null) result.delayMs = delayMs;
    if (padIndex != null) result.padIndex = padIndex;
    return result;
  }

  SetSimModeRequest._();

  factory SetSimModeRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetSimModeRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetSimModeRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'enabled')
    ..aI(2, _omitFieldNames ? '' : 'delayMs', fieldType: $pb.PbFieldType.OU3)
    ..aI(3, _omitFieldNames ? '' : 'padIndex', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetSimModeRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetSimModeRequest copyWith(void Function(SetSimModeRequest) updates) =>
      super.copyWith((message) => updates(message as SetSimModeRequest))
          as SetSimModeRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetSimModeRequest create() => SetSimModeRequest._();
  @$core.override
  SetSimModeRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetSimModeRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetSimModeRequest>(create);
  static SetSimModeRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get enabled => $_getBF(0);
  @$pb.TagNumber(1)
  set enabled($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasEnabled() => $_has(0);
  @$pb.TagNumber(1)
  void clearEnabled() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.int get delayMs => $_getIZ(1);
  @$pb.TagNumber(2)
  set delayMs($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasDelayMs() => $_has(1);
  @$pb.TagNumber(2)
  void clearDelayMs() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get padIndex => $_getIZ(2);
  @$pb.TagNumber(3)
  set padIndex($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasPadIndex() => $_has(2);
  @$pb.TagNumber(3)
  void clearPadIndex() => $_clearField(3);
}

class SetSimModeResponse extends $pb.GeneratedMessage {
  factory SetSimModeResponse({
    Status? status,
    $core.bool? enabled,
    $core.int? delayMs,
    $core.int? padIndex,
  }) {
    final result = create();
    if (status != null) result.status = status;
    if (enabled != null) result.enabled = enabled;
    if (delayMs != null) result.delayMs = delayMs;
    if (padIndex != null) result.padIndex = padIndex;
    return result;
  }

  SetSimModeResponse._();

  factory SetSimModeResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SetSimModeResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SetSimModeResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..aE<Status>(1, _omitFieldNames ? '' : 'status', enumValues: Status.values)
    ..aOB(2, _omitFieldNames ? '' : 'enabled')
    ..aI(3, _omitFieldNames ? '' : 'delayMs', fieldType: $pb.PbFieldType.OU3)
    ..aI(4, _omitFieldNames ? '' : 'padIndex', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetSimModeResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SetSimModeResponse copyWith(void Function(SetSimModeResponse) updates) =>
      super.copyWith((message) => updates(message as SetSimModeResponse))
          as SetSimModeResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SetSimModeResponse create() => SetSimModeResponse._();
  @$core.override
  SetSimModeResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SetSimModeResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SetSimModeResponse>(create);
  static SetSimModeResponse? _defaultInstance;

  @$pb.TagNumber(1)
  Status get status => $_getN(0);
  @$pb.TagNumber(1)
  set status(Status value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasStatus() => $_has(0);
  @$pb.TagNumber(1)
  void clearStatus() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get enabled => $_getBF(1);
  @$pb.TagNumber(2)
  set enabled($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasEnabled() => $_has(1);
  @$pb.TagNumber(2)
  void clearEnabled() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.int get delayMs => $_getIZ(2);
  @$pb.TagNumber(3)
  set delayMs($core.int value) => $_setUnsignedInt32(2, value);
  @$pb.TagNumber(3)
  $core.bool hasDelayMs() => $_has(2);
  @$pb.TagNumber(3)
  void clearDelayMs() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get padIndex => $_getIZ(3);
  @$pb.TagNumber(4)
  set padIndex($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasPadIndex() => $_has(3);
  @$pb.TagNumber(4)
  void clearPadIndex() => $_clearField(4);
}

enum ConfigRequest_Request { listFeatures, setFeature, notSet }

/// Top-level request envelope
class ConfigRequest extends $pb.GeneratedMessage {
  factory ConfigRequest({
    ListFeaturesRequest? listFeatures,
    SetFeatureRequest? setFeature,
  }) {
    final result = create();
    if (listFeatures != null) result.listFeatures = listFeatures;
    if (setFeature != null) result.setFeature = setFeature;
    return result;
  }

  ConfigRequest._();

  factory ConfigRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ConfigRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static const $core.Map<$core.int, ConfigRequest_Request>
      _ConfigRequest_RequestByTag = {
    1: ConfigRequest_Request.listFeatures,
    2: ConfigRequest_Request.setFeature,
    0: ConfigRequest_Request.notSet
  };
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ConfigRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..oo(0, [1, 2])
    ..aOM<ListFeaturesRequest>(1, _omitFieldNames ? '' : 'listFeatures',
        subBuilder: ListFeaturesRequest.create)
    ..aOM<SetFeatureRequest>(2, _omitFieldNames ? '' : 'setFeature',
        subBuilder: SetFeatureRequest.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ConfigRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ConfigRequest copyWith(void Function(ConfigRequest) updates) =>
      super.copyWith((message) => updates(message as ConfigRequest))
          as ConfigRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ConfigRequest create() => ConfigRequest._();
  @$core.override
  ConfigRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ConfigRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ConfigRequest>(create);
  static ConfigRequest? _defaultInstance;

  @$pb.TagNumber(1)
  @$pb.TagNumber(2)
  ConfigRequest_Request whichRequest() =>
      _ConfigRequest_RequestByTag[$_whichOneof(0)]!;
  @$pb.TagNumber(1)
  @$pb.TagNumber(2)
  void clearRequest() => $_clearField($_whichOneof(0));

  @$pb.TagNumber(1)
  ListFeaturesRequest get listFeatures => $_getN(0);
  @$pb.TagNumber(1)
  set listFeatures(ListFeaturesRequest value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasListFeatures() => $_has(0);
  @$pb.TagNumber(1)
  void clearListFeatures() => $_clearField(1);
  @$pb.TagNumber(1)
  ListFeaturesRequest ensureListFeatures() => $_ensure(0);

  @$pb.TagNumber(2)
  SetFeatureRequest get setFeature => $_getN(1);
  @$pb.TagNumber(2)
  set setFeature(SetFeatureRequest value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasSetFeature() => $_has(1);
  @$pb.TagNumber(2)
  void clearSetFeature() => $_clearField(2);
  @$pb.TagNumber(2)
  SetFeatureRequest ensureSetFeature() => $_ensure(1);
}

enum ConfigResponse_Response { listFeatures, setFeature, notSet }

/// Top-level response envelope
class ConfigResponse extends $pb.GeneratedMessage {
  factory ConfigResponse({
    Status? status,
    ListFeaturesResponse? listFeatures,
    SetFeatureResponse? setFeature,
  }) {
    final result = create();
    if (status != null) result.status = status;
    if (listFeatures != null) result.listFeatures = listFeatures;
    if (setFeature != null) result.setFeature = setFeature;
    return result;
  }

  ConfigResponse._();

  factory ConfigResponse.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ConfigResponse.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static const $core.Map<$core.int, ConfigResponse_Response>
      _ConfigResponse_ResponseByTag = {
    2: ConfigResponse_Response.listFeatures,
    3: ConfigResponse_Response.setFeature,
    0: ConfigResponse_Response.notSet
  };
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ConfigResponse',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'domes.config'),
      createEmptyInstance: create)
    ..oo(0, [2, 3])
    ..aE<Status>(1, _omitFieldNames ? '' : 'status', enumValues: Status.values)
    ..aOM<ListFeaturesResponse>(2, _omitFieldNames ? '' : 'listFeatures',
        subBuilder: ListFeaturesResponse.create)
    ..aOM<SetFeatureResponse>(3, _omitFieldNames ? '' : 'setFeature',
        subBuilder: SetFeatureResponse.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ConfigResponse clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ConfigResponse copyWith(void Function(ConfigResponse) updates) =>
      super.copyWith((message) => updates(message as ConfigResponse))
          as ConfigResponse;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ConfigResponse create() => ConfigResponse._();
  @$core.override
  ConfigResponse createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ConfigResponse getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ConfigResponse>(create);
  static ConfigResponse? _defaultInstance;

  @$pb.TagNumber(2)
  @$pb.TagNumber(3)
  ConfigResponse_Response whichResponse() =>
      _ConfigResponse_ResponseByTag[$_whichOneof(0)]!;
  @$pb.TagNumber(2)
  @$pb.TagNumber(3)
  void clearResponse() => $_clearField($_whichOneof(0));

  @$pb.TagNumber(1)
  Status get status => $_getN(0);
  @$pb.TagNumber(1)
  set status(Status value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasStatus() => $_has(0);
  @$pb.TagNumber(1)
  void clearStatus() => $_clearField(1);

  @$pb.TagNumber(2)
  ListFeaturesResponse get listFeatures => $_getN(1);
  @$pb.TagNumber(2)
  set listFeatures(ListFeaturesResponse value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasListFeatures() => $_has(1);
  @$pb.TagNumber(2)
  void clearListFeatures() => $_clearField(2);
  @$pb.TagNumber(2)
  ListFeaturesResponse ensureListFeatures() => $_ensure(1);

  @$pb.TagNumber(3)
  SetFeatureResponse get setFeature => $_getN(2);
  @$pb.TagNumber(3)
  set setFeature(SetFeatureResponse value) => $_setField(3, value);
  @$pb.TagNumber(3)
  $core.bool hasSetFeature() => $_has(2);
  @$pb.TagNumber(3)
  void clearSetFeature() => $_clearField(3);
  @$pb.TagNumber(3)
  SetFeatureResponse ensureSetFeature() => $_ensure(2);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
