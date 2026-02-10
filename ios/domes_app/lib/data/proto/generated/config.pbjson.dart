// This is a generated file - do not edit.
//
// Generated from config.proto.

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

@$core.Deprecated('Use msgTypeDescriptor instead')
const MsgType$json = {
  '1': 'MsgType',
  '2': [
    {'1': 'MSG_TYPE_UNKNOWN', '2': 0},
    {'1': 'MSG_TYPE_LIST_FEATURES_REQ', '2': 32},
    {'1': 'MSG_TYPE_LIST_FEATURES_RSP', '2': 33},
    {'1': 'MSG_TYPE_SET_FEATURE_REQ', '2': 34},
    {'1': 'MSG_TYPE_SET_FEATURE_RSP', '2': 35},
    {'1': 'MSG_TYPE_GET_FEATURE_REQ', '2': 36},
    {'1': 'MSG_TYPE_GET_FEATURE_RSP', '2': 37},
    {'1': 'MSG_TYPE_SET_LED_PATTERN_REQ', '2': 38},
    {'1': 'MSG_TYPE_SET_LED_PATTERN_RSP', '2': 39},
    {'1': 'MSG_TYPE_GET_LED_PATTERN_REQ', '2': 40},
    {'1': 'MSG_TYPE_GET_LED_PATTERN_RSP', '2': 41},
    {'1': 'MSG_TYPE_SET_IMU_TRIAGE_REQ', '2': 42},
    {'1': 'MSG_TYPE_SET_IMU_TRIAGE_RSP', '2': 43},
    {'1': 'MSG_TYPE_GET_MODE_REQ', '2': 48},
    {'1': 'MSG_TYPE_GET_MODE_RSP', '2': 49},
    {'1': 'MSG_TYPE_SET_MODE_REQ', '2': 50},
    {'1': 'MSG_TYPE_SET_MODE_RSP', '2': 51},
    {'1': 'MSG_TYPE_GET_SYSTEM_INFO_REQ', '2': 52},
    {'1': 'MSG_TYPE_GET_SYSTEM_INFO_RSP', '2': 53},
  ],
};

/// Descriptor for `MsgType`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List msgTypeDescriptor = $convert.base64Decode(
    'CgdNc2dUeXBlEhQKEE1TR19UWVBFX1VOS05PV04QABIeChpNU0dfVFlQRV9MSVNUX0ZFQVRVUk'
    'VTX1JFURAgEh4KGk1TR19UWVBFX0xJU1RfRkVBVFVSRVNfUlNQECESHAoYTVNHX1RZUEVfU0VU'
    'X0ZFQVRVUkVfUkVRECISHAoYTVNHX1RZUEVfU0VUX0ZFQVRVUkVfUlNQECMSHAoYTVNHX1RZUE'
    'VfR0VUX0ZFQVRVUkVfUkVRECQSHAoYTVNHX1RZUEVfR0VUX0ZFQVRVUkVfUlNQECUSIAocTVNH'
    'X1RZUEVfU0VUX0xFRF9QQVRURVJOX1JFURAmEiAKHE1TR19UWVBFX1NFVF9MRURfUEFUVEVSTl'
    '9SU1AQJxIgChxNU0dfVFlQRV9HRVRfTEVEX1BBVFRFUk5fUkVRECgSIAocTVNHX1RZUEVfR0VU'
    'X0xFRF9QQVRURVJOX1JTUBApEh8KG01TR19UWVBFX1NFVF9JTVVfVFJJQUdFX1JFURAqEh8KG0'
    '1TR19UWVBFX1NFVF9JTVVfVFJJQUdFX1JTUBArEhkKFU1TR19UWVBFX0dFVF9NT0RFX1JFURAw'
    'EhkKFU1TR19UWVBFX0dFVF9NT0RFX1JTUBAxEhkKFU1TR19UWVBFX1NFVF9NT0RFX1JFURAyEh'
    'kKFU1TR19UWVBFX1NFVF9NT0RFX1JTUBAzEiAKHE1TR19UWVBFX0dFVF9TWVNURU1fSU5GT19S'
    'RVEQNBIgChxNU0dfVFlQRV9HRVRfU1lTVEVNX0lORk9fUlNQEDU=');

