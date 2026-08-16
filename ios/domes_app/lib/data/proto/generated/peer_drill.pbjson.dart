// This is a generated file - do not edit.
//
// Generated from peer_drill.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports
// ignore_for_file: unused_import

import 'dart:convert' as $convert;
import 'dart:core' as $core;
import 'dart:typed_data' as $typed_data;

@$core.Deprecated('Use contractVersionDescriptor instead')
const ContractVersion$json = {
  '1': 'ContractVersion',
  '2': [
    {'1': 'CONTRACT_VERSION_UNSPECIFIED', '2': 0},
    {'1': 'CONTRACT_VERSION_1', '2': 1},
  ],
};

/// Descriptor for `ContractVersion`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List contractVersionDescriptor = $convert.base64Decode(
    'Cg9Db250cmFjdFZlcnNpb24SIAocQ09OVFJBQ1RfVkVSU0lPTl9VTlNQRUNJRklFRBAAEhYKEk'
    'NPTlRSQUNUX1ZFUlNJT05fMRAB');

@$core.Deprecated('Use peerRoleDescriptor instead')
const PeerRole$json = {
  '1': 'PeerRole',
  '2': [
    {'1': 'PEER_ROLE_UNSPECIFIED', '2': 0},
    {'1': 'PEER_ROLE_MASTER', '2': 1},
    {'1': 'PEER_ROLE_SLAVE', '2': 2},
  ],
};

/// Descriptor for `PeerRole`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List peerRoleDescriptor = $convert.base64Decode(
    'CghQZWVyUm9sZRIZChVQRUVSX1JPTEVfVU5TUEVDSUZJRUQQABIUChBQRUVSX1JPTEVfTUFTVE'
    'VSEAESEwoPUEVFUl9ST0xFX1NMQVZFEAI=');

@$core.Deprecated('Use peerLifecycleStateDescriptor instead')
const PeerLifecycleState$json = {
  '1': 'PeerLifecycleState',
  '2': [
    {'1': 'PEER_LIFECYCLE_STATE_UNSPECIFIED', '2': 0},
    {'1': 'PEER_LIFECYCLE_STATE_DISCOVERY', '2': 1},
    {'1': 'PEER_LIFECYCLE_STATE_READY', '2': 2},
    {'1': 'PEER_LIFECYCLE_STATE_ARMED', '2': 3},
    {'1': 'PEER_LIFECYCLE_STATE_STOPPED', '2': 4},
  ],
};

/// Descriptor for `PeerLifecycleState`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List peerLifecycleStateDescriptor = $convert.base64Decode(
    'ChJQZWVyTGlmZWN5Y2xlU3RhdGUSJAogUEVFUl9MSUZFQ1lDTEVfU1RBVEVfVU5TUEVDSUZJRU'
    'QQABIiCh5QRUVSX0xJRkVDWUNMRV9TVEFURV9ESVNDT1ZFUlkQARIeChpQRUVSX0xJRkVDWUNM'
    'RV9TVEFURV9SRUFEWRACEh4KGlBFRVJfTElGRUNZQ0xFX1NUQVRFX0FSTUVEEAMSIAocUEVFUl'
    '9MSUZFQ1lDTEVfU1RBVEVfU1RPUFBFRBAE');

@$core.Deprecated('Use peerMessageTypeDescriptor instead')
const PeerMessageType$json = {
  '1': 'PeerMessageType',
  '2': [
    {'1': 'PEER_MESSAGE_TYPE_UNKNOWN', '2': 0},
    {'1': 'PEER_MESSAGE_TYPE_BEACON', '2': 1},
    {'1': 'PEER_MESSAGE_TYPE_PING', '2': 2},
    {'1': 'PEER_MESSAGE_TYPE_PONG', '2': 3},
    {'1': 'PEER_MESSAGE_TYPE_JOIN_GAME', '2': 16},
    {'1': 'PEER_MESSAGE_TYPE_ARM_TOUCH', '2': 17},
    {'1': 'PEER_MESSAGE_TYPE_SET_COLOR', '2': 18},
    {'1': 'PEER_MESSAGE_TYPE_STOP_ALL', '2': 19},
    {'1': 'PEER_MESSAGE_TYPE_SIMULATE_TOUCH', '2': 20},
    {'1': 'PEER_MESSAGE_TYPE_TOUCH_EVENT', '2': 32},
    {'1': 'PEER_MESSAGE_TYPE_TIMEOUT_EVENT', '2': 33},
  ],
};

