/**
 * @file test_frame_codec.cpp
 * @brief Unit tests for frame encoding/decoding
 */

#include "unity.h"
#include "protocol/frameCodec.hpp"

#include <array>
#include <cstring>

using namespace domes;

// =============================================================================
// encodeFrame Tests
// =============================================================================

extern "C" void test_encodeFrame_minimal_frame_no_payload(void) {
    std::array<uint8_t, 64> buf;
    size_t frameLen = 0;

    TransportError err = encodeFrame(0x01, nullptr, 0, buf.data(), buf.size(), &frameLen);

    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(9, frameLen);
    TEST_ASSERT_EQUAL_HEX8(kFrameStartByte0, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(kFrameStartByte1, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);
}

extern "C" void test_encodeFrame_with_payload(void) {
    std::array<uint8_t, 64> buf;
    size_t frameLen = 0;
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    TransportError err = encodeFrame(0x42, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(13, frameLen);
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x42, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0xDE, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, buf[7]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, buf[8]);
}

extern "C" void test_encodeFrame_buffer_too_small_returns_error(void) {
    std::array<uint8_t, 8> smallBuf;
    size_t frameLen = 0;

    TransportError err = encodeFrame(0x01, nullptr, 0, smallBuf.data(), smallBuf.size(), &frameLen);

    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

extern "C" void test_encodeFrame_payload_too_large_returns_error(void) {
    std::array<uint8_t, 2048> buf;
    size_t frameLen = 0;
    uint8_t payload[kMaxPayloadSize + 1] = {0};

    TransportError err = encodeFrame(0x01, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    TEST_ASSERT_EQUAL(TransportError::kInvalidArg, err);
}

extern "C" void test_encodeFrame_max_payload_size_succeeds(void) {
    std::array<uint8_t, kMaxFrameSize> buf;
    size_t frameLen = 0;
    uint8_t payload[kMaxPayloadSize];
    std::memset(payload, 0xAA, sizeof(payload));

    TransportError err = encodeFrame(0x01, payload, sizeof(payload), buf.data(), buf.size(), &frameLen);

    TEST_ASSERT_EQUAL(TransportError::kOk, err);
    TEST_ASSERT_EQUAL(kMaxFrameSize, frameLen);
}

// =============================================================================
// FrameDecoder State Machine Tests
// =============================================================================

extern "C" void test_FrameDecoder_initial_state_is_kWaitStart0(void) {
    FrameDecoder decoder;

    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart0, decoder.getState());
    TEST_ASSERT_FALSE(decoder.isComplete());
    TEST_ASSERT_FALSE(decoder.isError());
}

extern "C" void test_FrameDecoder_detects_start_bytes(void) {
    FrameDecoder decoder;

    FrameDecoder::State state = decoder.feedByte(kFrameStartByte0);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart1, state);

    state = decoder.feedByte(kFrameStartByte1);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitLenLow, state);
}

extern "C" void test_FrameDecoder_handles_0xAA_0xAA_0x55_sequence(void) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart1, decoder.getState());

    decoder.feedByte(0xAA);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart1, decoder.getState());

    decoder.feedByte(0x55);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitLenLow, decoder.getState());
}

extern "C" void test_FrameDecoder_rejects_non_start_byte_after_0xAA(void) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x00);
    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart0, decoder.getState());
}

extern "C" void test_FrameDecoder_rejects_zero_length(void) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    decoder.feedByte(0x00);
    decoder.feedByte(0x00);

    TEST_ASSERT_EQUAL(FrameDecoder::State::kError, decoder.getState());
    TEST_ASSERT_TRUE(decoder.isError());
}

extern "C" void test_FrameDecoder_rejects_oversized_length(void) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    uint16_t badLen = kMaxPayloadSize + 2;
    decoder.feedByte(static_cast<uint8_t>(badLen & 0xFF));
    decoder.feedByte(static_cast<uint8_t>(badLen >> 8));

    TEST_ASSERT_EQUAL(FrameDecoder::State::kError, decoder.getState());
}

