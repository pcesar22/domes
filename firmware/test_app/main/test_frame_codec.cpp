/**
 * @file test_frame_codec.cpp
 * @brief Unit tests for frame encoding/decoding
 */

#include <gtest/gtest.h>
#include "protocol/frameCodec.hpp"

#include <array>
#include <cstring>

using namespace domes;

// =============================================================================
// encodeFrame Tests
// =============================================================================

TEST(EncodeFrame, MinimalFrameNoPayload) {
    std::array<uint8_t, 64> buf{};
    size_t frameLen = 0;

    TransportError err = encodeFrame(0x01, nullptr, 0, buf.data(), buf.size(), &frameLen);

    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(9u, frameLen);
    EXPECT_EQ(kFrameStartByte0, buf[0]);
    EXPECT_EQ(kFrameStartByte1, buf[1]);
    EXPECT_EQ(0x01, buf[2]);
    EXPECT_EQ(0x00, buf[3]);
    EXPECT_EQ(0x01, buf[4]);
}

TEST(EncodeFrame, WithPayload) {
    std::array<uint8_t, 64> buf{};
    size_t frameLen = 0;
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    TransportError err = encodeFrame(0x42, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(13u, frameLen);
    EXPECT_EQ(0x05, buf[2]);
    EXPECT_EQ(0x42, buf[4]);
    EXPECT_EQ(0xDE, buf[5]);
    EXPECT_EQ(0xAD, buf[6]);
    EXPECT_EQ(0xBE, buf[7]);
    EXPECT_EQ(0xEF, buf[8]);
}

TEST(EncodeFrame, BufferTooSmallReturnsError) {
    std::array<uint8_t, 8> smallBuf{};
    size_t frameLen = 0;

    TransportError err = encodeFrame(0x01, nullptr, 0, smallBuf.data(), smallBuf.size(), &frameLen);

    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(EncodeFrame, PayloadTooLargeReturnsError) {
    std::array<uint8_t, 2048> buf{};
    size_t frameLen = 0;
    uint8_t payload[kMaxPayloadSize + 1] = {0};

    TransportError err = encodeFrame(0x01, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    EXPECT_EQ(TransportError::kInvalidArg, err);
}

TEST(EncodeFrame, MaxPayloadSizeSucceeds) {
    std::array<uint8_t, kMaxFrameSize> buf{};
    size_t frameLen = 0;
    uint8_t payload[kMaxPayloadSize];
    std::memset(payload, 0xAA, sizeof(payload));

    TransportError err = encodeFrame(0x01, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    EXPECT_EQ(TransportError::kOk, err);
    EXPECT_EQ(kMaxFrameSize, frameLen);
}

// =============================================================================
// FrameDecoder State Machine Tests
// =============================================================================

TEST(FrameDecoder, InitialStateIsWaitStart0) {
    FrameDecoder decoder;

    EXPECT_EQ(FrameDecoder::State::kWaitStart0, decoder.getState());
    EXPECT_FALSE(decoder.isComplete());
    EXPECT_FALSE(decoder.isError());
}

TEST(FrameDecoder, DetectsStartBytes) {
    FrameDecoder decoder;

    FrameDecoder::State state = decoder.feedByte(kFrameStartByte0);
    EXPECT_EQ(FrameDecoder::State::kWaitStart1, state);

    state = decoder.feedByte(kFrameStartByte1);
    EXPECT_EQ(FrameDecoder::State::kWaitLenLow, state);
}

TEST(FrameDecoder, Handles0xAA0xAA0x55Sequence) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    EXPECT_EQ(FrameDecoder::State::kWaitStart1, decoder.getState());

    decoder.feedByte(0xAA);
    EXPECT_EQ(FrameDecoder::State::kWaitStart1, decoder.getState());

    decoder.feedByte(0x55);
    EXPECT_EQ(FrameDecoder::State::kWaitLenLow, decoder.getState());
}

TEST(FrameDecoder, RejectsNonStartByteAfter0xAA) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x00);
    EXPECT_EQ(FrameDecoder::State::kWaitStart0, decoder.getState());
}

TEST(FrameDecoder, RejectsZeroLength) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    decoder.feedByte(0x00);
    decoder.feedByte(0x00);

    EXPECT_EQ(FrameDecoder::State::kError, decoder.getState());
    EXPECT_TRUE(decoder.isError());
}