/// Descriptor for `PeerMessageType`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List peerMessageTypeDescriptor = $convert.base64Decode(
    'Cg9QZWVyTWVzc2FnZVR5cGUSHQoZUEVFUl9NRVNTQUdFX1RZUEVfVU5LTk9XThAAEhwKGFBFRV'
    'JfTUVTU0FHRV9UWVBFX0JFQUNPThABEhoKFlBFRVJfTUVTU0FHRV9UWVBFX1BJTkcQAhIaChZQ'
    'RUVSX01FU1NBR0VfVFlQRV9QT05HEAMSHwobUEVFUl9NRVNTQUdFX1RZUEVfSk9JTl9HQU1FEB'
    'ASHwobUEVFUl9NRVNTQUdFX1RZUEVfQVJNX1RPVUNIEBESHwobUEVFUl9NRVNTQUdFX1RZUEVf'
    'U0VUX0NPTE9SEBISHgoaUEVFUl9NRVNTQUdFX1RZUEVfU1RPUF9BTEwQExIkCiBQRUVSX01FU1'
    'NBR0VfVFlQRV9TSU1VTEFURV9UT1VDSBAUEiEKHVBFRVJfTUVTU0FHRV9UWVBFX1RPVUNIX0VW'
    'RU5UECASIwofUEVFUl9NRVNTQUdFX1RZUEVfVElNRU9VVF9FVkVOVBAh');

@$core.Deprecated('Use peerHeaderDescriptor instead')
const PeerHeader$json = {
  '1': 'PeerHeader',
  '2': [
    {
      '1': 'version',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.peer.ContractVersion',
      '10': 'version'
    },
    {'1': 'src_pod_id', '3': 2, '4': 1, '5': 13, '10': 'srcPodId'},
    {'1': 'dst_pod_id', '3': 3, '4': 1, '5': 13, '10': 'dstPodId'},
    {
      '1': 'sender_role',
      '3': 4,
      '4': 1,
      '5': 14,
      '6': '.domes.peer.PeerRole',
      '10': 'senderRole'
    },
    {'1': 'timestamp_us', '3': 5, '4': 1, '5': 4, '10': 'timestampUs'},
    {'1': 'sequence', '3': 6, '4': 1, '5': 13, '10': 'sequence'},
    {'1': 'sender_mac', '3': 7, '4': 1, '5': 12, '10': 'senderMac'},
  ],
};

/// Descriptor for `PeerHeader`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List peerHeaderDescriptor = $convert.base64Decode(
    'CgpQZWVySGVhZGVyEjUKB3ZlcnNpb24YASABKA4yGy5kb21lcy5wZWVyLkNvbnRyYWN0VmVyc2'
    'lvblIHdmVyc2lvbhIcCgpzcmNfcG9kX2lkGAIgASgNUghzcmNQb2RJZBIcCgpkc3RfcG9kX2lk'
    'GAMgASgNUghkc3RQb2RJZBI1CgtzZW5kZXJfcm9sZRgEIAEoDjIULmRvbWVzLnBlZXIuUGVlcl'
    'JvbGVSCnNlbmRlclJvbGUSIQoMdGltZXN0YW1wX3VzGAUgASgEUgt0aW1lc3RhbXBVcxIaCghz'
    'ZXF1ZW5jZRgGIAEoDVIIc2VxdWVuY2USHQoKc2VuZGVyX21hYxgHIAEoDFIJc2VuZGVyTWFj');

@$core.Deprecated('Use beaconDescriptor instead')
const Beacon$json = {
  '1': 'Beacon',
};

/// Descriptor for `Beacon`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List beaconDescriptor =
    $convert.base64Decode('CgZCZWFjb24=');

@$core.Deprecated('Use pingDescriptor instead')
const Ping$json = {
  '1': 'Ping',
};

/// Descriptor for `Ping`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List pingDescriptor = $convert.base64Decode('CgRQaW5n');

