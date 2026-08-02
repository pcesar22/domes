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
    {'1': 'MSG_TYPE_SET_POD_ID_REQ', '2': 54},
    {'1': 'MSG_TYPE_SET_POD_ID_RSP', '2': 55},
    {'1': 'MSG_TYPE_GET_HEALTH_REQ', '2': 56},
    {'1': 'MSG_TYPE_GET_HEALTH_RSP', '2': 57},
    {'1': 'MSG_TYPE_GET_ESPNOW_STATUS_REQ', '2': 58},
    {'1': 'MSG_TYPE_GET_ESPNOW_STATUS_RSP', '2': 59},
    {'1': 'MSG_TYPE_ESPNOW_BENCH_REQ', '2': 60},
    {'1': 'MSG_TYPE_ESPNOW_BENCH_RSP', '2': 61},
    {'1': 'MSG_TYPE_GET_CRASH_DUMP_REQ', '2': 62},
    {'1': 'MSG_TYPE_GET_CRASH_DUMP_RSP', '2': 63},
    {'1': 'MSG_TYPE_CLEAR_CRASH_DUMP_REQ', '2': 64},
    {'1': 'MSG_TYPE_CLEAR_CRASH_DUMP_RSP', '2': 65},
    {'1': 'MSG_TYPE_GET_MEMORY_PROFILE_REQ', '2': 66},
    {'1': 'MSG_TYPE_GET_MEMORY_PROFILE_RSP', '2': 67},
    {'1': 'MSG_TYPE_SELF_TEST_REQ', '2': 68},
    {'1': 'MSG_TYPE_SELF_TEST_RSP', '2': 69},
    {'1': 'MSG_TYPE_CHECK_UPDATE_REQ', '2': 70},
    {'1': 'MSG_TYPE_CHECK_UPDATE_RSP', '2': 71},
    {'1': 'MSG_TYPE_SET_AUTO_UPDATE_REQ', '2': 72},
    {'1': 'MSG_TYPE_SET_AUTO_UPDATE_RSP', '2': 73},
    {'1': 'MSG_TYPE_SIMULATE_TOUCH_REQ', '2': 76},
    {'1': 'MSG_TYPE_SIMULATE_TOUCH_RSP', '2': 77},
    {'1': 'MSG_TYPE_SET_SIM_MODE_REQ', '2': 78},
    {'1': 'MSG_TYPE_SET_SIM_MODE_RSP', '2': 79},
    {'1': 'MSG_TYPE_TOUCH_EVENT_NTF', '2': 80},
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
    'RVEQNBIgChxNU0dfVFlQRV9HRVRfU1lTVEVNX0lORk9fUlNQEDUSGwoXTVNHX1RZUEVfU0VUX1'
    'BPRF9JRF9SRVEQNhIbChdNU0dfVFlQRV9TRVRfUE9EX0lEX1JTUBA3EhsKF01TR19UWVBFX0dF'
    'VF9IRUFMVEhfUkVREDgSGwoXTVNHX1RZUEVfR0VUX0hFQUxUSF9SU1AQORIiCh5NU0dfVFlQRV'
    '9HRVRfRVNQTk9XX1NUQVRVU19SRVEQOhIiCh5NU0dfVFlQRV9HRVRfRVNQTk9XX1NUQVRVU19S'
    'U1AQOxIdChlNU0dfVFlQRV9FU1BOT1dfQkVOQ0hfUkVREDwSHQoZTVNHX1RZUEVfRVNQTk9XX0'
    'JFTkNIX1JTUBA9Eh8KG01TR19UWVBFX0dFVF9DUkFTSF9EVU1QX1JFURA+Eh8KG01TR19UWVBF'
    'X0dFVF9DUkFTSF9EVU1QX1JTUBA/EiEKHU1TR19UWVBFX0NMRUFSX0NSQVNIX0RVTVBfUkVREE'
    'ASIQodTVNHX1RZUEVfQ0xFQVJfQ1JBU0hfRFVNUF9SU1AQQRIjCh9NU0dfVFlQRV9HRVRfTUVN'
    'T1JZX1BST0ZJTEVfUkVREEISIwofTVNHX1RZUEVfR0VUX01FTU9SWV9QUk9GSUxFX1JTUBBDEh'
    'oKFk1TR19UWVBFX1NFTEZfVEVTVF9SRVEQRBIaChZNU0dfVFlQRV9TRUxGX1RFU1RfUlNQEEUS'
    'HQoZTVNHX1RZUEVfQ0hFQ0tfVVBEQVRFX1JFURBGEh0KGU1TR19UWVBFX0NIRUNLX1VQREFURV'
    '9SU1AQRxIgChxNU0dfVFlQRV9TRVRfQVVUT19VUERBVEVfUkVREEgSIAocTVNHX1RZUEVfU0VU'
    'X0FVVE9fVVBEQVRFX1JTUBBJEh8KG01TR19UWVBFX1NJTVVMQVRFX1RPVUNIX1JFURBMEh8KG0'
    '1TR19UWVBFX1NJTVVMQVRFX1RPVUNIX1JTUBBNEh0KGU1TR19UWVBFX1NFVF9TSU1fTU9ERV9S'
    'RVEQThIdChlNU0dfVFlQRV9TRVRfU0lNX01PREVfUlNQEE8SHAoYTVNHX1RZUEVfVE9VQ0hfRV'
    'ZFTlRfTlRGEFA=');

