/**
 * @file test_ota_protocol.cpp
 * @brief Unit tests for OTA protocol serialization/deserialization
 */

#include "protocol/otaProtocol.hpp"

#include <array>
#include <cstring>

#include <gtest/gtest.h>

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

    TransportError err =
        serializeOtaBegin(firmwareSize, sha256, version, buf.data(), buf.size(), &len);
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

TEST(OtaBegin, SerializeWithNullSHA256ReturnsError) {
    std::array<uint8_t, 128> buf{};
    size_t len = 0;

    TransportError err = serializeOtaBegin(1000, nullptr, "v1.0.0", buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(OtaBegin, SerializeRejectsMissingVersion) {
    std::array<uint8_t, 128> buf{};
    std::array<uint8_t, kSha256Size> sha256{};
    size_t len = 0;

    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaBegin(1000, sha256.data(), nullptr, buf.data(), buf.size(), &len));
    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaBegin(1000, sha256.data(), "", buf.data(), buf.size(), &len));
}

TEST(OtaBegin, RejectsVersionWithoutRoomForNullTerminator) {
    std::array<uint8_t, 128> buf{};
    std::array<uint8_t, kSha256Size> sha256{};
    std::array<char, kOtaVersionMaxLen + 1> version{};
    version.fill('x');
    version.back() = '\0';
    size_t len = 0;

    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaBegin(1000, sha256.data(), version.data(), buf.data(), buf.size(), &len));
}

TEST(OtaBegin, AcceptsLongestNullTerminatedVersion) {
    std::array<uint8_t, 128> buf{};
    std::array<uint8_t, kSha256Size> sha256{};
    std::array<char, kOtaVersionMaxLen> version{};
    version.fill('x');
    version.back() = '\0';
    size_t len = 0;

    EXPECT_EQ(TransportError::kOk,
              serializeOtaBegin(1000, sha256.data(), version.data(), buf.data(), buf.size(), &len));
}

TEST(OtaBegin, BufferTooSmallReturnsError) {
    std::array<uint8_t, 10> smallBuf{};
    std::array<uint8_t, kSha256Size> sha256{};
    size_t len = 0;

    TransportError err =
        serializeOtaBegin(1000, sha256.data(), "v1.0.0", smallBuf.data(), smallBuf.size(), &len);
    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(OtaBegin, DeserializeWithShortPayloadReturnsError) {
    uint8_t shortPayload[10] = {0};
    uint32_t outSize = 0;

    TransportError err =
        deserializeOtaBegin(shortPayload, sizeof(shortPayload), &outSize, nullptr, nullptr, 0);
    EXPECT_EQ(TransportError::kProtocolError, err);
}

TEST(OtaBegin, DeserializeRejectsOversizedPayload) {
    std::array<uint8_t, sizeof(OtaBeginPayload) + 1> payload{};
    auto* begin = reinterpret_cast<OtaBeginPayload*>(payload.data());
    std::memcpy(begin->version.data(), "v1.0.0", 7);
    uint32_t outSize = 0;

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaBegin(payload.data(), payload.size(), &outSize, nullptr, nullptr, 0));
}

TEST(OtaBegin, DeserializeRejectsVersionWithoutNullTerminator) {
    OtaBeginPayload payload{};
    payload.version.fill('x');
    uint32_t outSize = 0;
    char version[kOtaVersionMaxLen]{};

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaBegin(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload),
                                  &outSize, nullptr, version, sizeof(version)));
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

TEST(OtaData, RejectsZeroLengthChunk) {
    std::array<uint8_t, 32> buf{};
    size_t len = 0;

    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaData(0, nullptr, 0, buf.data(), buf.size(), &len));

    OtaDataHeader header{};
    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;
    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaData(reinterpret_cast<const uint8_t*>(&header), sizeof(header),
                                 &outOffset, &outData, &outDataLen));
}

TEST(OtaData, AcceptsOneByteMinimumChunk) {
    std::array<uint8_t, 32> buf{};
    const uint8_t input = 0x42;
    size_t len = 0;

    ASSERT_EQ(TransportError::kOk, serializeOtaData(7, &input, 1, buf.data(), buf.size(), &len));

    uint32_t outOffset = 0;
    const uint8_t* outData = nullptr;
    size_t outDataLen = 0;
    ASSERT_EQ(TransportError::kOk,
              deserializeOtaData(buf.data(), len, &outOffset, &outData, &outDataLen));
    EXPECT_EQ(7u, outOffset);
    ASSERT_EQ(1u, outDataLen);
    EXPECT_EQ(input, outData[0]);
}