@$core.Deprecated('Use pongDescriptor instead')
const Pong$json = {
  '1': 'Pong',
};

/// Descriptor for `Pong`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List pongDescriptor = $convert.base64Decode('CgRQb25n');

@$core.Deprecated('Use joinGameDescriptor instead')
const JoinGame$json = {
  '1': 'JoinGame',
  '2': [
    {
      '1': 'assigned_role',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.peer.PeerRole',
      '10': 'assignedRole'
    },
  ],
};

/// Descriptor for `JoinGame`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List joinGameDescriptor = $convert.base64Decode(
    'CghKb2luR2FtZRI5Cg1hc3NpZ25lZF9yb2xlGAEgASgOMhQuZG9tZXMucGVlci5QZWVyUm9sZV'
    'IMYXNzaWduZWRSb2xl');

@$core.Deprecated('Use armTouchDescriptor instead')
const ArmTouch$json = {
  '1': 'ArmTouch',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 13, '10': 'roundToken'},
    {'1': 'timeout_ms', '3': 2, '4': 1, '5': 13, '10': 'timeoutMs'},
    {'1': 'feedback_mode', '3': 3, '4': 1, '5': 13, '10': 'feedbackMode'},
  ],
};

/// Descriptor for `ArmTouch`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List armTouchDescriptor = $convert.base64Decode(
    'CghBcm1Ub3VjaBIfCgtyb3VuZF90b2tlbhgBIAEoDVIKcm91bmRUb2tlbhIdCgp0aW1lb3V0X2'
    '1zGAIgASgNUgl0aW1lb3V0TXMSIwoNZmVlZGJhY2tfbW9kZRgDIAEoDVIMZmVlZGJhY2tNb2Rl');

@$core.Deprecated('Use setColorDescriptor instead')
const SetColor$json = {
  '1': 'SetColor',
  '2': [
    {'1': 'r', '3': 1, '4': 1, '5': 13, '10': 'r'},
    {'1': 'g', '3': 2, '4': 1, '5': 13, '10': 'g'},
    {'1': 'b', '3': 3, '4': 1, '5': 13, '10': 'b'},
  ],
};

/// Descriptor for `SetColor`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setColorDescriptor = $convert.base64Decode(
    'CghTZXRDb2xvchIMCgFyGAEgASgNUgFyEgwKAWcYAiABKA1SAWcSDAoBYhgDIAEoDVIBYg==');

@$core.Deprecated('Use stopAllDescriptor instead')
const StopAll$json = {
  '1': 'StopAll',
};

/// Descriptor for `StopAll`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List stopAllDescriptor =
    $convert.base64Decode('CgdTdG9wQWxs');

@$core.Deprecated('Use simulateTouchDescriptor instead')
const SimulateTouch$json = {
  '1': 'SimulateTouch',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 13, '10': 'roundToken'},
    {'1': 'pad_index', '3': 2, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `SimulateTouch`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List simulateTouchDescriptor = $convert.base64Decode(
    'Cg1TaW11bGF0ZVRvdWNoEh8KC3JvdW5kX3Rva2VuGAEgASgNUgpyb3VuZFRva2VuEhsKCXBhZF'
    '9pbmRleBgCIAEoDVIIcGFkSW5kZXg=');

@$core.Deprecated('Use touchEventDescriptor instead')
const TouchEvent$json = {
  '1': 'TouchEvent',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 13, '10': 'roundToken'},
    {'1': 'reaction_time_us', '3': 2, '4': 1, '5': 13, '10': 'reactionTimeUs'},
    {'1': 'pad_index', '3': 3, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `TouchEvent`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List touchEventDescriptor = $convert.base64Decode(
    'CgpUb3VjaEV2ZW50Eh8KC3JvdW5kX3Rva2VuGAEgASgNUgpyb3VuZFRva2VuEigKEHJlYWN0aW'
    '9uX3RpbWVfdXMYAiABKA1SDnJlYWN0aW9uVGltZVVzEhsKCXBhZF9pbmRleBgDIAEoDVIIcGFk'
    'SW5kZXg=');

@$core.Deprecated('Use timeoutEventDescriptor instead')
const TimeoutEvent$json = {
  '1': 'TimeoutEvent',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 13, '10': 'roundToken'},
  ],
};

