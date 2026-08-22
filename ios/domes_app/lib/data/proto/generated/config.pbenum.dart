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

  /// System mode commands (0x30-0x37 range)
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
  static const MsgType MSG_TYPE_SET_POD_ID_REQ =
      MsgType._(54, _omitEnumNames ? '' : 'MSG_TYPE_SET_POD_ID_REQ');
  static const MsgType MSG_TYPE_SET_POD_ID_RSP =
      MsgType._(55, _omitEnumNames ? '' : 'MSG_TYPE_SET_POD_ID_RSP');

  /// Observability commands (0x38-0x49 range)
  static const MsgType MSG_TYPE_GET_HEALTH_REQ =
      MsgType._(56, _omitEnumNames ? '' : 'MSG_TYPE_GET_HEALTH_REQ');
  static const MsgType MSG_TYPE_GET_HEALTH_RSP =
      MsgType._(57, _omitEnumNames ? '' : 'MSG_TYPE_GET_HEALTH_RSP');
  static const MsgType MSG_TYPE_GET_ESPNOW_STATUS_REQ =
      MsgType._(58, _omitEnumNames ? '' : 'MSG_TYPE_GET_ESPNOW_STATUS_REQ');
  static const MsgType MSG_TYPE_GET_ESPNOW_STATUS_RSP =
      MsgType._(59, _omitEnumNames ? '' : 'MSG_TYPE_GET_ESPNOW_STATUS_RSP');
  static const MsgType MSG_TYPE_ESPNOW_BENCH_REQ =
      MsgType._(60, _omitEnumNames ? '' : 'MSG_TYPE_ESPNOW_BENCH_REQ');
  static const MsgType MSG_TYPE_ESPNOW_BENCH_RSP =
      MsgType._(61, _omitEnumNames ? '' : 'MSG_TYPE_ESPNOW_BENCH_RSP');

  /// Clean-restart snapshot commands with legacy crash-dump names (0x3E-0x41)
  static const MsgType MSG_TYPE_GET_CRASH_DUMP_REQ =
      MsgType._(62, _omitEnumNames ? '' : 'MSG_TYPE_GET_CRASH_DUMP_REQ');
  static const MsgType MSG_TYPE_GET_CRASH_DUMP_RSP =
      MsgType._(63, _omitEnumNames ? '' : 'MSG_TYPE_GET_CRASH_DUMP_RSP');
  static const MsgType MSG_TYPE_CLEAR_CRASH_DUMP_REQ =
      MsgType._(64, _omitEnumNames ? '' : 'MSG_TYPE_CLEAR_CRASH_DUMP_REQ');
  static const MsgType MSG_TYPE_CLEAR_CRASH_DUMP_RSP =
      MsgType._(65, _omitEnumNames ? '' : 'MSG_TYPE_CLEAR_CRASH_DUMP_RSP');

  /// Memory profiler commands (0x42-0x43)
  static const MsgType MSG_TYPE_GET_MEMORY_PROFILE_REQ =
      MsgType._(66, _omitEnumNames ? '' : 'MSG_TYPE_GET_MEMORY_PROFILE_REQ');
  static const MsgType MSG_TYPE_GET_MEMORY_PROFILE_RSP =
      MsgType._(67, _omitEnumNames ? '' : 'MSG_TYPE_GET_MEMORY_PROFILE_RSP');

  /// Self-test / smoke test commands (0x44-0x45)
  static const MsgType MSG_TYPE_SELF_TEST_REQ =
      MsgType._(68, _omitEnumNames ? '' : 'MSG_TYPE_SELF_TEST_REQ');
  static const MsgType MSG_TYPE_SELF_TEST_RSP =
      MsgType._(69, _omitEnumNames ? '' : 'MSG_TYPE_SELF_TEST_RSP');

  /// GitHub OTA commands (0x46-0x49)
  static const MsgType MSG_TYPE_CHECK_UPDATE_REQ =
      MsgType._(70, _omitEnumNames ? '' : 'MSG_TYPE_CHECK_UPDATE_REQ');
  static const MsgType MSG_TYPE_CHECK_UPDATE_RSP =
      MsgType._(71, _omitEnumNames ? '' : 'MSG_TYPE_CHECK_UPDATE_RSP');
  static const MsgType MSG_TYPE_SET_AUTO_UPDATE_REQ =
      MsgType._(72, _omitEnumNames ? '' : 'MSG_TYPE_SET_AUTO_UPDATE_REQ');
  static const MsgType MSG_TYPE_SET_AUTO_UPDATE_RSP =
      MsgType._(73, _omitEnumNames ? '' : 'MSG_TYPE_SET_AUTO_UPDATE_RSP');

  /// Touch injection commands (0x4C-0x4D)
  static const MsgType MSG_TYPE_SIMULATE_TOUCH_REQ =
      MsgType._(76, _omitEnumNames ? '' : 'MSG_TYPE_SIMULATE_TOUCH_REQ');
  static const MsgType MSG_TYPE_SIMULATE_TOUCH_RSP =
      MsgType._(77, _omitEnumNames ? '' : 'MSG_TYPE_SIMULATE_TOUCH_RSP');

  /// Sim drill mode commands (0x4E-0x4F)
  static const MsgType MSG_TYPE_SET_SIM_MODE_REQ =
      MsgType._(78, _omitEnumNames ? '' : 'MSG_TYPE_SET_SIM_MODE_REQ');
  static const MsgType MSG_TYPE_SET_SIM_MODE_RSP =
      MsgType._(79, _omitEnumNames ? '' : 'MSG_TYPE_SET_SIM_MODE_RSP');

  /// Device-originated touch notification (0x50)
  static const MsgType MSG_TYPE_TOUCH_EVENT_NTF =
      MsgType._(80, _omitEnumNames ? '' : 'MSG_TYPE_TOUCH_EVENT_NTF');

  /// Bounded feedback interface commands (0x51-0x56)
  static const MsgType MSG_TYPE_GET_AUDIO_VOLUME_REQ =
      MsgType._(81, _omitEnumNames ? '' : 'MSG_TYPE_GET_AUDIO_VOLUME_REQ');
  static const MsgType MSG_TYPE_GET_AUDIO_VOLUME_RSP =
      MsgType._(82, _omitEnumNames ? '' : 'MSG_TYPE_GET_AUDIO_VOLUME_RSP');
  static const MsgType MSG_TYPE_SET_AUDIO_VOLUME_REQ =
      MsgType._(83, _omitEnumNames ? '' : 'MSG_TYPE_SET_AUDIO_VOLUME_REQ');
  static const MsgType MSG_TYPE_SET_AUDIO_VOLUME_RSP =
      MsgType._(84, _omitEnumNames ? '' : 'MSG_TYPE_SET_AUDIO_VOLUME_RSP');
  static const MsgType MSG_TYPE_TRIGGER_FEEDBACK_REQ =
      MsgType._(85, _omitEnumNames ? '' : 'MSG_TYPE_TRIGGER_FEEDBACK_REQ');
  static const MsgType MSG_TYPE_TRIGGER_FEEDBACK_RSP =
      MsgType._(86, _omitEnumNames ? '' : 'MSG_TYPE_TRIGGER_FEEDBACK_RSP');

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
    MSG_TYPE_SET_POD_ID_REQ,
    MSG_TYPE_SET_POD_ID_RSP,
    MSG_TYPE_GET_HEALTH_REQ,
    MSG_TYPE_GET_HEALTH_RSP,
    MSG_TYPE_GET_ESPNOW_STATUS_REQ,
    MSG_TYPE_GET_ESPNOW_STATUS_RSP,
    MSG_TYPE_ESPNOW_BENCH_REQ,
    MSG_TYPE_ESPNOW_BENCH_RSP,
    MSG_TYPE_GET_CRASH_DUMP_REQ,
    MSG_TYPE_GET_CRASH_DUMP_RSP,
    MSG_TYPE_CLEAR_CRASH_DUMP_REQ,
    MSG_TYPE_CLEAR_CRASH_DUMP_RSP,
    MSG_TYPE_GET_MEMORY_PROFILE_REQ,
    MSG_TYPE_GET_MEMORY_PROFILE_RSP,
    MSG_TYPE_SELF_TEST_REQ,
    MSG_TYPE_SELF_TEST_RSP,
    MSG_TYPE_CHECK_UPDATE_REQ,
    MSG_TYPE_CHECK_UPDATE_RSP,
    MSG_TYPE_SET_AUTO_UPDATE_REQ,
    MSG_TYPE_SET_AUTO_UPDATE_RSP,
    MSG_TYPE_SIMULATE_TOUCH_REQ,
    MSG_TYPE_SIMULATE_TOUCH_RSP,
    MSG_TYPE_SET_SIM_MODE_REQ,
    MSG_TYPE_SET_SIM_MODE_RSP,
    MSG_TYPE_TOUCH_EVENT_NTF,
    MSG_TYPE_GET_AUDIO_VOLUME_REQ,
    MSG_TYPE_GET_AUDIO_VOLUME_RSP,
    MSG_TYPE_SET_AUDIO_VOLUME_REQ,
    MSG_TYPE_SET_AUDIO_VOLUME_RSP,
    MSG_TYPE_TRIGGER_FEEDBACK_REQ,
    MSG_TYPE_TRIGGER_FEEDBACK_RSP,
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
  static const Status STATUS_NO_DATA =
      Status._(5, _omitEnumNames ? '' : 'STATUS_NO_DATA');
  static const Status STATUS_INVALID_VALUE =
      Status._(6, _omitEnumNames ? '' : 'STATUS_INVALID_VALUE');
  static const Status STATUS_DISABLED =
      Status._(7, _omitEnumNames ? '' : 'STATUS_DISABLED');
  static const Status STATUS_REJECTED =
      Status._(8, _omitEnumNames ? '' : 'STATUS_REJECTED');
  static const Status STATUS_STORAGE_ERROR =
      Status._(9, _omitEnumNames ? '' : 'STATUS_STORAGE_ERROR');

  static const $core.List<Status> values = <Status>[
    STATUS_OK,
    STATUS_ERROR,
    STATUS_INVALID_FEATURE,
    STATUS_BUSY,
    STATUS_INVALID_PATTERN,
    STATUS_NO_DATA,
    STATUS_INVALID_VALUE,
    STATUS_DISABLED,
    STATUS_REJECTED,
    STATUS_STORAGE_ERROR,
  ];

  static final $core.List<Status?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 9);
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