@$core.Deprecated('Use statusDescriptor instead')
const Status$json = {
  '1': 'Status',
  '2': [
    {'1': 'STATUS_OK', '2': 0},
    {'1': 'STATUS_ERROR', '2': 1},
    {'1': 'STATUS_INVALID_FEATURE', '2': 2},
    {'1': 'STATUS_BUSY', '2': 3},
    {'1': 'STATUS_INVALID_PATTERN', '2': 4},
    {'1': 'STATUS_NO_DATA', '2': 5},
  ],
};

/// Descriptor for `Status`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List statusDescriptor = $convert.base64Decode(
    'CgZTdGF0dXMSDQoJU1RBVFVTX09LEAASEAoMU1RBVFVTX0VSUk9SEAESGgoWU1RBVFVTX0lOVk'
    'FMSURfRkVBVFVSRRACEg8KC1NUQVRVU19CVVNZEAMSGgoWU1RBVFVTX0lOVkFMSURfUEFUVEVS'
    'ThAEEhIKDlNUQVRVU19OT19EQVRBEAU=');

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

@$core.Deprecated('Use resetReasonDescriptor instead')
const ResetReason$json = {
  '1': 'ResetReason',
  '2': [
    {'1': 'RESET_REASON_UNKNOWN', '2': 0},
    {'1': 'RESET_REASON_POWER_ON', '2': 1},
    {'1': 'RESET_REASON_EXTERNAL_PIN', '2': 2},
    {'1': 'RESET_REASON_SOFTWARE', '2': 3},
    {'1': 'RESET_REASON_PANIC', '2': 4},
    {'1': 'RESET_REASON_INTERRUPT_WATCHDOG', '2': 5},
    {'1': 'RESET_REASON_TASK_WATCHDOG', '2': 6},
    {'1': 'RESET_REASON_WATCHDOG', '2': 7},
    {'1': 'RESET_REASON_DEEP_SLEEP', '2': 8},
    {'1': 'RESET_REASON_BROWNOUT', '2': 9},
    {'1': 'RESET_REASON_SDIO', '2': 10},
    {'1': 'RESET_REASON_USB', '2': 11},
    {'1': 'RESET_REASON_JTAG', '2': 12},
    {'1': 'RESET_REASON_EFUSE', '2': 13},
    {'1': 'RESET_REASON_POWER_GLITCH', '2': 14},
    {'1': 'RESET_REASON_CPU_LOCKUP', '2': 15},
  ],
};

/// Descriptor for `ResetReason`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List resetReasonDescriptor = $convert.base64Decode(
    'CgtSZXNldFJlYXNvbhIYChRSRVNFVF9SRUFTT05fVU5LTk9XThAAEhkKFVJFU0VUX1JFQVNPTl'
    '9QT1dFUl9PThABEh0KGVJFU0VUX1JFQVNPTl9FWFRFUk5BTF9QSU4QAhIZChVSRVNFVF9SRUFT'
    'T05fU09GVFdBUkUQAxIWChJSRVNFVF9SRUFTT05fUEFOSUMQBBIjCh9SRVNFVF9SRUFTT05fSU'
    '5URVJSVVBUX1dBVENIRE9HEAUSHgoaUkVTRVRfUkVBU09OX1RBU0tfV0FUQ0hET0cQBhIZChVS'
    'RVNFVF9SRUFTT05fV0FUQ0hET0cQBxIbChdSRVNFVF9SRUFTT05fREVFUF9TTEVFUBAIEhkKFV'
    'JFU0VUX1JFQVNPTl9CUk9XTk9VVBAJEhUKEVJFU0VUX1JFQVNPTl9TRElPEAoSFAoQUkVTRVRf'
    'UkVBU09OX1VTQhALEhUKEVJFU0VUX1JFQVNPTl9KVEFHEAwSFgoSUkVTRVRfUkVBU09OX0VGVV'
    'NFEA0SHQoZUkVTRVRfUkVBU09OX1BPV0VSX0dMSVRDSBAOEhsKF1JFU0VUX1JFQVNPTl9DUFVf'
    'TE9DS1VQEA8=');

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

@$core.Deprecated('Use getFeatureRequestDescriptor instead')
const GetFeatureRequest$json = {
  '1': 'GetFeatureRequest',
  '2': [
    {
      '1': 'feature',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.domes.config.Feature',
      '10': 'feature'
    },
  ],
};