@$core.Deprecated('Use statusDescriptor instead')
const Status$json = {
  '1': 'Status',
  '2': [
    {'1': 'STATUS_OK', '2': 0},
    {'1': 'STATUS_ERROR', '2': 1},
    {'1': 'STATUS_INVALID_FEATURE', '2': 2},
    {'1': 'STATUS_BUSY', '2': 3},
    {'1': 'STATUS_INVALID_PATTERN', '2': 4},
  ],
};

/// Descriptor for `Status`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List statusDescriptor = $convert.base64Decode(
    'CgZTdGF0dXMSDQoJU1RBVFVTX09LEAASEAoMU1RBVFVTX0VSUk9SEAESGgoWU1RBVFVTX0lOVk'
    'FMSURfRkVBVFVSRRACEg8KC1NUQVRVU19CVVNZEAMSGgoWU1RBVFVTX0lOVkFMSURfUEFUVEVS'
    'ThAE');

@$core.Deprecated('Use ledPatternTypeDescriptor instead')
const LedPatternType$json = {
  '1': 'LedPatternType',
  '2': [
    {'1': 'LED_PATTERN_OFF', '2': 0},
    {'1': 'LED_PATTERN_SOLID', '2': 1},
    {'1': 'LED_PATTERN_BREATHING', '2': 2},
    {'1': 'LED_PATTERN_COLOR_CYCLE', '2': 3},
  ],
};

/// Descriptor for `LedPatternType`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List ledPatternTypeDescriptor = $convert.base64Decode(
    'Cg5MZWRQYXR0ZXJuVHlwZRITCg9MRURfUEFUVEVSTl9PRkYQABIVChFMRURfUEFUVEVSTl9TT0'
    'xJRBABEhkKFUxFRF9QQVRURVJOX0JSRUFUSElORxACEhsKF0xFRF9QQVRURVJOX0NPTE9SX0NZ'
    'Q0xFEAM=');

@$core.Deprecated('Use featureDescriptor instead')
const Feature$json = {
  '1': 'Feature',
  '2': [
    {'1': 'FEATURE_UNKNOWN', '2': 0},
    {'1': 'FEATURE_LED_EFFECTS', '2': 1},
    {'1': 'FEATURE_BLE_ADVERTISING', '2': 2},
    {'1': 'FEATURE_WIFI', '2': 3},
    {'1': 'FEATURE_ESP_NOW', '2': 4},
    {'1': 'FEATURE_TOUCH', '2': 5},
    {'1': 'FEATURE_HAPTIC', '2': 6},
    {'1': 'FEATURE_AUDIO', '2': 7},
  ],
};

/// Descriptor for `Feature`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List featureDescriptor = $convert.base64Decode(
    'CgdGZWF0dXJlEhMKD0ZFQVRVUkVfVU5LTk9XThAAEhcKE0ZFQVRVUkVfTEVEX0VGRkVDVFMQAR'
    'IbChdGRUFUVVJFX0JMRV9BRFZFUlRJU0lORxACEhAKDEZFQVRVUkVfV0lGSRADEhMKD0ZFQVRV'
    'UkVfRVNQX05PVxAEEhEKDUZFQVRVUkVfVE9VQ0gQBRISCg5GRUFUVVJFX0hBUFRJQxAGEhEKDU'
    'ZFQVRVUkVfQVVESU8QBw==');

@$core.Deprecated('Use systemModeDescriptor instead')
const SystemMode$json = {
  '1': 'SystemMode',
  '2': [
    {'1': 'SYSTEM_MODE_BOOTING', '2': 0},
    {'1': 'SYSTEM_MODE_IDLE', '2': 1},
    {'1': 'SYSTEM_MODE_TRIAGE', '2': 2},
    {'1': 'SYSTEM_MODE_CONNECTED', '2': 3},
    {'1': 'SYSTEM_MODE_GAME', '2': 4},
    {'1': 'SYSTEM_MODE_ERROR', '2': 5},
  ],
};