extern "C" void test_FrameDecoder_reset_clears_state(void) {
    FrameDecoder decoder;

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    decoder.feedByte(0x05);

    decoder.reset();

    TEST_ASSERT_EQUAL(FrameDecoder::State::kWaitStart0, decoder.getState());
    TEST_ASSERT_FALSE(decoder.isComplete());
    TEST_ASSERT_FALSE(decoder.isError());
}

// =============================================================================
// Round-Trip Tests
// =============================================================================

extern "C" void test_Frame_round_trip_encode_then_decode(void) {
    std::array<uint8_t, 64> frameBuf;
    size_t frameLen = 0;
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t type = 0x07;

    TransportError err = encodeFrame(type, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isComplete());
    TEST_ASSERT_EQUAL(type, decoder.getType());
    TEST_ASSERT_EQUAL(sizeof(payload), decoder.getPayloadLen());

    const uint8_t* decoded = decoder.getPayload();
    TEST_ASSERT_EQUAL_HEX8(0x11, decoded[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22, decoded[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33, decoded[2]);
    TEST_ASSERT_EQUAL_HEX8(0x44, decoded[3]);
}

extern "C" void test_Frame_round_trip_no_payload(void) {
    std::array<uint8_t, 64> frameBuf;
    size_t frameLen = 0;
    uint8_t type = 0xFF;

    TransportError err = encodeFrame(type, nullptr, 0, frameBuf.data(), frameBuf.size(), &frameLen);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isComplete());
    TEST_ASSERT_EQUAL(type, decoder.getType());
    TEST_ASSERT_EQUAL(0, decoder.getPayloadLen());
}

extern "C" void test_Frame_round_trip_max_payload(void) {
    std::array<uint8_t, kMaxFrameSize> frameBuf;
    size_t frameLen = 0;

    std::array<uint8_t, kMaxPayloadSize> payload;
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    TransportError err = encodeFrame(0x01, payload.data(), payload.size(), frameBuf.data(), frameBuf.size(), &frameLen);
    TEST_ASSERT_EQUAL(TransportError::kOk, err);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isComplete());
    TEST_ASSERT_EQUAL(kMaxPayloadSize, decoder.getPayloadLen());

    const uint8_t* decoded = decoder.getPayload();
    for (size_t i = 0; i < kMaxPayloadSize; ++i) {
        TEST_ASSERT_EQUAL_HEX8(payload[i], decoded[i]);
    }
}

// =============================================================================
// CRC Validation Tests
// =============================================================================

extern "C" void test_FrameDecoder_detects_CRC_mismatch(void) {
    std::array<uint8_t, 64> frameBuf;
    size_t frameLen = 0;
    uint8_t payload[] = {0xAA, 0xBB};

    encodeFrame(0x01, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    frameBuf[frameLen - 1] ^= 0xFF;

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isError());
    TEST_ASSERT_FALSE(decoder.isComplete());
}

extern "C" void test_FrameDecoder_detects_payload_corruption(void) {
    std::array<uint8_t, 64> frameBuf;
    size_t frameLen = 0;
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

    encodeFrame(0x01, payload, sizeof(payload), frameBuf.data(), frameBuf.size(), &frameLen);
    frameBuf[6] ^= 0x01;

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isError());
}

// =============================================================================
// Noise Resilience Tests
// =============================================================================

extern "C" void test_FrameDecoder_handles_garbage_before_sync(void) {
    std::array<uint8_t, 64> frameBuf;
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

    TEST_ASSERT_TRUE(decoder.isComplete());
    TEST_ASSERT_EQUAL(0x01, decoder.getType());
}

extern "C" void test_FrameDecoder_requires_reset_after_complete(void) {
    std::array<uint8_t, 64> frameBuf;
    size_t frameLen = 0;

    encodeFrame(0x01, nullptr, 0, frameBuf.data(), frameBuf.size(), &frameLen);

    FrameDecoder decoder;
    for (size_t i = 0; i < frameLen; ++i) {
        decoder.feedByte(frameBuf[i]);
    }

    TEST_ASSERT_TRUE(decoder.isComplete());

    decoder.feedByte(0xAA);
    decoder.feedByte(0x55);
    TEST_ASSERT_TRUE(decoder.isComplete());
}
