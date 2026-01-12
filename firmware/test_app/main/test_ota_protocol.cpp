/**
 * @file test_ota_protocol.cpp
 * @brief Unit tests for OTA protocol serialization/deserialization
 */

#include <gtest/gtest.h>
#include "protocol/otaProtocol.hpp"

#include <array>
#include <cstring>

using namespace domes;

// =============================================================================
// OTA_BEGIN Tests
// =============================================================================

TEST(OtaBegin, SerializeDeserializeRoundTrip) {
    std::array<uint8_t, 128> buf{};
    size_t len = 0;

    uint32_t firmwareSize = 123456;
    uint8_t sha256[kSha256Size];
    for (size_t i = 0; i < kSha256Size; ++i) {
        sha256[i] = static_cast<uint8_t>(i);
    }
    const char* version = "v1.2.3-test";

    TransportError err = serializeOtaBegin(firmwareSize, sha256, version, buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaBeginPayload), len);

    uint32_t outSize = 0;
    uint8_t outSha[kSha256Size] = {0};
    char outVersion[kOtaVersionMaxLen] = {0};

    err = deserializeOtaBegin(buf.data(), len, &outSize, outSha, outVersion, sizeof(outVersion));
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(firmwareSize, outSize);
    EXPECT_EQ(0, std::memcmp(sha256, outSha, kSha256Size));
    EXPECT_STREQ(version, outVersion);
}

TEST(OtaBegin, SerializeWithNullSHA256) {
    std::array<uint8_t, 128> buf{};
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, "v1.0.0", buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);

    uint32_t outSize = 0;
    uint8_t outSha[kSha256Size];
    std::memset(outSha, 0xFF, sizeof(outSha));

    deserializeOtaBegin(buf.data(), len, &outSize, outSha, nullptr, 0);

    for (size_t i = 0; i < kSha256Size; ++i) {
        EXPECT_EQ(0x00, outSha[i]);
    }
}

TEST(OtaBegin, SerializeWithNullVersion) {
    std::array<uint8_t, 128> buf{};
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, nullptr, buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);

    uint32_t outSize = 0;
    char outVersion[32] = "garbage";

    deserializeOtaBegin(buf.data(), len, &outSize, nullptr, outVersion, sizeof(outVersion));
    EXPECT_STREQ("", outVersion);
}

TEST(OtaBegin, BufferTooSmallReturnsError) {
    std::array<uint8_t, 10> smallBuf{};
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, "v1.0.0", smallBuf.data(), smallBuf.size(), &len);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(OtaBegin, DeserializeWithShortPayloadReturnsError) {
    uint8_t shortPayload[10] = {0};
    uint32_t outSize = 0;

    TransportError err = deserializeOtaBegin(shortPayload, sizeof(shortPayload), &outSize, nullptr, nullptr, 0);
    EXPECT_EQ(TransportError::kProtocolError, err);
}

// =============================================================================
// OTA_DATA Tests
// =============================================================================

TEST(OtaData, SerializeDeserializeRoundTrip) {
    std::array<uint8_t, 128> buf{};
    size_t len = 0;

    uint32_t offset = 4096;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    TransportError err = serializeOtaData(offset, data, sizeof(data), buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaDataHeader) + sizeof(data), len);

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;

    err = deserializeOtaData(buf.data(), len, &outOffset, &outData, &outDataLen);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(offset, outOffset);
    EXPECT_EQ(sizeof(data), outDataLen);
    ASSERT_NE(nullptr, outData);
    EXPECT_EQ(0, std::memcmp(data, outData, sizeof(data)));
}

TEST(OtaData, SerializeEmptyPayload) {
    std::array<uint8_t, 32> buf{};
    size_t len = 0;

    TransportError err = serializeOtaData(0, nullptr, 0, buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaDataHeader), len);

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 99;

    err = deserializeOtaData(buf.data(), len, &outOffset, &outData, &outDataLen);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(0u, outDataLen);
}

TEST(OtaData, SerializeMaxChunkSize) {
    std::array<uint8_t, kOtaChunkSize + 16> buf{};
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize> data;
    data.fill(0x42);

    TransportError err = serializeOtaData(0, data.data(), data.size(), buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaDataHeader) + kOtaChunkSize, len);
}

