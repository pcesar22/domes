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

/// Existing feedback bitmask values. Values outside this enum are invalid.
class FeedbackMode extends $pb.ProtobufEnum {
  static const FeedbackMode FEEDBACK_MODE_NONE =
      FeedbackMode._(0, _omitEnumNames ? '' : 'FEEDBACK_MODE_NONE');
  static const FeedbackMode FEEDBACK_MODE_LED =
      FeedbackMode._(1, _omitEnumNames ? '' : 'FEEDBACK_MODE_LED');
  static const FeedbackMode FEEDBACK_MODE_AUDIO =
      FeedbackMode._(2, _omitEnumNames ? '' : 'FEEDBACK_MODE_AUDIO');
  static const FeedbackMode FEEDBACK_MODE_LED_AND_AUDIO =
      FeedbackMode._(3, _omitEnumNames ? '' : 'FEEDBACK_MODE_LED_AND_AUDIO');

  static const $core.List<FeedbackMode> values = <FeedbackMode>[
    FEEDBACK_MODE_NONE,
    FEEDBACK_MODE_LED,
    FEEDBACK_MODE_AUDIO,
    FEEDBACK_MODE_LED_AND_AUDIO,
  ];

  static final $core.List<FeedbackMode?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 3);
  static FeedbackMode? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const FeedbackMode._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
