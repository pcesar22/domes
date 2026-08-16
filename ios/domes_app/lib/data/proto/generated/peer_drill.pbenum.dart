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

class ContractVersion extends $pb.ProtobufEnum {
  static const ContractVersion CONTRACT_VERSION_UNSPECIFIED = ContractVersion._(
      0, _omitEnumNames ? '' : 'CONTRACT_VERSION_UNSPECIFIED');
  static const ContractVersion CONTRACT_VERSION_1 =
      ContractVersion._(1, _omitEnumNames ? '' : 'CONTRACT_VERSION_1');

  static const $core.List<ContractVersion> values = <ContractVersion>[
    CONTRACT_VERSION_UNSPECIFIED,
    CONTRACT_VERSION_1,
  ];

  static final $core.List<ContractVersion?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 1);
  static ContractVersion? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const ContractVersion._(super.value, super.name);
}

class PeerRole extends $pb.ProtobufEnum {
  static const PeerRole PEER_ROLE_UNSPECIFIED =
      PeerRole._(0, _omitEnumNames ? '' : 'PEER_ROLE_UNSPECIFIED');
  static const PeerRole PEER_ROLE_MASTER =
      PeerRole._(1, _omitEnumNames ? '' : 'PEER_ROLE_MASTER');
  static const PeerRole PEER_ROLE_SLAVE =
      PeerRole._(2, _omitEnumNames ? '' : 'PEER_ROLE_SLAVE');

  static const $core.List<PeerRole> values = <PeerRole>[
    PEER_ROLE_UNSPECIFIED,
    PEER_ROLE_MASTER,
    PEER_ROLE_SLAVE,
  ];

  static final $core.List<PeerRole?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 2);
  static PeerRole? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const PeerRole._(super.value, super.name);
}

class PeerLifecycleState extends $pb.ProtobufEnum {
  static const PeerLifecycleState PEER_LIFECYCLE_STATE_UNSPECIFIED =
      PeerLifecycleState._(
          0, _omitEnumNames ? '' : 'PEER_LIFECYCLE_STATE_UNSPECIFIED');
  static const PeerLifecycleState PEER_LIFECYCLE_STATE_DISCOVERY =
      PeerLifecycleState._(
          1, _omitEnumNames ? '' : 'PEER_LIFECYCLE_STATE_DISCOVERY');
  static const PeerLifecycleState PEER_LIFECYCLE_STATE_READY =
      PeerLifecycleState._(
          2, _omitEnumNames ? '' : 'PEER_LIFECYCLE_STATE_READY');
  static const PeerLifecycleState PEER_LIFECYCLE_STATE_ARMED =
      PeerLifecycleState._(
          3, _omitEnumNames ? '' : 'PEER_LIFECYCLE_STATE_ARMED');
  static const PeerLifecycleState PEER_LIFECYCLE_STATE_STOPPED =
      PeerLifecycleState._(
          4, _omitEnumNames ? '' : 'PEER_LIFECYCLE_STATE_STOPPED');

  static const $core.List<PeerLifecycleState> values = <PeerLifecycleState>[
    PEER_LIFECYCLE_STATE_UNSPECIFIED,
    PEER_LIFECYCLE_STATE_DISCOVERY,
    PEER_LIFECYCLE_STATE_READY,
    PEER_LIFECYCLE_STATE_ARMED,
    PEER_LIFECYCLE_STATE_STOPPED,
  ];

  static final $core.List<PeerLifecycleState?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 4);
  static PeerLifecycleState? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const PeerLifecycleState._(super.value, super.name);
}

/// Values deliberately match the deployed ESP-NOW type byte.
class PeerMessageType extends $pb.ProtobufEnum {
  static const PeerMessageType PEER_MESSAGE_TYPE_UNKNOWN =
      PeerMessageType._(0, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_UNKNOWN');
  static const PeerMessageType PEER_MESSAGE_TYPE_BEACON =
      PeerMessageType._(1, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_BEACON');
  static const PeerMessageType PEER_MESSAGE_TYPE_PING =
      PeerMessageType._(2, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_PING');
  static const PeerMessageType PEER_MESSAGE_TYPE_PONG =
      PeerMessageType._(3, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_PONG');
  static const PeerMessageType PEER_MESSAGE_TYPE_JOIN_GAME = PeerMessageType._(
      16, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_JOIN_GAME');
  static const PeerMessageType PEER_MESSAGE_TYPE_ARM_TOUCH = PeerMessageType._(
      17, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_ARM_TOUCH');
  static const PeerMessageType PEER_MESSAGE_TYPE_SET_COLOR = PeerMessageType._(
      18, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_SET_COLOR');
  static const PeerMessageType PEER_MESSAGE_TYPE_STOP_ALL =
      PeerMessageType._(19, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_STOP_ALL');
  static const PeerMessageType PEER_MESSAGE_TYPE_SIMULATE_TOUCH =
      PeerMessageType._(
          20, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_SIMULATE_TOUCH');
  static const PeerMessageType PEER_MESSAGE_TYPE_TOUCH_EVENT =
      PeerMessageType._(
          32, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_TOUCH_EVENT');
  static const PeerMessageType PEER_MESSAGE_TYPE_TIMEOUT_EVENT =
      PeerMessageType._(
          33, _omitEnumNames ? '' : 'PEER_MESSAGE_TYPE_TIMEOUT_EVENT');

  static const $core.List<PeerMessageType> values = <PeerMessageType>[
    PEER_MESSAGE_TYPE_UNKNOWN,
    PEER_MESSAGE_TYPE_BEACON,
    PEER_MESSAGE_TYPE_PING,
    PEER_MESSAGE_TYPE_PONG,
    PEER_MESSAGE_TYPE_JOIN_GAME,
    PEER_MESSAGE_TYPE_ARM_TOUCH,
    PEER_MESSAGE_TYPE_SET_COLOR,
    PEER_MESSAGE_TYPE_STOP_ALL,
    PEER_MESSAGE_TYPE_SIMULATE_TOUCH,
    PEER_MESSAGE_TYPE_TOUCH_EVENT,
    PEER_MESSAGE_TYPE_TIMEOUT_EVENT,
  ];

  static final $core.Map<$core.int, PeerMessageType> _byValue =
      $pb.ProtobufEnum.initByValue(values);
  static PeerMessageType? valueOf($core.int value) => _byValue[value];

  const PeerMessageType._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
