/**
 * @file test_ota_protocol.cpp
 * @brief Unit tests for OTA protocol serialization/deserialization
 */

#include "unity.h"
#include "protocol/otaProtocol.hpp"

#include <array>
#include <cstring>

using namespace domes;

// =============================================================================
// OTA_BEGIN Tests
// =============================================================================

extern "C" void test_OTA_BEGIN_serialize_deserialize_round_trip(void) {
    std::array<uint8_t, 128> buf;
    size_t len = 0;

    uint32_t firmwareSize = 123456;
    uint8_t sha256[kSha256Size];
    for (size_t i = 0; i < kSha256Size; ++i) {
        sha256[i] = static_cast<uint8_t>(i);
    }
    const char* version = "v1.2.3-test";

    TransportError err = serializeOtaBegin(firmwareSize, sha256, version, buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaBeginPayload), len);

    uint32_t outSize = 0;
    uint8_t outSha[kSha256Size] = {0};
    char outVersion[kOtaVersionMaxLen] = {0};

    err = deserializeOtaBegin(buf.data(), len, &outSize, outSha, outVersion, sizeof(outVersion));
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL_UINT32(firmwareSize, outSize);
    TEST_ASSERT_EQUAL_MEMORY(sha256, outSha, kSha256Size);
    TEST_ASSERT_EQUAL_STRING(version, outVersion);
}

extern "C" void test_OTA_BEGIN_serialize_with_null_SHA256(void) {
    std::array<uint8_t, 128> buf;
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, "v1.0.0", buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);

    uint32_t outSize = 0;
    uint8_t outSha[kSha256Size];
    std::memset(outSha, 0xFF, sizeof(outSha));

    deserializeOtaBegin(buf.data(), len, &outSize, outSha, nullptr, 0);

    for (size_t i = 0; i < kSha256Size; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x00, outSha[i]);
    }
}

extern "C" void test_OTA_BEGIN_serialize_with_null_version(void) {
    std::array<uint8_t, 128> buf;
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, nullptr, buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);

    uint32_t outSize = 0;
    char outVersion[32] = "garbage";

    deserializeOtaBegin(buf.data(), len, &outSize, nullptr, outVersion, sizeof(outVersion));
    TEST_ASSERT_EQUAL_STRING("", outVersion);
}

extern "C" void test_OTA_BEGIN_buffer_too_small_returns_error(void) {
    std::array<uint8_t, 10> smallBuf;
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, "v1.0.0", smallBuf.data(), smallBuf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

extern "C" void test_OTA_BEGIN_deserialize_with_short_payload_returns_error(void) {
    uint8_t shortPayload[10] = {0};
    uint32_t outSize = 0;

    TransportError err = deserializeOtaBegin(shortPayload, sizeof(shortPayload), &outSize, nullptr, nullptr, 0);
    TEST_ASSERT_EQUAL(TransportError::kProtocolError, err);
}

// =============================================================================
// OTA_DATA Tests
// =============================================================================

extern "C" void test_OTA_DATA_serialize_deserialize_round_trip(void) {
    std::array<uint8_t, 128> buf;
    size_t len = 0;

    uint32_t offset = 4096;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    TransportError err = serializeOtaData(offset, data, sizeof(data), buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaDataHeader) + sizeof(data), len);

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;

    err = deserializeOtaData(buf.data(), len, &outOffset, &outData, &outDataLen);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL_UINT32(offset, outOffset);
    TEST_ASSERT_EQUAL(sizeof(data), outDataLen);
    TEST_ASSERT_NOT_NULL(outData);
    TEST_ASSERT_EQUAL_MEMORY(data, outData, sizeof(data));
}

extern "C" void test_OTA_DATA_serialize_empty_payload(void) {
    std::array<uint8_t, 32> buf;
    size_t len = 0;

    TransportError err = serializeOtaData(0, nullptr, 0, buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaDataHeader), len);

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 99;

    err = deserializeOtaData(buf.data(), len, &outOffset, &outData, &outDataLen);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(0, outDataLen);
}

extern "C" void test_OTA_DATA_serialize_max_chunk_size(void) {
    std::array<uint8_t, kOtaChunkSize + 16> buf;
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize> data;
    data.fill(0x42);

    TransportError err = serializeOtaData(0, data.data(), data.size(), buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaDataHeader) + kOtaChunkSize, len);
}