TEST(FrameDecoder, RejectsOversizedLength) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    uint16_t badLen = kMaxPayloadSize + 2;
    decoder.feedByte(static_cast<uint8_t>(badLen & 0xFF));
    decoder.feedByte(static_cast<uint8_t>(badLen >> 8));

    EXPECT_EQ(FrameDecoder::State::kError, decoder.getState());
}

TEST(FrameDecoder, ResetClearsState) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    decoder.feedByte(0x05);

    decoder.reset();

    EXPECT_EQ(FrameDecoder::State::kWaitStart0, decoder.getState());
    EXPECT_FALSE(decoder.isComplete());
    EXPECT_FALSE(decoder.isError());
}

// =============================================================================
// Round-Trip Tests
// =============================================================================

TEST(FrameRoundTrip, EncodeThenDecode) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t type = 0x07;

    TransportError err = encodeFrame(type, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    EXPECT_EQ(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isComplete());
    EXPECT_EQ(type, decoder.getType());
    EXPECT_EQ(sizeof(payload), decoder.getPayloadLen());

    const uint8_t* decoded = decoder.getPayload();
    EXPECT_EQ(0x11, decoded[0]);
    EXPECT_EQ(0x22, decoded[1]);
    EXPECT_EQ(0x33, decoded[2]);
    EXPECT_EQ(0x44, decoded[3]);
}

TEST(FrameRoundTrip, NoPayload) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;
    uint8_t type = 0xFF;

    TransportError err = encodeFrame(type, nullptr, 0, frameBuf.data(), frameBuf.size(), &frameLen);
    EXPECT_EQ(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isComplete());
    EXPECT_EQ(type, decoder.getType());
    EXPECT_EQ(0u, decoder.getPayloadLen());
}

TEST(FrameRoundTrip, MaxPayload) {
    std::array<uint8_t, kMaxFrameSize> frameBuf{};
    size_t frameLen = 0;

    std::array<uint8_t, kMaxPayloadSize> payload;
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    TransportError err = encodeFrame(0x01, payload.data(), payload.size(), frameBuf.data(), frameBuf.size(), &frameLen);
    EXPECT_EQ(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isComplete());
    EXPECT_EQ(kMaxPayloadSize, decoder.getPayloadLen());

    const uint8_t* decoded = decoder.getPayload();
    for (size_t i = 0; i < kMaxPayloadSize; ++i) {
        EXPECT_EQ(payload[i], decoded[i]);
    }
}

// =============================================================================
// CRC Validation Tests
// =============================================================================

TEST(FrameDecoder, DetectsCRCMismatch) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;
    uint8_t payload[] = {0xAA, 0xBB};

    encodeFrame(0x01, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    frameBuf[frameLen - 1] ^= 0xFF;

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isError());
    EXPECT_FALSE(decoder.isComplete());
}

TEST(FrameDecoder, DetectsPayloadCorruption) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

    encodeFrame(0x01, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    frameBuf[6] ^= 0x01;

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isError());
}

// =============================================================================
// Noise Resilience Tests
// =============================================================================

TEST(FrameDecoder, HandlesGarbageBeforeSync) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;
    uint8_t payload[] = {0x42};

    encodeFrame(0x01, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);

    FrameDecoder decoder;

    decoder.feedByte(0x00);
    decoder.feedByte(0x12);
    decoder.feedByte(0x34);
    decoder.feedByte(0xFF);

    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isComplete());
    EXPECT_EQ(0x01, decoder.getType());
}

TEST(FrameDecoder, RequiresResetAfterComplete) {
    std::array<uint8_t, 64> frameBuf{};
    size_t frameLen = 0;

    encodeFrame(0x01, nullptr, 0, frameBuf.data(), frameBuf.size(), &frameLen);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    EXPECT_TRUE(decoder.isComplete());

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    EXPECT_TRUE(decoder.isComplete());
}
