/// Message type constants for the DOMES config protocol.
///
/// These mirror the MsgType enum from config.proto.
/// We define them as constants here because protobuf enums
/// in Dart don't directly map to the wire byte values we need
/// for frame encoding.
library;

// Config commands (0x20-0x2F range)
const int kListFeaturesReq = 0x20;
const int kListFeaturesRsp = 0x21;
const int kSetFeatureReq = 0x22;
const int kSetFeatureRsp = 0x23;
const int kGetFeatureReq = 0x24;
const int kGetFeatureRsp = 0x25;
const int kSetLedPatternReq = 0x26;
const int kSetLedPatternRsp = 0x27;
const int kGetLedPatternReq = 0x28;
const int kGetLedPatternRsp = 0x29;
const int kSetImuTriageReq = 0x2A;
const int kSetImuTriageRsp = 0x2B;

// System mode commands (0x30-0x35 range)
const int kGetModeReq = 0x30;
const int kGetModeRsp = 0x31;
const int kSetModeReq = 0x32;
const int kSetModeRsp = 0x33;
const int kGetSystemInfoReq = 0x34;
const int kGetSystemInfoRsp = 0x35;

// OTA message types (0x01-0x05)
const int kOtaBegin = 0x01;
const int kOtaData = 0x02;
const int kOtaEnd = 0x03;
const int kOtaAck = 0x04;
const int kOtaAbort = 0x05;