TEST(OtaData, SerializeOversizedChunkReturnsError) {
    std::array<uint8_t, 2048> buf{};
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize + 1> oversizedData{};

    TransportError err = serializeOtaData(0, oversizedData.data(), oversizedData.size(), buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(OtaData, DeserializeWithTruncatedPayloadReturnsError) {
    OtaDataHeader header;
    header.offset = 0;
    header.length = 100;

    uint8_t payload[sizeof(OtaDataHeader) + 10];
    std::memcpy(payload, &header, sizeof(header));

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;

    TransportError err = deserializeOtaData(payload, sizeof(payload), &outOffset, &outData, &outDataLen);
    EXPECT_EQ(TransportError::kProtocolError, err);
}

// =============================================================================
// OTA_END Tests
// =============================================================================

TEST(OtaEnd, SerializeProducesZeroLengthPayload) {
    std::array<uint8_t, 32> buf{};
    size_t len = 99;

    TransportError err = serializeOtaEnd(buf.data(), buf.size(), &len);

    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(0u, len);
}

TEST(OtaEnd, SerializeWithNullOutLenReturnsError) {
    std::array<uint8_t, 32> buf{};

    TransportError err = serializeOtaEnd(buf.data(), buf.size(), nullptr);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

// =============================================================================
// OTA_ACK Tests
// =============================================================================

TEST(OtaAck, SerializeDeserializeRoundTrip) {
    std::array<uint8_t, 32> buf{};
    size_t len = 0;

    OtaStatus status = OtaStatus::kOk;
    uint32_t nextOffset = 2048;

    TransportError err = serializeOtaAck(status, nextOffset, buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaAckPayload), len);

    OtaStatus outStatus = OtaStatus::kAborted;
    uint32_t outOffset = 0;

    err = deserializeOtaAck(buf.data(), len, &outStatus, &outOffset);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(OtaStatus::kOk, outStatus);
    EXPECT_EQ(nextOffset, outOffset);
}

TEST(OtaAck, AllStatusCodes) {
    std::array<uint8_t, 32> buf{};
    size_t len = 0;

    OtaStatus statuses[] = {
        OtaStatus::kOk, OtaStatus::kBusy, OtaStatus::kFlashError,
        OtaStatus::kVerifyFailed, OtaStatus::kSizeMismatch,
        OtaStatus::kOffsetMismatch, OtaStatus::kVersionError,
        OtaStatus::kPartitionError, OtaStatus::kAborted,
    };

    for (auto status : statuses) {
        TransportError err = serializeOtaAck(status, 0, buf.data(), buf.size(), &len);
        EXPECT_EQ(TransportError::kOk, err);

        OtaStatus outStatus;
        uint32_t outOffset;
        err = deserializeOtaAck(buf.data(), len, &outStatus, &outOffset);
        EXPECT_EQ(TransportError::kOk, err);
        EXPECT_EQ(status, outStatus);
    }
}

TEST(OtaAck, BufferTooSmallReturnsError) {
    std::array<uint8_t, 2> smallBuf{};
    size_t len = 0;

    TransportError err = serializeOtaAck(OtaStatus::kOk, 0, smallBuf.data(), smallBuf.size(), &len);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

// =============================================================================
// OTA_ABORT Tests
// =============================================================================

TEST(OtaAbort, SerializeDeserializeRoundTrip) {
    std::array<uint8_t, 32> buf{};
    size_t len = 0;

    OtaStatus reason = OtaStatus::kFlashError;

    TransportError err = serializeOtaAbort(reason, buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaAbortPayload), len);

    OtaStatus outReason = OtaStatus::kOk;
    err = deserializeOtaAbort(buf.data(), len, &outReason);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(OtaStatus::kFlashError, outReason);
}

TEST(OtaAbort, DeserializeWithNullReasonReturnsError) {
    uint8_t payload[] = {0x02};

    TransportError err = deserializeOtaAbort(payload, sizeof(payload), nullptr);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(OtaAbort, DeserializeWithEmptyPayloadReturnsError) {
    OtaStatus outReason;

    TransportError err = deserializeOtaAbort(nullptr, 0, &outReason);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

// =============================================================================
// Null Argument Tests
// =============================================================================

TEST(OtaSerializer, RejectsNullBuffer) {
    size_t len = 0;

    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaBegin(0, nullptr, nullptr, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaData(0, nullptr, 0, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaAck(OtaStatus::kOk, 0, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaAbort(OtaStatus::kOk, nullptr, 0, &len));
}

TEST(OtaSerializer, RejectsNullOutLen) {
    std::array<uint8_t, 128> buf{};

    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaBegin(0, nullptr, nullptr, buf.data(), buf.size(), nullptr));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaData(0, nullptr, 0, buf.data(), buf.size(), nullptr));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaAck(OtaStatus::kOk, 0, buf.data(), buf.size(), nullptr));
    EXPECT_EQ(TransportError::kInvalidArg,
        serializeOtaAbort(OtaStatus::kOk, buf.data(), buf.size(), nullptr));
}

TEST(OtaDeserializer, RejectsNullRequiredOutputs) {
    uint8_t payload[128] = {0};

    EXPECT_EQ(TransportError::kInvalidArg,
        deserializeOtaBegin(payload, sizeof(OtaBeginPayload), nullptr, nullptr, nullptr, 0));
    EXPECT_EQ(TransportError::kInvalidArg,
        deserializeOtaData(payload, sizeof(OtaDataHeader), nullptr, nullptr, nullptr));
    EXPECT_EQ(TransportError::kInvalidArg,
        deserializeOtaAck(payload, sizeof(OtaAckPayload), nullptr, nullptr));
    EXPECT_EQ(TransportError::kInvalidArg,
        deserializeOtaAbort(payload, sizeof(OtaAbortPayload), nullptr));
}