/// Descriptor for `GetFeatureRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getFeatureRequestDescriptor = $convert.base64Decode(
    'ChFHZXRGZWF0dXJlUmVxdWVzdBIvCgdmZWF0dXJlGAEgASgOMhUuZG9tZXMuY29uZmlnLkZlYX'
    'R1cmVSB2ZlYXR1cmU=');

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
    {'1': 'pod_id', '3': 2, '4': 1, '5': 13, '10': 'podId'},
  ],
};

/// Descriptor for `ListFeaturesResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listFeaturesResponseDescriptor = $convert.base64Decode(
    'ChRMaXN0RmVhdHVyZXNSZXNwb25zZRI2CghmZWF0dXJlcxgBIAMoCzIaLmRvbWVzLmNvbmZpZy'
    '5GZWF0dXJlU3RhdGVSCGZlYXR1cmVzEhUKBnBvZF9pZBgCIAEoDVIFcG9kSWQ=');

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

@$core.Deprecated('Use getFeatureResponseDescriptor instead')
const GetFeatureResponse$json = {
  '1': 'GetFeatureResponse',
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

/// Descriptor for `GetFeatureResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getFeatureResponseDescriptor = $convert.base64Decode(
    'ChJHZXRGZWF0dXJlUmVzcG9uc2USNAoHZmVhdHVyZRgBIAEoCzIaLmRvbWVzLmNvbmZpZy5GZW'
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
    {'1': 'pod_id', '3': 7, '4': 1, '5': 13, '10': 'podId'},
    {
      '1': 'reset_reason',
      '3': 8,
      '4': 1,
      '5': 14,
      '6': '.domes.config.ResetReason',
      '10': 'resetReason'
    },
  ],
};

/// Descriptor for `GetSystemInfoResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getSystemInfoResponseDescriptor = $convert.base64Decode(
    'ChVHZXRTeXN0ZW1JbmZvUmVzcG9uc2USKQoQZmlybXdhcmVfdmVyc2lvbhgBIAEoCVIPZmlybX'
    'dhcmVWZXJzaW9uEhkKCHVwdGltZV9zGAIgASgNUgd1cHRpbWVTEhsKCWZyZWVfaGVhcBgDIAEo'
    'DVIIZnJlZUhlYXASHQoKYm9vdF9jb3VudBgEIAEoDVIJYm9vdENvdW50EiwKBG1vZGUYBSABKA'
    '4yGC5kb21lcy5jb25maWcuU3lzdGVtTW9kZVIEbW9kZRIhCgxmZWF0dXJlX21hc2sYBiABKA1S'
    'C2ZlYXR1cmVNYXNrEhUKBnBvZF9pZBgHIAEoDVIFcG9kSWQSPAoMcmVzZXRfcmVhc29uGAggAS'
    'gOMhkuZG9tZXMuY29uZmlnLlJlc2V0UmVhc29uUgtyZXNldFJlYXNvbg==');

@$core.Deprecated('Use setPodIdRequestDescriptor instead')
const SetPodIdRequest$json = {
  '1': 'SetPodIdRequest',
  '2': [
    {'1': 'pod_id', '3': 1, '4': 1, '5': 13, '10': 'podId'},
  ],
};

/// Descriptor for `SetPodIdRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setPodIdRequestDescriptor = $convert
    .base64Decode('Cg9TZXRQb2RJZFJlcXVlc3QSFQoGcG9kX2lkGAEgASgNUgVwb2RJZA==');

@$core.Deprecated('Use setPodIdResponseDescriptor instead')
const SetPodIdResponse$json = {
  '1': 'SetPodIdResponse',
  '2': [
    {'1': 'pod_id', '3': 1, '4': 1, '5': 13, '10': 'podId'},
  ],
};

/// Descriptor for `SetPodIdResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setPodIdResponseDescriptor = $convert
    .base64Decode('ChBTZXRQb2RJZFJlc3BvbnNlEhUKBnBvZF9pZBgBIAEoDVIFcG9kSWQ=');

@$core.Deprecated('Use taskHealthDescriptor instead')
const TaskHealth$json = {
  '1': 'TaskHealth',
  '2': [
    {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    {'1': 'stack_high_water', '3': 2, '4': 1, '5': 13, '10': 'stackHighWater'},
    {'1': 'priority', '3': 3, '4': 1, '5': 13, '10': 'priority'},
    {'1': 'core', '3': 4, '4': 1, '5': 13, '10': 'core'},
  ],
};

/// Descriptor for `TaskHealth`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List taskHealthDescriptor = $convert.base64Decode(
    'CgpUYXNrSGVhbHRoEhIKBG5hbWUYASABKAlSBG5hbWUSKAoQc3RhY2tfaGlnaF93YXRlchgCIA'
    'EoDVIOc3RhY2tIaWdoV2F0ZXISGgoIcHJpb3JpdHkYAyABKA1SCHByaW9yaXR5EhIKBGNvcmUY'
    'BCABKA1SBGNvcmU=');

