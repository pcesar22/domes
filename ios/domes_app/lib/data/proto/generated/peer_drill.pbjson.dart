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

@$core.Deprecated('Use feedbackModeDescriptor instead')
const FeedbackMode$json = {
  '1': 'FeedbackMode',
  '2': [
    {'1': 'FEEDBACK_MODE_NONE', '2': 0},
    {'1': 'FEEDBACK_MODE_LED', '2': 1},
    {'1': 'FEEDBACK_MODE_AUDIO', '2': 2},
    {'1': 'FEEDBACK_MODE_LED_AND_AUDIO', '2': 3},
  ],
};

/// Descriptor for `FeedbackMode`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List feedbackModeDescriptor = $convert.base64Decode(
    'CgxGZWVkYmFja01vZGUSFgoSRkVFREJBQ0tfTU9ERV9OT05FEAASFQoRRkVFREJBQ0tfTU9ERV'
    '9MRUQQARIXChNGRUVEQkFDS19NT0RFX0FVRElPEAISHwobRkVFREJBQ0tfTU9ERV9MRURfQU5E'
    'X0FVRElPEAM=');

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
};

/// Descriptor for `JoinGame`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List joinGameDescriptor =
    $convert.base64Decode('CghKb2luR2FtZQ==');

@$core.Deprecated('Use stopAllDescriptor instead')
const StopAll$json = {
  '1': 'StopAll',
};

/// Descriptor for `StopAll`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List stopAllDescriptor =
    $convert.base64Decode('CgdTdG9wQWxs');

@$core.Deprecated('Use armTouchDescriptor instead')
const ArmTouch$json = {
  '1': 'ArmTouch',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 7, '10': 'roundToken'},
    {'1': 'timeout_ms', '3': 2, '4': 1, '5': 13, '10': 'timeoutMs'},
    {
      '1': 'feedback_mode',
      '3': 3,
      '4': 1,
      '5': 14,
      '6': '.domes.peer_drill.FeedbackMode',
      '10': 'feedbackMode'
    },
  ],
};

/// Descriptor for `ArmTouch`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List armTouchDescriptor = $convert.base64Decode(
    'CghBcm1Ub3VjaBIfCgtyb3VuZF90b2tlbhgBIAEoB1IKcm91bmRUb2tlbhIdCgp0aW1lb3V0X2'
    '1zGAIgASgNUgl0aW1lb3V0TXMSQwoNZmVlZGJhY2tfbW9kZRgDIAEoDjIeLmRvbWVzLnBlZXJf'
    'ZHJpbGwuRmVlZGJhY2tNb2RlUgxmZWVkYmFja01vZGU=');

@$core.Deprecated('Use setColorDescriptor instead')
const SetColor$json = {
  '1': 'SetColor',
  '2': [
    {'1': 'red', '3': 1, '4': 1, '5': 13, '10': 'red'},
    {'1': 'green', '3': 2, '4': 1, '5': 13, '10': 'green'},
    {'1': 'blue', '3': 3, '4': 1, '5': 13, '10': 'blue'},
  ],
};

/// Descriptor for `SetColor`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setColorDescriptor = $convert.base64Decode(
    'CghTZXRDb2xvchIQCgNyZWQYASABKA1SA3JlZBIUCgVncmVlbhgCIAEoDVIFZ3JlZW4SEgoEYm'
    'x1ZRgDIAEoDVIEYmx1ZQ==');