extern "C" void test_OTA_DATA_serialize_oversized_chunk_returns_error(void) {
    std::array<uint8_t, 2048> buf;
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize + 1> oversizedData;

    TransportError err = serializeOtaData(0, oversizedData.data(), oversizedData.size(), buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

extern "C" void test_OTA_DATA_deserialize_with_truncated_payload_returns_error(void) {
    OtaDataHeader header;
    header.offset = 0;
    header.length = 100;

    uint8_t payload[sizeof(OtaDataHeader) + 10];
    std::memcpy(payload, &header, sizeof(header));

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;

    TransportError err = deserializeOtaData(payload, sizeof(payload), &outOffset, &outData, &outDataLen);
    TEST_ASSERT_EQUAL(TransportError::kProtocolError, err);
}

// =============================================================================
// OTA_END Tests
// =============================================================================

extern "C" void test_OTA_END_serialize_produces_zero_length_payload(void) {
    std::array<uint8_t, 32> buf;
    size_t len = 99;

    TransportError err = serializeOtaEnd(buf.data(), buf.size(), &len);

    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(0, len);
}

extern "C" void test_OTA_END_serialize_with_null_outLen_returns_error(void) {
    std::array<uint8_t, 32> buf;

    TransportError err = serializeOtaEnd(buf.data(), buf.size(), nullptr);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

// =============================================================================
// OTA_ACK Tests
// =============================================================================

extern "C" void test_OTA_ACK_serialize_deserialize_round_trip(void) {
    std::array<uint8_t, 32> buf;
    size_t len = 0;

    OtaStatus status = OtaStatus::kOk;
    uint32_t nextOffset = 2048;

    TransportError err = serializeOtaAck(status, nextOffset, buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaAckPayload), len);

    OtaStatus outStatus = OtaStatus::kAborted;
    uint32_t outOffset = 0;

    err = deserializeOtaAck(buf.data(), len, &outStatus, &outOffset);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(OtaStatus::kOk, outStatus);
    TEST_ASSERT_EQUAL_UINT32(nextOffset, outOffset);
}

extern "C" void test_OTA_ACK_all_status_codes(void) {
    std::array<uint8_t, 32> buf;
    size_t len = 0;

    OtaStatus statuses[] = {
        OtaStatus::kOk, OtaStatus::kBusy, OtaStatus::kFlashError,
        OtaStatus::kVerifyFailed, OtaStatus::kSizeMismatch,
        OtaStatus::kOffsetMismatch, OtaStatus::kVersionError,
        OtaStatus::kPartitionError, OtaStatus::kAborted,
    };

    for (auto status : statuses) {
        TransportError err = serializeOtaAck(status, 0, buf.data(), buf.size(), &len);
        TEST_ASSERT_EQUAL(TransportError::kOk, err);

        OtaStatus outStatus;
        uint32_t outOffset;
        err = deserializeOtaAck(buf.data(), len, &outStatus, &outOffset);
        TEST_ASSERT_EQUAL(TransportError::kOk, err);
        TEST_ASSERT_EQUAL(status, outStatus);
    }
}

extern "C" void test_OTA_ACK_buffer_too_small_returns_error(void) {
    std::array<uint8_t, 2> smallBuf;
    size_t len = 0;

    TransportError err = serializeOtaAck(OtaStatus::kOk, 0, smallBuf.data(), smallBuf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

// =============================================================================
// OTA_ABORT Tests
// =============================================================================

extern "C" void test_OTA_ABORT_serialize_deserialize_round_trip(void) {
    std::array<uint8_t, 32> buf;
    size_t len = 0;

    OtaStatus reason = OtaStatus::kFlashError;

    TransportError err = serializeOtaAbort(reason, buf.data(), buf.size(), &len);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(sizeof(OtaAbortPayload), len);

    OtaStatus outReason = OtaStatus::kOk;
    err = deserializeOtaAbort(buf.data(), len, &outReason);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(OtaStatus::kFlashError, outReason);
}

extern "C" void test_OTA_ABORT_deserialize_with_null_reason_returns_error(void) {
    uint8_t payload[] = {0x02};

    TransportError err = deserializeOtaAbort(payload, sizeof(payload), nullptr);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

extern "C" void test_OTA_ABORT_deserialize_with_empty_payload_returns_error(void) {
    OtaStatus outReason;

    TransportError err = deserializeOtaAbort(nullptr, 0, &outReason);
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

// =============================================================================
// Null Argument Tests
// =============================================================================

extern "C" void test_All_serializers_reject_null_buffer(void) {
    size_t len = 0;

    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaBegin(0, nullptr, nullptr, nullptr, 0, &len));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaData(0, nullptr, 0, nullptr, 0, &len));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaAck(OtaStatus::kOk, 0, nullptr, 0, &len));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaAbort(OtaStatus::kOk, nullptr, 0, &len));
}

extern "C" void test_All_serializers_reject_null_outLen(void) {
    std::array<uint8_t, 128> buf;

    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaBegin(0, nullptr, nullptr, buf.data(), buf.size(), nullptr));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaData(0, nullptr, 0, buf.data(), buf.size(), nullptr));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaAck(OtaStatus::kOk, 0, buf.data(), buf.size(), nullptr));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        serializeOtaAbort(OtaStatus::kOk, buf.data(), buf.size(), nullptr));
}

extern "C" void test_All_deserializers_reject_null_required_outputs(void) {
    uint8_t payload[128] = {0};

    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        deserializeOtaBegin(payload, sizeof(OtaBeginPayload), nullptr, nullptr, nullptr, 0));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        deserializeOtaData(payload, sizeof(OtaDataHeader), nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        deserializeOtaAck(payload, sizeof(OtaAckPayload), nullptr, nullptr));
    TEST_ASSERT_EQUAL(TransportError::kInvalidArg,
        deserializeOtaAbort(payload, sizeof(OtaAbortPayload), nullptr));
}