TEST(OtaData, SerializeMaxChunkSize) {
    std::array<uint8_t, kOtaChunkSize + 16> buf{};
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize> data;
    data.fill(0x42);

    TransportError err =
        serializeOtaData(0, data.data(), data.size(), buf.data(), buf.size(), &len);
    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(sizeof(OtaDataHeader) + kOtaChunkSize, len);
}

TEST(OtaData, SerializeOversizedChunkReturnsError) {
    std::array<uint8_t, 2048> buf{};
    size_t len = 0;

    std::array<uint8_t, kOtaChunkSize + 1> oversizedData{};

    TransportError err = serializeOtaData(0, oversizedData.data(), oversizedData.size(), buf.data(),
                                          buf.size(), &len);
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

    TransportError err =
        deserializeOtaData(payload, sizeof(payload), &outOffset, &outData, &outDataLen);
    EXPECT_EQ(TransportError::kProtocolError, err);
}

TEST(OtaData, DeserializeRejectsTrailingBytes) {
    std::array<uint8_t, sizeof(OtaDataHeader) + 2> payload{};
    auto* header = reinterpret_cast<OtaDataHeader*>(payload.data());
    header->length = 1;
    uint32_t offset = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaData(payload.data(), payload.size(), &offset, &data, &dataLen));
}

TEST(OtaData, DeserializeRejectsChunkAboveProtocolMaximum) {
    std::array<uint8_t, sizeof(OtaDataHeader) + kOtaChunkSize + 1> payload{};
    auto* header = reinterpret_cast<OtaDataHeader*>(payload.data());
    header->length = static_cast<uint16_t>(kOtaChunkSize + 1);
    uint32_t offset = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaData(payload.data(), payload.size(), &offset, &data, &dataLen));
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

TEST(OtaEnd, DeserializeRequiresEmptyPayload) {
    const uint8_t byte = 0;
    EXPECT_EQ(TransportError::kOk, deserializeOtaEnd(nullptr, 0));
    EXPECT_EQ(TransportError::kProtocolError, deserializeOtaEnd(&byte, 1));
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
        OtaStatus::kOk,           OtaStatus::kBusy,           OtaStatus::kFlashError,
        OtaStatus::kVerifyFailed, OtaStatus::kSizeMismatch,   OtaStatus::kOffsetMismatch,
        OtaStatus::kVersionError, OtaStatus::kPartitionError, OtaStatus::kAborted,
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

TEST(OtaAck, RejectsTrailingBytesAndUnknownStatus) {
    std::array<uint8_t, sizeof(OtaAckPayload) + 1> payload{};
    OtaStatus status = OtaStatus::kOk;
    uint32_t nextOffset = 0;

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaAck(payload.data(), payload.size(), &status, &nextOffset));

    payload[0] = 0xFF;
    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaAck(payload.data(), sizeof(OtaAckPayload), &status, &nextOffset));
    size_t len = 0;
    EXPECT_EQ(TransportError::kInvalidArg, serializeOtaAck(static_cast<OtaStatus>(0xFF), 0,
                                                           payload.data(), payload.size(), &len));
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

TEST(OtaAbort, RejectsTrailingBytesAndUnknownStatus) {
    std::array<uint8_t, sizeof(OtaAbortPayload) + 1> payload{};
    OtaStatus reason = OtaStatus::kOk;

    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaAbort(payload.data(), payload.size(), &reason));
    payload[0] = 0xFF;
    EXPECT_EQ(TransportError::kProtocolError,
              deserializeOtaAbort(payload.data(), sizeof(OtaAbortPayload), &reason));
    size_t len = 0;
    EXPECT_EQ(TransportError::kInvalidArg, serializeOtaAbort(static_cast<OtaStatus>(0xFF),
                                                             payload.data(), payload.size(), &len));
}

// =============================================================================
// Null Argument Tests
// =============================================================================

TEST(OtaSerializer, RejectsNullBuffer) {
    std::array<uint8_t, kSha256Size> sha256{};
    size_t len = 0;

    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaBegin(0, sha256.data(), nullptr, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg, serializeOtaData(0, nullptr, 0, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg, serializeOtaAck(OtaStatus::kOk, 0, nullptr, 0, &len));
    EXPECT_EQ(TransportError::kInvalidArg, serializeOtaAbort(OtaStatus::kOk, nullptr, 0, &len));
}

TEST(OtaSerializer, RejectsNullOutLen) {
    std::array<uint8_t, 128> buf{};
    std::array<uint8_t, kSha256Size> sha256{};

    EXPECT_EQ(TransportError::kInvalidArg,
              serializeOtaBegin(0, sha256.data(), nullptr, buf.data(), buf.size(), nullptr));
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
