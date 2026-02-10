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

/// Frame-level message types for config protocol
/// Used in the frame header [0xAA 0x55][Len][MsgType][Payload][CRC32]
class MsgType extends $pb.ProtobufEnum {
  static const MsgType MSG_TYPE_UNKNOWN =
      MsgType._(0, _omitEnumNames ? '' : 'MSG_TYPE_UNKNOWN');

  /// Config commands (0x20-0x2F range)
  static const MsgType MSG_TYPE_LIST_FEATURES_REQ =
      MsgType._(32, _omitEnumNames ? '' : 'MSG_TYPE_LIST_FEATURES_REQ');
  static const MsgType MSG_TYPE_LIST_FEATURES_RSP =
      MsgType._(33, _omitEnumNames ? '' : 'MSG_TYPE_LIST_FEATURES_RSP');
  static const MsgType MSG_TYPE_SET_FEATURE_REQ =
      MsgType._(34, _omitEnumNames ? '' : 'MSG_TYPE_SET_FEATURE_REQ');
  static const MsgType MSG_TYPE_SET_FEATURE_RSP =
      MsgType._(35, _omitEnumNames ? '' : 'MSG_TYPE_SET_FEATURE_RSP');
  static const MsgType MSG_TYPE_GET_FEATURE_REQ =
      MsgType._(36, _omitEnumNames ? '' : 'MSG_TYPE_GET_FEATURE_REQ');
  static const MsgType MSG_TYPE_GET_FEATURE_RSP =
      MsgType._(37, _omitEnumNames ? '' : 'MSG_TYPE_GET_FEATURE_RSP');
  static const MsgType MSG_TYPE_SET_LED_PATTERN_REQ =
      MsgType._(38, _omitEnumNames ? '' : 'MSG_TYPE_SET_LED_PATTERN_REQ');
  static const MsgType MSG_TYPE_SET_LED_PATTERN_RSP =
      MsgType._(39, _omitEnumNames ? '' : 'MSG_TYPE_SET_LED_PATTERN_RSP');
  static const MsgType MSG_TYPE_GET_LED_PATTERN_REQ =
      MsgType._(40, _omitEnumNames ? '' : 'MSG_TYPE_GET_LED_PATTERN_REQ');
  static const MsgType MSG_TYPE_GET_LED_PATTERN_RSP =
      MsgType._(41, _omitEnumNames ? '' : 'MSG_TYPE_GET_LED_PATTERN_RSP');
  static const MsgType MSG_TYPE_SET_IMU_TRIAGE_REQ =
      MsgType._(42, _omitEnumNames ? '' : 'MSG_TYPE_SET_IMU_TRIAGE_REQ');
  static const MsgType MSG_TYPE_SET_IMU_TRIAGE_RSP =
      MsgType._(43, _omitEnumNames ? '' : 'MSG_TYPE_SET_IMU_TRIAGE_RSP');

  /// System mode commands (0x30-0x35 range)
  static const MsgType MSG_TYPE_GET_MODE_REQ =
      MsgType._(48, _omitEnumNames ? '' : 'MSG_TYPE_GET_MODE_REQ');
  static const MsgType MSG_TYPE_GET_MODE_RSP =
      MsgType._(49, _omitEnumNames ? '' : 'MSG_TYPE_GET_MODE_RSP');
  static const MsgType MSG_TYPE_SET_MODE_REQ =
      MsgType._(50, _omitEnumNames ? '' : 'MSG_TYPE_SET_MODE_REQ');
  static const MsgType MSG_TYPE_SET_MODE_RSP =
      MsgType._(51, _omitEnumNames ? '' : 'MSG_TYPE_SET_MODE_RSP');
  static const MsgType MSG_TYPE_GET_SYSTEM_INFO_REQ =
      MsgType._(52, _omitEnumNames ? '' : 'MSG_TYPE_GET_SYSTEM_INFO_REQ');
  static const MsgType MSG_TYPE_GET_SYSTEM_INFO_RSP =
      MsgType._(53, _omitEnumNames ? '' : 'MSG_TYPE_GET_SYSTEM_INFO_RSP');

  static const $core.List<MsgType> values = <MsgType>[
    MSG_TYPE_UNKNOWN,
    MSG_TYPE_LIST_FEATURES_REQ,
    MSG_TYPE_LIST_FEATURES_RSP,
    MSG_TYPE_SET_FEATURE_REQ,
    MSG_TYPE_SET_FEATURE_RSP,
    MSG_TYPE_GET_FEATURE_REQ,
    MSG_TYPE_GET_FEATURE_RSP,
    MSG_TYPE_SET_LED_PATTERN_REQ,
    MSG_TYPE_SET_LED_PATTERN_RSP,
    MSG_TYPE_GET_LED_PATTERN_REQ,
    MSG_TYPE_GET_LED_PATTERN_RSP,
    MSG_TYPE_SET_IMU_TRIAGE_REQ,
    MSG_TYPE_SET_IMU_TRIAGE_RSP,
    MSG_TYPE_GET_MODE_REQ,
    MSG_TYPE_GET_MODE_RSP,
    MSG_TYPE_SET_MODE_REQ,
    MSG_TYPE_SET_MODE_RSP,
    MSG_TYPE_GET_SYSTEM_INFO_REQ,
    MSG_TYPE_GET_SYSTEM_INFO_RSP,
  ];

  static final $core.Map<$core.int, MsgType> _byValue =
      $pb.ProtobufEnum.initByValue(values);
  static MsgType? valueOf($core.int value) => _byValue[value];