/// Descriptor for `SystemMode`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List systemModeDescriptor = $convert.base64Decode(
    'CgpTeXN0ZW1Nb2RlEhcKE1NZU1RFTV9NT0RFX0JPT1RJTkcQABIUChBTWVNURU1fTU9ERV9JRE'
    'xFEAESFgoSU1lTVEVNX01PREVfVFJJQUdFEAISGQoVU1lTVEVNX01PREVfQ09OTkVDVEVEEAMS'
    'FAoQU1lTVEVNX01PREVfR0FNRRAEEhUKEVNZU1RFTV9NT0RFX0VSUk9SEAU=');

@$core.Deprecated('Use colorDescriptor instead')
const Color$json = {
  '1': 'Color',
  '2': [
    {'1': 'r', '3': 1, '4': 1, '5': 13, '10': 'r'},
    {'1': 'g', '3': 2, '4': 1, '5': 13, '10': 'g'},
    {'1': 'b', '3': 3, '4': 1, '5': 13, '10': 'b'},
    {'1': 'w', '3': 4, '4': 1, '5': 13, '10': 'w'},
  ],
};

/// Descriptor for `Color`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List colorDescriptor = $convert.base64Decode(
    'CgVDb2xvchIMCgFyGAEgASgNUgFyEgwKAWcYAiABKA1SAWcSDAoBYhgDIAEoDVIBYhIMCgF3GA'
    'QgASgNUgF3');