@$core.Deprecated('Use getHealthRequestDescriptor instead')
const GetHealthRequest$json = {
  '1': 'GetHealthRequest',
};

/// Descriptor for `GetHealthRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getHealthRequestDescriptor =
    $convert.base64Decode('ChBHZXRIZWFsdGhSZXF1ZXN0');

@$core.Deprecated('Use getHealthResponseDescriptor instead')
const GetHealthResponse$json = {
  '1': 'GetHealthResponse',
  '2': [
    {'1': 'free_heap', '3': 1, '4': 1, '5': 13, '10': 'freeHeap'},
    {'1': 'min_free_heap', '3': 2, '4': 1, '5': 13, '10': 'minFreeHeap'},
    {'1': 'uptime_seconds', '3': 3, '4': 1, '5': 13, '10': 'uptimeSeconds'},
    {'1': 'wifi_rssi', '3': 4, '4': 1, '5': 5, '10': 'wifiRssi'},
    {
      '1': 'tasks',
      '3': 5,
      '4': 3,
      '5': 11,
      '6': '.domes.config.TaskHealth',
      '10': 'tasks'
    },
  ],
};

/// Descriptor for `GetHealthResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getHealthResponseDescriptor = $convert.base64Decode(
    'ChFHZXRIZWFsdGhSZXNwb25zZRIbCglmcmVlX2hlYXAYASABKA1SCGZyZWVIZWFwEiIKDW1pbl'
    '9mcmVlX2hlYXAYAiABKA1SC21pbkZyZWVIZWFwEiUKDnVwdGltZV9zZWNvbmRzGAMgASgNUg11'
    'cHRpbWVTZWNvbmRzEhsKCXdpZmlfcnNzaRgEIAEoBVIId2lmaVJzc2kSLgoFdGFza3MYBSADKA'
    'syGC5kb21lcy5jb25maWcuVGFza0hlYWx0aFIFdGFza3M=');

@$core.Deprecated('Use espNowPeerDescriptor instead')
const EspNowPeer$json = {
  '1': 'EspNowPeer',
  '2': [
    {'1': 'mac', '3': 1, '4': 1, '5': 12, '10': 'mac'},
    {'1': 'rssi', '3': 2, '4': 1, '5': 5, '10': 'rssi'},
    {'1': 'last_seen_ms', '3': 3, '4': 1, '5': 13, '10': 'lastSeenMs'},
  ],
};

/// Descriptor for `EspNowPeer`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List espNowPeerDescriptor = $convert.base64Decode(
    'CgpFc3BOb3dQZWVyEhAKA21hYxgBIAEoDFIDbWFjEhIKBHJzc2kYAiABKAVSBHJzc2kSIAoMbG'
    'FzdF9zZWVuX21zGAMgASgNUgpsYXN0U2Vlbk1z');

@$core.Deprecated('Use getEspNowStatusRequestDescriptor instead')
const GetEspNowStatusRequest$json = {
  '1': 'GetEspNowStatusRequest',
};

/// Descriptor for `GetEspNowStatusRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getEspNowStatusRequestDescriptor =
    $convert.base64Decode('ChZHZXRFc3BOb3dTdGF0dXNSZXF1ZXN0');

@$core.Deprecated('Use getEspNowStatusResponseDescriptor instead')
const GetEspNowStatusResponse$json = {
  '1': 'GetEspNowStatusResponse',
  '2': [
    {'1': 'peer_count', '3': 1, '4': 1, '5': 13, '10': 'peerCount'},
    {'1': 'channel', '3': 2, '4': 1, '5': 13, '10': 'channel'},
    {'1': 'tx_count', '3': 3, '4': 1, '5': 13, '10': 'txCount'},
    {'1': 'rx_count', '3': 4, '4': 1, '5': 13, '10': 'rxCount'},
    {'1': 'tx_fail_count', '3': 5, '4': 1, '5': 13, '10': 'txFailCount'},
    {'1': 'last_rtt_us', '3': 6, '4': 1, '5': 13, '10': 'lastRttUs'},
    {'1': 'discovery_state', '3': 7, '4': 1, '5': 9, '10': 'discoveryState'},
    {
      '1': 'peers',
      '3': 8,
      '4': 3,
      '5': 11,
      '6': '.domes.config.EspNowPeer',
      '10': 'peers'
    },
  ],
};