/// Known software feedback probes. Acceptance means that firmware queued or
/// triggered the request; it never represents sensed physical completion.
class FeedbackProbe extends $pb.ProtobufEnum {
  static const FeedbackProbe FEEDBACK_PROBE_UNKNOWN =
      FeedbackProbe._(0, _omitEnumNames ? '' : 'FEEDBACK_PROBE_UNKNOWN');
  static const FeedbackProbe FEEDBACK_PROBE_EMBEDDED_BEEP =
      FeedbackProbe._(1, _omitEnumNames ? '' : 'FEEDBACK_PROBE_EMBEDDED_BEEP');
  static const FeedbackProbe FEEDBACK_PROBE_FIXED_HAPTIC =
      FeedbackProbe._(2, _omitEnumNames ? '' : 'FEEDBACK_PROBE_FIXED_HAPTIC');

  static const $core.List<FeedbackProbe> values = <FeedbackProbe>[
    FEEDBACK_PROBE_UNKNOWN,
    FEEDBACK_PROBE_EMBEDDED_BEEP,
    FEEDBACK_PROBE_FIXED_HAPTIC,
  ];

  static final $core.List<FeedbackProbe?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 2);
  static FeedbackProbe? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const FeedbackProbe._(super.value, super.name);
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

/// Normalized device reset cause. Firmware maps ESP-IDF reset reasons at the
/// platform boundary; host clients consume this enum without duplicating IDs.
class ResetReason extends $pb.ProtobufEnum {
  static const ResetReason RESET_REASON_UNKNOWN =
      ResetReason._(0, _omitEnumNames ? '' : 'RESET_REASON_UNKNOWN');
  static const ResetReason RESET_REASON_POWER_ON =
      ResetReason._(1, _omitEnumNames ? '' : 'RESET_REASON_POWER_ON');
  static const ResetReason RESET_REASON_EXTERNAL_PIN =
      ResetReason._(2, _omitEnumNames ? '' : 'RESET_REASON_EXTERNAL_PIN');
  static const ResetReason RESET_REASON_SOFTWARE =
      ResetReason._(3, _omitEnumNames ? '' : 'RESET_REASON_SOFTWARE');
  static const ResetReason RESET_REASON_PANIC =
      ResetReason._(4, _omitEnumNames ? '' : 'RESET_REASON_PANIC');
  static const ResetReason RESET_REASON_INTERRUPT_WATCHDOG =
      ResetReason._(5, _omitEnumNames ? '' : 'RESET_REASON_INTERRUPT_WATCHDOG');
  static const ResetReason RESET_REASON_TASK_WATCHDOG =
      ResetReason._(6, _omitEnumNames ? '' : 'RESET_REASON_TASK_WATCHDOG');
  static const ResetReason RESET_REASON_WATCHDOG =
      ResetReason._(7, _omitEnumNames ? '' : 'RESET_REASON_WATCHDOG');
  static const ResetReason RESET_REASON_DEEP_SLEEP =
      ResetReason._(8, _omitEnumNames ? '' : 'RESET_REASON_DEEP_SLEEP');
  static const ResetReason RESET_REASON_BROWNOUT =
      ResetReason._(9, _omitEnumNames ? '' : 'RESET_REASON_BROWNOUT');
  static const ResetReason RESET_REASON_SDIO =
      ResetReason._(10, _omitEnumNames ? '' : 'RESET_REASON_SDIO');
  static const ResetReason RESET_REASON_USB =
      ResetReason._(11, _omitEnumNames ? '' : 'RESET_REASON_USB');
  static const ResetReason RESET_REASON_JTAG =
      ResetReason._(12, _omitEnumNames ? '' : 'RESET_REASON_JTAG');
  static const ResetReason RESET_REASON_EFUSE =
      ResetReason._(13, _omitEnumNames ? '' : 'RESET_REASON_EFUSE');
  static const ResetReason RESET_REASON_POWER_GLITCH =
      ResetReason._(14, _omitEnumNames ? '' : 'RESET_REASON_POWER_GLITCH');
  static const ResetReason RESET_REASON_CPU_LOCKUP =
      ResetReason._(15, _omitEnumNames ? '' : 'RESET_REASON_CPU_LOCKUP');

  static const $core.List<ResetReason> values = <ResetReason>[
    RESET_REASON_UNKNOWN,
    RESET_REASON_POWER_ON,
    RESET_REASON_EXTERNAL_PIN,
    RESET_REASON_SOFTWARE,
    RESET_REASON_PANIC,
    RESET_REASON_INTERRUPT_WATCHDOG,
    RESET_REASON_TASK_WATCHDOG,
    RESET_REASON_WATCHDOG,
    RESET_REASON_DEEP_SLEEP,
    RESET_REASON_BROWNOUT,
    RESET_REASON_SDIO,
    RESET_REASON_USB,
    RESET_REASON_JTAG,
    RESET_REASON_EFUSE,
    RESET_REASON_POWER_GLITCH,
    RESET_REASON_CPU_LOCKUP,
  ];

  static final $core.List<ResetReason?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 15);
  static ResetReason? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const ResetReason._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
