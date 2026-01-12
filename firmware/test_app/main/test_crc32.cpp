/**
 * @file test_crc32.cpp
 * @brief Unit tests for CRC32 calculation
 *
 * Tests the CRC32 implementation against known test vectors from
 * IEEE 802.3 / ZIP specification (polynomial 0xEDB88320, reflected).
 */

#include <gtest/gtest.h>
#include "utils/crc32.hpp"

#include <cstring>

using namespace domes;

// =============================================================================
// Known Test Vectors
// =============================================================================

TEST(Crc32, EmptyBufferReturns0x00000000) {
    uint32_t crc = crc32(nullptr, 0);
    EXPECT_EQ(0x00000000u, crc);
}

TEST(Crc32, Of123456789MatchesIEEE802_3CheckValue) {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint32_t crc = crc32(data, sizeof(data));
    EXPECT_EQ(0xCBF43926u, crc);
}

TEST(Crc32, SingleByte0x00) {
    uint8_t data[] = {0x00};
    uint32_t crc = crc32(data, 1);
    EXPECT_EQ(0xD202EF8Du, crc);
}

TEST(Crc32, SingleByte0xFF) {
    uint8_t data[] = {0xFF};
    uint32_t crc = crc32(data, 1);
    EXPECT_EQ(0xFF000000u, crc);
}

TEST(Crc32, AllZeros4Bytes) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint32_t crc = crc32(data, sizeof(data));
    EXPECT_EQ(0x2144DF1Cu, crc);
}

TEST(Crc32, AllOnes4Bytes) {
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t crc = crc32(data, sizeof(data));
    EXPECT_EQ(0xFFFFFFFFu, crc);
}

// =============================================================================
// Incremental Calculation
// =============================================================================

TEST(Crc32, IncrementalMatchesSingleShot) {
    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};

    uint32_t singleShot = crc32(data, sizeof(data));

    uint32_t running = kCrc32Init;
    running = crc32Update(data, 5, running);
    running = crc32Update(data + 5, 6, running);
    uint32_t incremental = crc32Finalize(running);

    EXPECT_EQ(singleShot, incremental);
}

TEST(Crc32, IncrementalByteByByteMatchesSingleShot) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    uint32_t singleShot = crc32(data, sizeof(data));

    uint32_t running = kCrc32Init;
    for (size_t i = 0; i < sizeof(data); ++i) {
        running = crc32Update(&data[i], 1, running);
    }
    uint32_t incremental = crc32Finalize(running);

    EXPECT_EQ(singleShot, incremental);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(Crc32, DifferentDataProducesDifferentCRC) {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};

    uint32_t crc1 = crc32(data1, sizeof(data1));
    uint32_t crc2 = crc32(data2, sizeof(data2));

    EXPECT_NE(crc1, crc2);
}

TEST(Crc32, DetectsSingleBitFlip) {
    uint8_t data1[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t data2[] = {0x01, 0x00, 0x00, 0x00};

    uint32_t crc1 = crc32(data1, sizeof(data1));
    uint32_t crc2 = crc32(data2, sizeof(data2));

    EXPECT_NE(crc1, crc2);
}

TEST(Crc32, LargeBuffer) {
    uint8_t data[1024];
    for (size_t i = 0; i < sizeof(data); ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    uint32_t crc = crc32(data, sizeof(data));
    EXPECT_NE(0u, crc);

    uint32_t crc2 = crc32(data, sizeof(data));
    EXPECT_EQ(crc, crc2);
}