@$core.Deprecated('Use simulateTouchDescriptor instead')
const SimulateTouch$json = {
  '1': 'SimulateTouch',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 7, '10': 'roundToken'},
    {'1': 'pad_index', '3': 2, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `SimulateTouch`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List simulateTouchDescriptor = $convert.base64Decode(
    'Cg1TaW11bGF0ZVRvdWNoEh8KC3JvdW5kX3Rva2VuGAEgASgHUgpyb3VuZFRva2VuEhsKCXBhZF'
    '9pbmRleBgCIAEoDVIIcGFkSW5kZXg=');

@$core.Deprecated('Use touchEventDescriptor instead')
const TouchEvent$json = {
  '1': 'TouchEvent',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 7, '10': 'roundToken'},
    {'1': 'reaction_time_us', '3': 2, '4': 1, '5': 13, '10': 'reactionTimeUs'},
    {'1': 'pad_index', '3': 3, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `TouchEvent`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List touchEventDescriptor = $convert.base64Decode(
    'CgpUb3VjaEV2ZW50Eh8KC3JvdW5kX3Rva2VuGAEgASgHUgpyb3VuZFRva2VuEigKEHJlYWN0aW'
    '9uX3RpbWVfdXMYAiABKA1SDnJlYWN0aW9uVGltZVVzEhsKCXBhZF9pbmRleBgDIAEoDVIIcGFk'
    'SW5kZXg=');

@$core.Deprecated('Use timeoutEventDescriptor instead')
const TimeoutEvent$json = {
  '1': 'TimeoutEvent',
  '2': [
    {'1': 'round_token', '3': 1, '4': 1, '5': 7, '10': 'roundToken'},
  ],
};

/// Descriptor for `TimeoutEvent`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List timeoutEventDescriptor = $convert.base64Decode(
    'CgxUaW1lb3V0RXZlbnQSHwoLcm91bmRfdG9rZW4YASABKAdSCnJvdW5kVG9rZW4=');

@$core.Deprecated('Use peerMessageDescriptor instead')
const PeerMessage$json = {
  '1': 'PeerMessage',
  '2': [
    {
      '1': 'beacon',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.Beacon',
      '9': 0,
      '10': 'beacon'
    },
    {
      '1': 'ping',
      '3': 2,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.Ping',
      '9': 0,
      '10': 'ping'
    },
    {
      '1': 'pong',
      '3': 3,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.Pong',
      '9': 0,
      '10': 'pong'
    },
    {
      '1': 'join_game',
      '3': 16,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.JoinGame',
      '9': 0,
      '10': 'joinGame'
    },
    {
      '1': 'arm_touch',
      '3': 17,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.ArmTouch',
      '9': 0,
      '10': 'armTouch'
    },
    {
      '1': 'set_color',
      '3': 18,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.SetColor',
      '9': 0,
      '10': 'setColor'
    },
    {
      '1': 'stop_all',
      '3': 19,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.StopAll',
      '9': 0,
      '10': 'stopAll'
    },
    {
      '1': 'simulate_touch',
      '3': 20,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.SimulateTouch',
      '9': 0,
      '10': 'simulateTouch'
    },
    {
      '1': 'touch_event',
      '3': 32,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.TouchEvent',
      '9': 0,
      '10': 'touchEvent'
    },
    {
      '1': 'timeout_event',
      '3': 33,
      '4': 1,
      '5': 11,
      '6': '.domes.peer_drill.TimeoutEvent',
      '9': 0,
      '10': 'timeoutEvent'
    },
    {
      '1': 'protocol_version',
      '3': 256,
      '4': 1,
      '5': 13,
      '10': 'protocolVersion'
    },
    {'1': 'sender_mac', '3': 257, '4': 1, '5': 12, '10': 'senderMac'},
    {'1': 'timestamp_us', '3': 258, '4': 1, '5': 7, '10': 'timestampUs'},
  ],
  '8': [
    {'1': 'payload'},
  ],
  '9': [
    {'1': 4, '2': 16},
    {'1': 21, '2': 32},
  ],
};

/// Descriptor for `PeerMessage`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List peerMessageDescriptor = $convert.base64Decode(
    'CgtQZWVyTWVzc2FnZRIyCgZiZWFjb24YASABKAsyGC5kb21lcy5wZWVyX2RyaWxsLkJlYWNvbk'
    'gAUgZiZWFjb24SLAoEcGluZxgCIAEoCzIWLmRvbWVzLnBlZXJfZHJpbGwuUGluZ0gAUgRwaW5n'
    'EiwKBHBvbmcYAyABKAsyFi5kb21lcy5wZWVyX2RyaWxsLlBvbmdIAFIEcG9uZxI5Cglqb2luX2'
    'dhbWUYECABKAsyGi5kb21lcy5wZWVyX2RyaWxsLkpvaW5HYW1lSABSCGpvaW5HYW1lEjkKCWFy'
    'bV90b3VjaBgRIAEoCzIaLmRvbWVzLnBlZXJfZHJpbGwuQXJtVG91Y2hIAFIIYXJtVG91Y2gSOQ'
    'oJc2V0X2NvbG9yGBIgASgLMhouZG9tZXMucGVlcl9kcmlsbC5TZXRDb2xvckgAUghzZXRDb2xv'
    'chI2CghzdG9wX2FsbBgTIAEoCzIZLmRvbWVzLnBlZXJfZHJpbGwuU3RvcEFsbEgAUgdzdG9wQW'
    'xsEkgKDnNpbXVsYXRlX3RvdWNoGBQgASgLMh8uZG9tZXMucGVlcl9kcmlsbC5TaW11bGF0ZVRv'
    'dWNoSABSDXNpbXVsYXRlVG91Y2gSPwoLdG91Y2hfZXZlbnQYICABKAsyHC5kb21lcy5wZWVyX2'
    'RyaWxsLlRvdWNoRXZlbnRIAFIKdG91Y2hFdmVudBJFCg10aW1lb3V0X2V2ZW50GCEgASgLMh4u'
    'ZG9tZXMucGVlcl9kcmlsbC5UaW1lb3V0RXZlbnRIAFIMdGltZW91dEV2ZW50EioKEHByb3RvY2'
    '9sX3ZlcnNpb24YgAIgASgNUg9wcm90b2NvbFZlcnNpb24SHgoKc2VuZGVyX21hYxiBAiABKAxS'
    'CXNlbmRlck1hYxIiCgx0aW1lc3RhbXBfdXMYggIgASgHUgt0aW1lc3RhbXBVc0IJCgdwYXlsb2'
    'FkSgQIBBAQSgQIFRAg');