/// Descriptor for `GetEspNowStatusResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getEspNowStatusResponseDescriptor = $convert.base64Decode(
    'ChdHZXRFc3BOb3dTdGF0dXNSZXNwb25zZRIdCgpwZWVyX2NvdW50GAEgASgNUglwZWVyQ291bn'
    'QSGAoHY2hhbm5lbBgCIAEoDVIHY2hhbm5lbBIZCgh0eF9jb3VudBgDIAEoDVIHdHhDb3VudBIZ'
    'CghyeF9jb3VudBgEIAEoDVIHcnhDb3VudBIiCg10eF9mYWlsX2NvdW50GAUgASgNUgt0eEZhaW'
    'xDb3VudBIeCgtsYXN0X3J0dF91cxgGIAEoDVIJbGFzdFJ0dFVzEicKD2Rpc2NvdmVyeV9zdGF0'
    'ZRgHIAEoCVIOZGlzY292ZXJ5U3RhdGUSLgoFcGVlcnMYCCADKAsyGC5kb21lcy5jb25maWcuRX'
    'NwTm93UGVlclIFcGVlcnM=');

@$core.Deprecated('Use espNowBenchRequestDescriptor instead')
const EspNowBenchRequest$json = {
  '1': 'EspNowBenchRequest',
  '2': [
    {'1': 'rounds', '3': 1, '4': 1, '5': 13, '10': 'rounds'},
  ],
};

/// Descriptor for `EspNowBenchRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List espNowBenchRequestDescriptor =
    $convert.base64Decode(
        'ChJFc3BOb3dCZW5jaFJlcXVlc3QSFgoGcm91bmRzGAEgASgNUgZyb3VuZHM=');

@$core.Deprecated('Use espNowBenchResponseDescriptor instead')
const EspNowBenchResponse$json = {
  '1': 'EspNowBenchResponse',
  '2': [
    {'1': 'rounds_completed', '3': 1, '4': 1, '5': 13, '10': 'roundsCompleted'},
    {'1': 'rounds_failed', '3': 2, '4': 1, '5': 13, '10': 'roundsFailed'},
    {'1': 'min_rtt_us', '3': 3, '4': 1, '5': 13, '10': 'minRttUs'},
    {'1': 'max_rtt_us', '3': 4, '4': 1, '5': 13, '10': 'maxRttUs'},
    {'1': 'mean_rtt_us', '3': 5, '4': 1, '5': 13, '10': 'meanRttUs'},
    {'1': 'p50_rtt_us', '3': 6, '4': 1, '5': 13, '10': 'p50RttUs'},
    {'1': 'p95_rtt_us', '3': 7, '4': 1, '5': 13, '10': 'p95RttUs'},
    {'1': 'p99_rtt_us', '3': 8, '4': 1, '5': 13, '10': 'p99RttUs'},
  ],
};

/// Descriptor for `EspNowBenchResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List espNowBenchResponseDescriptor = $convert.base64Decode(
    'ChNFc3BOb3dCZW5jaFJlc3BvbnNlEikKEHJvdW5kc19jb21wbGV0ZWQYASABKA1SD3JvdW5kc0'
    'NvbXBsZXRlZBIjCg1yb3VuZHNfZmFpbGVkGAIgASgNUgxyb3VuZHNGYWlsZWQSHAoKbWluX3J0'
    'dF91cxgDIAEoDVIIbWluUnR0VXMSHAoKbWF4X3J0dF91cxgEIAEoDVIIbWF4UnR0VXMSHgoLbW'
    'Vhbl9ydHRfdXMYBSABKA1SCW1lYW5SdHRVcxIcCgpwNTBfcnR0X3VzGAYgASgNUghwNTBSdHRV'
    'cxIcCgpwOTVfcnR0X3VzGAcgASgNUghwOTVSdHRVcxIcCgpwOTlfcnR0X3VzGAggASgNUghwOT'
    'lSdHRVcw==');

@$core.Deprecated('Use getCrashDumpRequestDescriptor instead')
const GetCrashDumpRequest$json = {
  '1': 'GetCrashDumpRequest',
};

/// Descriptor for `GetCrashDumpRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getCrashDumpRequestDescriptor =
    $convert.base64Decode('ChNHZXRDcmFzaER1bXBSZXF1ZXN0');

@$core.Deprecated('Use crashDumpResponseDescriptor instead')
const CrashDumpResponse$json = {
  '1': 'CrashDumpResponse',
  '2': [
    {'1': 'has_dump', '3': 1, '4': 1, '5': 8, '10': 'hasDump'},
    {'1': 'reason', '3': 2, '4': 1, '5': 9, '10': 'reason'},
    {'1': 'task_name', '3': 3, '4': 1, '5': 9, '10': 'taskName'},
    {'1': 'uptime_s', '3': 4, '4': 1, '5': 13, '10': 'uptimeS'},
    {'1': 'free_heap', '3': 5, '4': 1, '5': 13, '10': 'freeHeap'},
    {'1': 'backtrace', '3': 6, '4': 3, '5': 13, '10': 'backtrace'},
    {'1': 'timestamp', '3': 7, '4': 1, '5': 13, '10': 'timestamp'},
  ],
};

