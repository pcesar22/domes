/**
 * @file test_crc32.cpp
 * @brief Unit tests for CRC32 calculation
 *
 * Tests the CRC32 implementation against known test vectors from
 * IEEE 802.3 / ZIP specification (polynomial 0xEDB88320, reflected).
 */

#include "unity.h"
#include "utils/crc32.hpp"

#include <cstring>

using namespace domes;

// =============================================================================
// Known Test Vectors
// =============================================================================

extern "C" void test_crc32_empty_buffer_returns_0x00000000(void) {
    uint32_t crc = crc32(nullptr, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

extern "C" void test_crc32_of_123456789_matches_IEEE_802_3_check_value(void) {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint32_t crc = crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

extern "C" void test_crc32_single_byte_0x00(void) {
    uint8_t data[] = {0x00};
    uint32_t crc = crc32(data, 1);
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8D, crc);
}

extern "C" void test_crc32_single_byte_0xFF(void) {
    uint8_t data[] = {0xFF};
    uint32_t crc = crc32(data, 1);
    TEST_ASSERT_EQUAL_HEX32(0xFF000000, crc);
}

extern "C" void test_crc32_all_zeros_4_bytes(void) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint32_t crc = crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX32(0x2144DF1C, crc);
}

extern "C" void test_crc32_all_ones_4_bytes(void) {
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t crc = crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, crc);
}

// =============================================================================
// Incremental Calculation
// =============================================================================

extern "C" void test_crc32_incremental_matches_single_shot(void) {
    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};

    uint32_t singleShot = crc32(data, sizeof(data));

    uint32_t running = kCrc32Init;
    running = crc32Update(data, 5, running);
    running = crc32Update(data + 5, 6, running);
    uint32_t incremental = crc32Finalize(running);

    TEST_ASSERT_EQUAL_HEX32(singleShot, incremental);
}

extern "C" void test_crc32_incremental_byte_by_byte_matches_single_shot(void) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    uint32_t singleShot = crc32(data, sizeof(data));

    uint32_t running = kCrc32Init;
    for (size_t i = 0; i < sizeof(data); ++i) {
        running = crc32Update(&data[i], 1, running);
    }
    uint32_t incremental = crc32Finalize(running);

    TEST_ASSERT_EQUAL_HEX32(singleShot, incremental);
}

// =============================================================================
// Edge Cases
// =============================================================================

extern "C" void test_crc32_different_data_produces_different_CRC(void) {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};

    uint32_t crc1 = crc32(data1, sizeof(data1));
    uint32_t crc2 = crc32(data2, sizeof(data2));

    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

extern "C" void test_crc32_detects_single_bit_flip(void) {
    uint8_t data1[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t data2[] = {0x01, 0x00, 0x00, 0x00};

    uint32_t crc1 = crc32(data1, sizeof(data1));
    uint32_t crc2 = crc32(data2, sizeof(data2));

    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

extern "C" void test_crc32_large_buffer(void) {
    uint8_t data[1024];
    for (size_t i = 0; i < sizeof(data); ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    uint32_t crc = crc32(data, sizeof(data));
    TEST_ASSERT_NOT_EQUAL(0, crc);

    uint32_t crc2 = crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX32(crc, crc2);
}
