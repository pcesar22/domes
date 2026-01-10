#pragma once

/**
 * @file crc32.hpp
 * @brief CRC32 calculation for frame integrity checking
 *
 * Uses the standard CRC-32 polynomial (0xEDB88320, reflected).
 * This is the same algorithm used by Ethernet, ZIP, PNG, etc.
 *
 * Header-only implementation with compile-time lookup table generation.
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace domes {

namespace detail {

/**
 * @brief Generate CRC32 lookup table at compile time
 *
 * Uses the reflected polynomial 0xEDB88320.
 */
constexpr std::array<uint32_t, 256> generateCrc32Table() {
    std::array<uint32_t, 256> table{};
    constexpr uint32_t polynomial = 0xEDB88320;

    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

/// Compile-time CRC32 lookup table
inline constexpr auto kCrc32Table = generateCrc32Table();

}  // namespace detail

/**
 * @brief Calculate CRC32 checksum of a data buffer
 *
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @param initialCrc Starting CRC value (default 0xFFFFFFFF for new calculation)
 * @return CRC32 checksum
 *
 * @example
 * @code
 * uint8_t data[] = {0x01, 0x02, 0x03};
 * uint32_t crc = crc32(data, sizeof(data));
 *
 * // Incremental calculation:
 * uint32_t crc = 0xFFFFFFFF;
 * crc = crc32(part1, len1, crc);
 * crc = crc32(part2, len2, crc);
 * crc ^= 0xFFFFFFFF;  // Finalize
 * @endcode
 */
inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t initialCrc = 0xFFFFFFFF) {
    uint32_t crc = initialCrc;

    for (size_t i = 0; i < len; ++i) {
        uint8_t tableIndex = static_cast<uint8_t>(crc ^ data[i]);
        crc = detail::kCrc32Table[tableIndex] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Calculate CRC32 and return without finalization
 *
 * Use this for incremental CRC calculation. Call crc32Finalize()
 * on the final result.
 *
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @param crc Running CRC value
 * @return Updated CRC value (not finalized)
 */
inline uint32_t crc32Update(const uint8_t* data, size_t len, uint32_t crc) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t tableIndex = static_cast<uint8_t>(crc ^ data[i]);
        crc = detail::kCrc32Table[tableIndex] ^ (crc >> 8);
    }
    return crc;
}

/**
 * @brief Finalize CRC32 calculation
 *
 * @param crc Running CRC value from crc32Update()
 * @return Final CRC32 checksum
 */
inline uint32_t crc32Finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Initial value for incremental CRC32 calculation
 */
constexpr uint32_t kCrc32Init = 0xFFFFFFFF;

}  // namespace domes