/// Descriptor for `CrashDumpResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List crashDumpResponseDescriptor = $convert.base64Decode(
    'ChFDcmFzaER1bXBSZXNwb25zZRIZCghoYXNfZHVtcBgBIAEoCFIHaGFzRHVtcBIWCgZyZWFzb2'
    '4YAiABKAlSBnJlYXNvbhIbCgl0YXNrX25hbWUYAyABKAlSCHRhc2tOYW1lEhkKCHVwdGltZV9z'
    'GAQgASgNUgd1cHRpbWVTEhsKCWZyZWVfaGVhcBgFIAEoDVIIZnJlZUhlYXASHAoJYmFja3RyYW'
    'NlGAYgAygNUgliYWNrdHJhY2USHAoJdGltZXN0YW1wGAcgASgNUgl0aW1lc3RhbXA=');

@$core.Deprecated('Use clearCrashDumpRequestDescriptor instead')
const ClearCrashDumpRequest$json = {
  '1': 'ClearCrashDumpRequest',
};

/// Descriptor for `ClearCrashDumpRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List clearCrashDumpRequestDescriptor =
    $convert.base64Decode('ChVDbGVhckNyYXNoRHVtcFJlcXVlc3Q=');

@$core.Deprecated('Use clearCrashDumpResponseDescriptor instead')
const ClearCrashDumpResponse$json = {
  '1': 'ClearCrashDumpResponse',
  '2': [
    {'1': 'cleared', '3': 1, '4': 1, '5': 8, '10': 'cleared'},
  ],
};

/// Descriptor for `ClearCrashDumpResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List clearCrashDumpResponseDescriptor =
    $convert.base64Decode(
        'ChZDbGVhckNyYXNoRHVtcFJlc3BvbnNlEhgKB2NsZWFyZWQYASABKAhSB2NsZWFyZWQ=');

@$core.Deprecated('Use heapSampleDescriptor instead')
const HeapSample$json = {
  '1': 'HeapSample',
  '2': [
    {'1': 'timestamp_s', '3': 1, '4': 1, '5': 13, '10': 'timestampS'},
    {'1': 'free_heap', '3': 2, '4': 1, '5': 13, '10': 'freeHeap'},
    {'1': 'largest_block', '3': 3, '4': 1, '5': 13, '10': 'largestBlock'},
    {'1': 'min_free_heap', '3': 4, '4': 1, '5': 13, '10': 'minFreeHeap'},
  ],
};

/// Descriptor for `HeapSample`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List heapSampleDescriptor = $convert.base64Decode(
    'CgpIZWFwU2FtcGxlEh8KC3RpbWVzdGFtcF9zGAEgASgNUgp0aW1lc3RhbXBTEhsKCWZyZWVfaG'
    'VhcBgCIAEoDVIIZnJlZUhlYXASIwoNbGFyZ2VzdF9ibG9jaxgDIAEoDVIMbGFyZ2VzdEJsb2Nr'
    'EiIKDW1pbl9mcmVlX2hlYXAYBCABKA1SC21pbkZyZWVIZWFw');

@$core.Deprecated('Use getMemoryProfileRequestDescriptor instead')
const GetMemoryProfileRequest$json = {
  '1': 'GetMemoryProfileRequest',
};

/// Descriptor for `GetMemoryProfileRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getMemoryProfileRequestDescriptor =
    $convert.base64Decode('ChdHZXRNZW1vcnlQcm9maWxlUmVxdWVzdA==');

@$core.Deprecated('Use getMemoryProfileResponseDescriptor instead')
const GetMemoryProfileResponse$json = {
  '1': 'GetMemoryProfileResponse',
  '2': [
    {
      '1': 'current_free_heap',
      '3': 1,
      '4': 1,
      '5': 13,
      '10': 'currentFreeHeap'
    },
    {
      '1': 'current_min_free_heap',
      '3': 2,
      '4': 1,
      '5': 13,
      '10': 'currentMinFreeHeap'
    },
    {
      '1': 'current_largest_block',
      '3': 3,
      '4': 1,
      '5': 13,
      '10': 'currentLargestBlock'
    },
    {'1': 'total_heap', '3': 4, '4': 1, '5': 13, '10': 'totalHeap'},
    {
      '1': 'samples',
      '3': 5,
      '4': 3,
      '5': 11,
      '6': '.domes.config.HeapSample',
      '10': 'samples'
    },
  ],
};

/// Descriptor for `GetMemoryProfileResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getMemoryProfileResponseDescriptor = $convert.base64Decode(
    'ChhHZXRNZW1vcnlQcm9maWxlUmVzcG9uc2USKgoRY3VycmVudF9mcmVlX2hlYXAYASABKA1SD2'
    'N1cnJlbnRGcmVlSGVhcBIxChVjdXJyZW50X21pbl9mcmVlX2hlYXAYAiABKA1SEmN1cnJlbnRN'
    'aW5GcmVlSGVhcBIyChVjdXJyZW50X2xhcmdlc3RfYmxvY2sYAyABKA1SE2N1cnJlbnRMYXJnZX'
    'N0QmxvY2sSHQoKdG90YWxfaGVhcBgEIAEoDVIJdG90YWxIZWFwEjIKB3NhbXBsZXMYBSADKAsy'
    'GC5kb21lcy5jb25maWcuSGVhcFNhbXBsZVIHc2FtcGxlcw==');