@$core.Deprecated('Use featureStateDescriptor instead')
const FeatureState$json = {
  '1': 'FeatureState',
  '2': [
    {
      '1': 'feature',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.Feature',
      '10': 'feature'
    },
    {'1': 'enabled', '3': 2, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `FeatureState`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List featureStateDescriptor = $convert.base64Decode(
    'CgxGZWF0dXJlU3RhdGUSLwoHZmVhdHVyZRgBIAEoDjIVLmRvbWVzLmNvbmZpZy5GZWF0dXJlUg'
    'dmZWF0dXJlEhgKB2VuYWJsZWQYAiABKAhSB2VuYWJsZWQ=');

@$core.Deprecated('Use listFeaturesRequestDescriptor instead')
const ListFeaturesRequest$json = {
  '1': 'ListFeaturesRequest',
};

/// Descriptor for `ListFeaturesRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listFeaturesRequestDescriptor =
    $convert.base64Decode('ChNMaXN0RmVhdHVyZXNSZXF1ZXN0');

@$core.Deprecated('Use setFeatureRequestDescriptor instead')
const SetFeatureRequest$json = {
  '1': 'SetFeatureRequest',
  '2': [
    {
      '1': 'feature',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.Feature',
      '10': 'feature'
    },
    {'1': 'enabled', '3': 2, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `SetFeatureRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setFeatureRequestDescriptor = $convert.base64Decode(
    'ChFTZXRGZWF0dXJlUmVxdWVzdBIvCgdmZWF0dXJlGAEgASgOMhUuZG9tZXMuY29uZmlnLkZlYX'
    'R1cmVSB2ZlYXR1cmUSGAoHZW5hYmxlZBgCIAEoCFIHZW5hYmxlZA==');

@$core.Deprecated('Use ledPatternDescriptor instead')
const LedPattern$json = {
  '1': 'LedPattern',
  '2': [
    {
      '1': 'type',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.LedPatternType',
      '10': 'type'
    },
    {
      '1': 'color',
      '3': 2,
      '4': 1,
      '5': 11,
      '6': '.domes.config.Color',
      '10': 'color'
    },
    {
      '1': 'colors',
      '3': 3,
      '4': 3,
      '5': 11,
      '6': '.domes.config.Color',
      '10': 'colors'
    },
    {'1': 'period_ms', '3': 4, '4': 1, '5': 13, '10': 'periodMs'},
    {'1': 'brightness', '3': 5, '4': 1, '5': 13, '10': 'brightness'},
  ],
};

/// Descriptor for `LedPattern`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List ledPatternDescriptor = $convert.base64Decode(
    'CgpMZWRQYXR0ZXJuEjAKBHR5cGUYASABKA4yHC5kb21lcy5jb25maWcuTGVkUGF0dGVyblR5cG'
    'VSBHR5cGUSKQoFY29sb3IYAiABKAsyEy5kb21lcy5jb25maWcuQ29sb3JSBWNvbG9yEisKBmNv'
    'bG9ycxgDIAMoCzITLmRvbWVzLmNvbmZpZy5Db2xvclIGY29sb3JzEhsKCXBlcmlvZF9tcxgEIA'
    'EoDVIIcGVyaW9kTXMSHgoKYnJpZ2h0bmVzcxgFIAEoDVIKYnJpZ2h0bmVzcw==');

@$core.Deprecated('Use setLedPatternRequestDescriptor instead')
const SetLedPatternRequest$json = {
  '1': 'SetLedPatternRequest',
  '2': [
    {
      '1': 'pattern',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.config.LedPattern',
      '10': 'pattern'
    },
  ],
};

/// Descriptor for `SetLedPatternRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setLedPatternRequestDescriptor = $convert.base64Decode(
    'ChRTZXRMZWRQYXR0ZXJuUmVxdWVzdBIyCgdwYXR0ZXJuGAEgASgLMhguZG9tZXMuY29uZmlnLk'
    'xlZFBhdHRlcm5SB3BhdHRlcm4=');

@$core.Deprecated('Use getLedPatternRequestDescriptor instead')
const GetLedPatternRequest$json = {
  '1': 'GetLedPatternRequest',
};

/// Descriptor for `GetLedPatternRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getLedPatternRequestDescriptor =
    $convert.base64Decode('ChRHZXRMZWRQYXR0ZXJuUmVxdWVzdA==');

@$core.Deprecated('Use listFeaturesResponseDescriptor instead')
const ListFeaturesResponse$json = {
  '1': 'ListFeaturesResponse',
  '2': [
    {
      '1': 'features',
      '3': 1,
      '4': 3,
      '5': 11,
      '6': '.domes.config.FeatureState',
      '10': 'features'
    },
  ],
};

/// Descriptor for `ListFeaturesResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listFeaturesResponseDescriptor = $convert.base64Decode(
    'ChRMaXN0RmVhdHVyZXNSZXNwb25zZRI2CghmZWF0dXJlcxgBIAMoCzIaLmRvbWVzLmNvbmZpZy'
    '5GZWF0dXJlU3RhdGVSCGZlYXR1cmVz');

@$core.Deprecated('Use setFeatureResponseDescriptor instead')
const SetFeatureResponse$json = {
  '1': 'SetFeatureResponse',
  '2': [
    {
      '1': 'feature',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.config.FeatureState',
      '10': 'feature'
    },
  ],
};

/// Descriptor for `SetFeatureResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setFeatureResponseDescriptor = $convert.base64Decode(
    'ChJTZXRGZWF0dXJlUmVzcG9uc2USNAoHZmVhdHVyZRgBIAEoCzIaLmRvbWVzLmNvbmZpZy5GZW'
    'F0dXJlU3RhdGVSB2ZlYXR1cmU=');

@$core.Deprecated('Use setLedPatternResponseDescriptor instead')
const SetLedPatternResponse$json = {
  '1': 'SetLedPatternResponse',
  '2': [
    {
      '1': 'pattern',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.config.LedPattern',
      '10': 'pattern'
    },
  ],
};

/// Descriptor for `SetLedPatternResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setLedPatternResponseDescriptor = $convert.base64Decode(
    'ChVTZXRMZWRQYXR0ZXJuUmVzcG9uc2USMgoHcGF0dGVybhgBIAEoCzIYLmRvbWVzLmNvbmZpZy'
    '5MZWRQYXR0ZXJuUgdwYXR0ZXJu');

@$core.Deprecated('Use getLedPatternResponseDescriptor instead')
const GetLedPatternResponse$json = {
  '1': 'GetLedPatternResponse',
  '2': [
    {
      '1': 'pattern',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.config.LedPattern',
      '10': 'pattern'
    },
  ],
};

/// Descriptor for `GetLedPatternResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getLedPatternResponseDescriptor = $convert.base64Decode(
    'ChVHZXRMZWRQYXR0ZXJuUmVzcG9uc2USMgoHcGF0dGVybhgBIAEoCzIYLmRvbWVzLmNvbmZpZy'
    '5MZWRQYXR0ZXJuUgdwYXR0ZXJu');

@$core.Deprecated('Use setImuTriageRequestDescriptor instead')
const SetImuTriageRequest$json = {
  '1': 'SetImuTriageRequest',
  '2': [
    {'1': 'enabled', '3': 1, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `SetImuTriageRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setImuTriageRequestDescriptor =
    $convert.base64Decode(
        'ChNTZXRJbXVUcmlhZ2VSZXF1ZXN0EhgKB2VuYWJsZWQYASABKAhSB2VuYWJsZWQ=');

@$core.Deprecated('Use setImuTriageResponseDescriptor instead')
const SetImuTriageResponse$json = {
  '1': 'SetImuTriageResponse',
  '2': [
    {'1': 'enabled', '3': 1, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `SetImuTriageResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setImuTriageResponseDescriptor =
    $convert.base64Decode(
        'ChRTZXRJbXVUcmlhZ2VSZXNwb25zZRIYCgdlbmFibGVkGAEgASgIUgdlbmFibGVk');

@$core.Deprecated('Use getModeRequestDescriptor instead')
const GetModeRequest$json = {
  '1': 'GetModeRequest',
};

/// Descriptor for `GetModeRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getModeRequestDescriptor =
    $convert.base64Decode('Cg5HZXRNb2RlUmVxdWVzdA==');

@$core.Deprecated('Use getModeResponseDescriptor instead')
const GetModeResponse$json = {
  '1': 'GetModeResponse',
  '2': [
    {
      '1': 'mode',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.SystemMode',
      '10': 'mode'
    },
    {'1': 'time_in_mode_ms', '3': 2, '4': 1, '5': 13, '10': 'timeInModeMs'},
  ],
};

/// Descriptor for `GetModeResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getModeResponseDescriptor = $convert.base64Decode(
    'Cg9HZXRNb2RlUmVzcG9uc2USLAoEbW9kZRgBIAEoDjIYLmRvbWVzLmNvbmZpZy5TeXN0ZW1Nb2'
    'RlUgRtb2RlEiUKD3RpbWVfaW5fbW9kZV9tcxgCIAEoDVIMdGltZUluTW9kZU1z');

@$core.Deprecated('Use setModeRequestDescriptor instead')
const SetModeRequest$json = {
  '1': 'SetModeRequest',
  '2': [
    {
      '1': 'mode',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.SystemMode',
      '10': 'mode'
    },
  ],
};

/// Descriptor for `SetModeRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setModeRequestDescriptor = $convert.base64Decode(
    'Cg5TZXRNb2RlUmVxdWVzdBIsCgRtb2RlGAEgASgOMhguZG9tZXMuY29uZmlnLlN5c3RlbU1vZG'
    'VSBG1vZGU=');

@$core.Deprecated('Use setModeResponseDescriptor instead')
const SetModeResponse$json = {
  '1': 'SetModeResponse',
  '2': [
    {
      '1': 'mode',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.SystemMode',
      '10': 'mode'
    },
    {'1': 'transition_ok', '3': 2, '4': 1, '5': 8, '10': 'transitionOk'},
  ],
};

/// Descriptor for `SetModeResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setModeResponseDescriptor = $convert.base64Decode(
    'Cg9TZXRNb2RlUmVzcG9uc2USLAoEbW9kZRgBIAEoDjIYLmRvbWVzLmNvbmZpZy5TeXN0ZW1Nb2'
    'RlUgRtb2RlEiMKDXRyYW5zaXRpb25fb2sYAiABKAhSDHRyYW5zaXRpb25Paw==');

@$core.Deprecated('Use getSystemInfoRequestDescriptor instead')
const GetSystemInfoRequest$json = {
  '1': 'GetSystemInfoRequest',
};

/// Descriptor for `GetSystemInfoRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getSystemInfoRequestDescriptor =
    $convert.base64Decode('ChRHZXRTeXN0ZW1JbmZvUmVxdWVzdA==');

@$core.Deprecated('Use getSystemInfoResponseDescriptor instead')
const GetSystemInfoResponse$json = {
  '1': 'GetSystemInfoResponse',
  '2': [
    {'1': 'firmware_version', '3': 1, '4': 1, '5': 9, '10': 'firmwareVersion'},
    {'1': 'uptime_s', '3': 2, '4': 1, '5': 13, '10': 'uptimeS'},
    {'1': 'free_heap', '3': 3, '4': 1, '5': 13, '10': 'freeHeap'},
    {'1': 'boot_count', '3': 4, '4': 1, '5': 13, '10': 'bootCount'},
    {
      '1': 'mode',
      '3': 5,
      '4': 1,
      '5': 14,
      '6': '.domes.config.SystemMode',
      '10': 'mode'
    },
    {'1': 'feature_mask', '3': 6, '4': 1, '5': 13, '10': 'featureMask'},
  ],
};

/// Descriptor for `GetSystemInfoResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getSystemInfoResponseDescriptor = $convert.base64Decode(
    'ChVHZXRTeXN0ZW1JbmZvUmVzcG9uc2USKQoQZmlybXdhcmVfdmVyc2lvbhgBIAEoCVIPZmlybX'
    'dhcmVWZXJzaW9uEhkKCHVwdGltZV9zGAIgASgNUgd1cHRpbWVTEhsKCWZyZWVfaGVhcBgDIAEo'
    'DVIIZnJlZUhlYXASHQoKYm9vdF9jb3VudBgEIAEoDVIJYm9vdENvdW50EiwKBG1vZGUYBSABKA'
    '4yGC5kb21lcy5jb25maWcuU3lzdGVtTW9kZVIEbW9kZRIhCgxmZWF0dXJlX21hc2sYBiABKA1S'
    'C2ZlYXR1cmVNYXNr');

@$core.Deprecated('Use configRequestDescriptor instead')
const ConfigRequest$json = {
  '1': 'ConfigRequest',
  '2': [
    {
      '1': 'list_features',
      '3': 1,
      '4': 1,
      '5': 11,
      '6': '.domes.config.ListFeaturesRequest',
      '9': 0,
      '10': 'listFeatures'
    },
    {
      '1': 'set_feature',
      '3': 2,
      '4': 1,
      '5': 11,
      '6': '.domes.config.SetFeatureRequest',
      '9': 0,
      '10': 'setFeature'
    },
  ],
  '8': [
    {'1': 'request'},
  ],
};

/// Descriptor for `ConfigRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List configRequestDescriptor = $convert.base64Decode(
    'Cg1Db25maWdSZXF1ZXN0EkgKDWxpc3RfZmVhdHVyZXMYASABKAsyIS5kb21lcy5jb25maWcuTG'
    'lzdEZlYXR1cmVzUmVxdWVzdEgAUgxsaXN0RmVhdHVyZXMSQgoLc2V0X2ZlYXR1cmUYAiABKAsy'
    'Hy5kb21lcy5jb25maWcuU2V0RmVhdHVyZVJlcXVlc3RIAFIKc2V0RmVhdHVyZUIJCgdyZXF1ZX'
    'N0');