  const MsgType._(super.value, super.name);
}

/// Status codes for responses
class Status extends $pb.ProtobufEnum {
  static const Status STATUS_OK =
      Status._(0, _omitEnumNames ? '' : 'STATUS_OK');
  static const Status STATUS_ERROR =
      Status._(1, _omitEnumNames ? '' : 'STATUS_ERROR');
  static const Status STATUS_INVALID_FEATURE =
      Status._(2, _omitEnumNames ? '' : 'STATUS_INVALID_FEATURE');
  static const Status STATUS_BUSY =
      Status._(3, _omitEnumNames ? '' : 'STATUS_BUSY');
  static const Status STATUS_INVALID_PATTERN =
      Status._(4, _omitEnumNames ? '' : 'STATUS_INVALID_PATTERN');

  static const $core.List<Status> values = <Status>[
    STATUS_OK,
    STATUS_ERROR,
    STATUS_INVALID_FEATURE,
    STATUS_BUSY,
    STATUS_INVALID_PATTERN,
  ];

  static final $core.List<Status?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 4);
  static Status? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const Status._(super.value, super.name);
}

/// LED pattern types
class LedPatternType extends $pb.ProtobufEnum {
  static const LedPatternType LED_PATTERN_OFF =
      LedPatternType._(0, _omitEnumNames ? '' : 'LED_PATTERN_OFF');
  static const LedPatternType LED_PATTERN_SOLID =
      LedPatternType._(1, _omitEnumNames ? '' : 'LED_PATTERN_SOLID');
  static const LedPatternType LED_PATTERN_BREATHING =
      LedPatternType._(2, _omitEnumNames ? '' : 'LED_PATTERN_BREATHING');
  static const LedPatternType LED_PATTERN_COLOR_CYCLE =
      LedPatternType._(3, _omitEnumNames ? '' : 'LED_PATTERN_COLOR_CYCLE');

  static const $core.List<LedPatternType> values = <LedPatternType>[
    LED_PATTERN_OFF,
    LED_PATTERN_SOLID,
    LED_PATTERN_BREATHING,
    LED_PATTERN_COLOR_CYCLE,
  ];

  static final $core.List<LedPatternType?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 3);
  static LedPatternType? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const LedPatternType._(super.value, super.name);
}

/// Runtime-toggleable features
class Feature extends $pb.ProtobufEnum {
  static const Feature FEATURE_UNKNOWN =
      Feature._(0, _omitEnumNames ? '' : 'FEATURE_UNKNOWN');
  static const Feature FEATURE_LED_EFFECTS =
      Feature._(1, _omitEnumNames ? '' : 'FEATURE_LED_EFFECTS');
  static const Feature FEATURE_BLE_ADVERTISING =
      Feature._(2, _omitEnumNames ? '' : 'FEATURE_BLE_ADVERTISING');
  static const Feature FEATURE_WIFI =
      Feature._(3, _omitEnumNames ? '' : 'FEATURE_WIFI');
  static const Feature FEATURE_ESP_NOW =
      Feature._(4, _omitEnumNames ? '' : 'FEATURE_ESP_NOW');
  static const Feature FEATURE_TOUCH =
      Feature._(5, _omitEnumNames ? '' : 'FEATURE_TOUCH');
  static const Feature FEATURE_HAPTIC =
      Feature._(6, _omitEnumNames ? '' : 'FEATURE_HAPTIC');
  static const Feature FEATURE_AUDIO =
      Feature._(7, _omitEnumNames ? '' : 'FEATURE_AUDIO');

  static const $core.List<Feature> values = <Feature>[
    FEATURE_UNKNOWN,
    FEATURE_LED_EFFECTS,
    FEATURE_BLE_ADVERTISING,
    FEATURE_WIFI,
    FEATURE_ESP_NOW,
    FEATURE_TOUCH,
    FEATURE_HAPTIC,
    FEATURE_AUDIO,
  ];

  static final $core.List<Feature?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 7);
  static Feature? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const Feature._(super.value, super.name);
}

/// System operating modes
class SystemMode extends $pb.ProtobufEnum {
  static const SystemMode SYSTEM_MODE_BOOTING =
      SystemMode._(0, _omitEnumNames ? '' : 'SYSTEM_MODE_BOOTING');
  static const SystemMode SYSTEM_MODE_IDLE =
      SystemMode._(1, _omitEnumNames ? '' : 'SYSTEM_MODE_IDLE');
  static const SystemMode SYSTEM_MODE_TRIAGE =
      SystemMode._(2, _omitEnumNames ? '' : 'SYSTEM_MODE_TRIAGE');
  static const SystemMode SYSTEM_MODE_CONNECTED =
      SystemMode._(3, _omitEnumNames ? '' : 'SYSTEM_MODE_CONNECTED');
  static const SystemMode SYSTEM_MODE_GAME =
      SystemMode._(4, _omitEnumNames ? '' : 'SYSTEM_MODE_GAME');
  static const SystemMode SYSTEM_MODE_ERROR =
      SystemMode._(5, _omitEnumNames ? '' : 'SYSTEM_MODE_ERROR');

  static const $core.List<SystemMode> values = <SystemMode>[
    SYSTEM_MODE_BOOTING,
    SYSTEM_MODE_IDLE,
    SYSTEM_MODE_TRIAGE,
    SYSTEM_MODE_CONNECTED,
    SYSTEM_MODE_GAME,
    SYSTEM_MODE_ERROR,
  ];

  static final $core.List<SystemMode?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 5);
  static SystemMode? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const SystemMode._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