@$core.Deprecated('Use selfTestResultDescriptor instead')
const SelfTestResult$json = {
  '1': 'SelfTestResult',
  '2': [
    {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    {'1': 'passed', '3': 2, '4': 1, '5': 8, '10': 'passed'},
    {'1': 'message', '3': 3, '4': 1, '5': 9, '10': 'message'},
  ],
};

/// Descriptor for `SelfTestResult`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List selfTestResultDescriptor = $convert.base64Decode(
    'Cg5TZWxmVGVzdFJlc3VsdBISCgRuYW1lGAEgASgJUgRuYW1lEhYKBnBhc3NlZBgCIAEoCFIGcG'
    'Fzc2VkEhgKB21lc3NhZ2UYAyABKAlSB21lc3NhZ2U=');

@$core.Deprecated('Use selfTestRequestDescriptor instead')
const SelfTestRequest$json = {
  '1': 'SelfTestRequest',
};

/// Descriptor for `SelfTestRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List selfTestRequestDescriptor =
    $convert.base64Decode('Cg9TZWxmVGVzdFJlcXVlc3Q=');

@$core.Deprecated('Use selfTestResponseDescriptor instead')
const SelfTestResponse$json = {
  '1': 'SelfTestResponse',
  '2': [
    {'1': 'tests_run', '3': 1, '4': 1, '5': 13, '10': 'testsRun'},
    {'1': 'tests_passed', '3': 2, '4': 1, '5': 13, '10': 'testsPassed'},
    {
      '1': 'results',
      '3': 3,
      '4': 3,
      '5': 11,
      '6': '.domes.config.SelfTestResult',
      '10': 'results'
    },
  ],
};

/// Descriptor for `SelfTestResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List selfTestResponseDescriptor = $convert.base64Decode(
    'ChBTZWxmVGVzdFJlc3BvbnNlEhsKCXRlc3RzX3J1bhgBIAEoDVIIdGVzdHNSdW4SIQoMdGVzdH'
    'NfcGFzc2VkGAIgASgNUgt0ZXN0c1Bhc3NlZBI2CgdyZXN1bHRzGAMgAygLMhwuZG9tZXMuY29u'
    'ZmlnLlNlbGZUZXN0UmVzdWx0UgdyZXN1bHRz');

@$core.Deprecated('Use checkUpdateRequestDescriptor instead')
const CheckUpdateRequest$json = {
  '1': 'CheckUpdateRequest',
};

/// Descriptor for `CheckUpdateRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List checkUpdateRequestDescriptor =
    $convert.base64Decode('ChJDaGVja1VwZGF0ZVJlcXVlc3Q=');

@$core.Deprecated('Use checkUpdateResponseDescriptor instead')
const CheckUpdateResponse$json = {
  '1': 'CheckUpdateResponse',
  '2': [
    {'1': 'update_available', '3': 1, '4': 1, '5': 8, '10': 'updateAvailable'},
    {'1': 'current_version', '3': 2, '4': 1, '5': 9, '10': 'currentVersion'},
    {
      '1': 'available_version',
      '3': 3,
      '4': 1,
      '5': 9,
      '10': 'availableVersion'
    },
    {'1': 'firmware_size', '3': 4, '4': 1, '5': 13, '10': 'firmwareSize'},
    {
      '1': 'auto_update_enabled',
      '3': 5,
      '4': 1,
      '5': 8,
      '10': 'autoUpdateEnabled'
    },
  ],
};

/// Descriptor for `CheckUpdateResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List checkUpdateResponseDescriptor = $convert.base64Decode(
    'ChNDaGVja1VwZGF0ZVJlc3BvbnNlEikKEHVwZGF0ZV9hdmFpbGFibGUYASABKAhSD3VwZGF0ZU'
    'F2YWlsYWJsZRInCg9jdXJyZW50X3ZlcnNpb24YAiABKAlSDmN1cnJlbnRWZXJzaW9uEisKEWF2'
    'YWlsYWJsZV92ZXJzaW9uGAMgASgJUhBhdmFpbGFibGVWZXJzaW9uEiMKDWZpcm13YXJlX3Npem'
    'UYBCABKA1SDGZpcm13YXJlU2l6ZRIuChNhdXRvX3VwZGF0ZV9lbmFibGVkGAUgASgIUhFhdXRv'
    'VXBkYXRlRW5hYmxlZA==');

