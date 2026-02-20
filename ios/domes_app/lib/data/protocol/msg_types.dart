/// Message type constants for protocols NOT covered by config.proto.
///
/// Config/system message types are defined in config.proto and accessed via
/// the generated MsgType enum (e.g. MsgType.MSG_TYPE_LIST_FEATURES_REQ.value).
///
/// OTA message types are a separate binary protocol not in any .proto file,
/// so we define them here.
library;

// OTA message types (0x01-0x05) — binary protocol, not in config.proto
const int kOtaBegin = 0x01;
const int kOtaData = 0x02;
const int kOtaEnd = 0x03;
const int kOtaAck = 0x04;
const int kOtaAbort = 0x05;
