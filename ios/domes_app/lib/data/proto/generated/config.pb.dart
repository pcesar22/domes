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
  }) {
    final result = create();
    if (features != null) result.features.addAll(features);
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
  }) {
    final result = create();
    if (firmwareVersion != null) result.firmwareVersion = firmwareVersion;
    if (uptimeS != null) result.uptimeS = uptimeS;
    if (freeHeap != null) result.freeHeap = freeHeap;
    if (bootCount != null) result.bootCount = bootCount;
    if (mode != null) result.mode = mode;
    if (featureMask != null) result.featureMask = featureMask;
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