@$core.Deprecated('Use setAutoUpdateRequestDescriptor instead')
const SetAutoUpdateRequest$json = {
  '1': 'SetAutoUpdateRequest',
  '2': [
    {'1': 'enabled', '3': 1, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `SetAutoUpdateRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setAutoUpdateRequestDescriptor =
    $convert.base64Decode(
        'ChRTZXRBdXRvVXBkYXRlUmVxdWVzdBIYCgdlbmFibGVkGAEgASgIUgdlbmFibGVk');

@$core.Deprecated('Use setAutoUpdateResponseDescriptor instead')
const SetAutoUpdateResponse$json = {
  '1': 'SetAutoUpdateResponse',
  '2': [
    {'1': 'enabled', '3': 1, '4': 1, '5': 8, '10': 'enabled'},
  ],
};

/// Descriptor for `SetAutoUpdateResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setAutoUpdateResponseDescriptor =
    $convert.base64Decode(
        'ChVTZXRBdXRvVXBkYXRlUmVzcG9uc2USGAoHZW5hYmxlZBgBIAEoCFIHZW5hYmxlZA==');

@$core.Deprecated('Use simulateTouchRequestDescriptor instead')
const SimulateTouchRequest$json = {
  '1': 'SimulateTouchRequest',
  '2': [
    {'1': 'pad_index', '3': 1, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `SimulateTouchRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List simulateTouchRequestDescriptor =
    $convert.base64Decode(
        'ChRTaW11bGF0ZVRvdWNoUmVxdWVzdBIbCglwYWRfaW5kZXgYASABKA1SCHBhZEluZGV4');

@$core.Deprecated('Use simulateTouchResponseDescriptor instead')
const SimulateTouchResponse$json = {
  '1': 'SimulateTouchResponse',
  '9': [
    {'1': 1, '2': 2},
  ],
};

/// Descriptor for `SimulateTouchResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List simulateTouchResponseDescriptor =
    $convert.base64Decode('ChVTaW11bGF0ZVRvdWNoUmVzcG9uc2VKBAgBEAI=');

@$core.Deprecated('Use setSimModeRequestDescriptor instead')
const SetSimModeRequest$json = {
  '1': 'SetSimModeRequest',
  '2': [
    {'1': 'enabled', '3': 1, '4': 1, '5': 8, '10': 'enabled'},
    {'1': 'delay_ms', '3': 2, '4': 1, '5': 13, '10': 'delayMs'},
    {'1': 'pad_index', '3': 3, '4': 1, '5': 13, '10': 'padIndex'},
  ],
};

/// Descriptor for `SetSimModeRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setSimModeRequestDescriptor = $convert.base64Decode(
    'ChFTZXRTaW1Nb2RlUmVxdWVzdBIYCgdlbmFibGVkGAEgASgIUgdlbmFibGVkEhkKCGRlbGF5X2'
    '1zGAIgASgNUgdkZWxheU1zEhsKCXBhZF9pbmRleBgDIAEoDVIIcGFkSW5kZXg=');

@$core.Deprecated('Use setSimModeResponseDescriptor instead')
const SetSimModeResponse$json = {
  '1': 'SetSimModeResponse',
  '2': [
    {'1': 'enabled', '3': 2, '4': 1, '5': 8, '10': 'enabled'},
    {'1': 'delay_ms', '3': 3, '4': 1, '5': 13, '10': 'delayMs'},
    {'1': 'pad_index', '3': 4, '4': 1, '5': 13, '10': 'padIndex'},
  ],
  '9': [
    {'1': 1, '2': 2},
  ],
};

/// Descriptor for `SetSimModeResponse`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setSimModeResponseDescriptor = $convert.base64Decode(
    'ChJTZXRTaW1Nb2RlUmVzcG9uc2USGAoHZW5hYmxlZBgCIAEoCFIHZW5hYmxlZBIZCghkZWxheV'
    '9tcxgDIAEoDVIHZGVsYXlNcxIbCglwYWRfaW5kZXgYBCABKA1SCHBhZEluZGV4SgQIARAC');

@$core.Deprecated('Use touchEventNotificationDescriptor instead')
const TouchEventNotification$json = {
  '1': 'TouchEventNotification',
  '2': [
    {'1': 'pod_id', '3': 1, '4': 1, '5': 13, '10': 'podId'},
    {'1': 'pad_index', '3': 2, '4': 1, '5': 13, '10': 'padIndex'},
    {'1': 'timestamp_us', '3': 3, '4': 1, '5': 4, '10': 'timestampUs'},
  ],
};

/// Descriptor for `TouchEventNotification`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List touchEventNotificationDescriptor = $convert.base64Decode(
    'ChZUb3VjaEV2ZW50Tm90aWZpY2F0aW9uEhUKBnBvZF9pZBgBIAEoDVIFcG9kSWQSGwoJcGFkX2'
    'luZGV4GAIgASgNUghwYWRJbmRleBIhCgx0aW1lc3RhbXBfdXMYAyABKARSC3RpbWVzdGFtcFVz');