/// Descriptor for `TimeoutEvent`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List timeoutEventDescriptor = $convert.base64Decode(
    'CgxUaW1lb3V0RXZlbnQSHwoLcm91bmRfdG9rZW4YASABKA1SCnJvdW5kVG9rZW4=');

@$core.Deprecated('Use peerMessageDescriptor instead')
const PeerMessage$json = {
  '1': 'PeerMessage',
  '2': [
    {
      '1': 'header',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.PeerHeader',
      '10': 'header'
    },
    {
      '1': 'beacon',
      '3': 16,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.Beacon',
      '9': 0,
      '10': 'beacon'
    },
    {
      '1': 'ping',
      '3': 17,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.Ping',
      '9': 0,
      '10': 'ping'
    },
    {
      '1': 'pong',
      '3': 18,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.Pong',
      '9': 0,
      '10': 'pong'
    },
    {
      '1': 'join_game',
      '3': 19,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.JoinGame',
      '9': 0,
      '10': 'joinGame'
    },
    {
      '1': 'arm_touch',
      '3': 20,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.ArmTouch',
      '9': 0,
      '10': 'armTouch'
    },
    {
      '1': 'set_color',
      '3': 21,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.SetColor',
      '9': 0,
      '10': 'setColor'
    },
    {
      '1': 'stop_all',
      '3': 22,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.StopAll',
      '9': 0,
      '10': 'stopAll'
    },
    {
      '1': 'simulate_touch',
      '3': 23,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.SimulateTouch',
      '9': 0,
      '10': 'simulateTouch'
    },
    {
      '1': 'touch_event',
      '3': 24,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.TouchEvent',
      '9': 0,
      '10': 'touchEvent'
    },
    {
      '1': 'timeout_event',
      '3': 25,
      '4': 1,
      '5': 11,
      '6': '.domes.peer.TimeoutEvent',
      '9': 0,
      '10': 'timeoutEvent'
    },
  ],
  '8': [
    {'1': 'payload'},
  ],
};

/// Descriptor for `PeerMessage`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List peerMessageDescriptor = $convert.base64Decode(
    'CgtQZWVyTWVzc2FnZRIuCgZoZWFkZXIYASABKAsyFi5kb21lcy5wZWVyLlBlZXJIZWFkZXJSBm'
    'hlYWRlchIsCgZiZWFjb24YECABKAsyEi5kb21lcy5wZWVyLkJlYWNvbkgAUgZiZWFjb24SJgoE'
    'cGluZxgRIAEoCzIQLmRvbWVzLnBlZXIuUGluZ0gAUgRwaW5nEiYKBHBvbmcYEiABKAsyEC5kb2'
    '1lcy5wZWVyLlBvbmdIAFIEcG9uZxIzCglqb2luX2dhbWUYEyABKAsyFC5kb21lcy5wZWVyLkpv'
    'aW5HYW1lSABSCGpvaW5HYW1lEjMKCWFybV90b3VjaBgUIAEoCzIULmRvbWVzLnBlZXIuQXJtVG'
    '91Y2hIAFIIYXJtVG91Y2gSMwoJc2V0X2NvbG9yGBUgASgLMhQuZG9tZXMucGVlci5TZXRDb2xv'
    'ckgAUghzZXRDb2xvchIwCghzdG9wX2FsbBgWIAEoCzITLmRvbWVzLnBlZXIuU3RvcEFsbEgAUg'
    'dzdG9wQWxsEkIKDnNpbXVsYXRlX3RvdWNoGBcgASgLMhkuZG9tZXMucGVlci5TaW11bGF0ZVRv'
    'dWNoSABSDXNpbXVsYXRlVG91Y2gSOQoLdG91Y2hfZXZlbnQYGCABKAsyFi5kb21lcy5wZWVyLl'
    'RvdWNoRXZlbnRIAFIKdG91Y2hFdmVudBI/Cg10aW1lb3V0X2V2ZW50GBkgASgLMhguZG9tZXMu'
    'cGVlci5UaW1lb3V0RXZlbnRIAFIMdGltZW91dEV2ZW50QgkKB3BheWxvYWQ=');