@$core.Deprecated('Use configResponseDescriptor instead')
const ConfigResponse$json = {
  '1': 'ConfigResponse',
  '2': [
    {
      '1': 'status',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.Status',
      '10': 'status'
    },
    {
      '1': 'list_features',
      '3': 2,
      '4': 1,
      '5': 11,
      '6': '.domes.config.ListFeaturesResponse',
      '9': 0,
      '10': 'listFeatures'
    },
    {
      '1': 'set_feature',
      '3': 3,
      '4': 1,
      '5': 11,
      '6': '.domes.config.SetFeatureResponse',
      '9': 0,
      '10': 'setFeature'
    },
  ],
  '8': [
    {'1': 'response'},
  ],
};

/// Descriptor for `ConfigResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List configResponseDescriptor = $convert.base64Decode(
    'Cg5Db25maWdSZXNwb25zZRIsCgZzdGF0dXMYASABKA4yFC5kb21lcy5jb25maWcuU3RhdHVzUg'
    'ZzdGF0dXMSSQoNbGlzdF9mZWF0dXJlcxgCIAEoCzIiLmRvbWVzLmNvbmZpZy5MaXN0RmVhdHVy'
    'ZXNSZXNwb25zZUgAUgxsaXN0RmVhdHVyZXMSQwoLc2V0X2ZlYXR1cmUYAyABKAsyIC5kb21lcy'
    '5jb25maWcuU2V0RmVhdHVyZVJlc3BvbnNlSABSCnNldEZlYXR1cmVCCgoIcmVzcG9uc2U=');
