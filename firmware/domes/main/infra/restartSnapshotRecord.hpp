#pragma once

/**
 * @file restartSnapshotRecord.hpp
 * @brief Deterministic clean-restart snapshot record and integrity helpers
 */

#include "utils/crc32.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace domes::infra {

/// Maximum backtrace depth stored in a restart snapshot.
constexpr size_t kMaxBacktraceDepth = 16;

/// Current integrity-checked restart-snapshot format.
constexpr uint8_t kRestartSnapshotFormatVersion = 2;

/**
 * @brief Stored clean-restart snapshot data
 */
struct CrashDumpData {
    bool valid = false;
    char reason[64] = {};
    char taskName[16] = {};
    uint32_t uptimeS = 0;
    uint32_t freeHeap = 0;
    uint32_t backtrace[kMaxBacktraceDepth] = {};
    uint8_t backtraceDepth = 0;
    uint32_t bootCount = 0;
    char firmwareVersion[32] = {};
    char elfSha256[65] = {};
    uint8_t formatVersion = 0;
    uint32_t recordCrc = 0;
};

inline void updateRestartSnapshotCrc(uint32_t& crc, const void* data, size_t size) {
    crc = domes::crc32Update(static_cast<const uint8_t*>(data), size, crc);
}

/**
 * @brief Calculate the deterministic CRC for every persisted format-2 field
 */
inline uint32_t calculateRestartSnapshotCrc(const CrashDumpData& dump) {
    uint32_t crc = domes::kCrc32Init;
    updateRestartSnapshotCrc(crc, &dump.formatVersion, sizeof(dump.formatVersion));
    updateRestartSnapshotCrc(crc, dump.reason, sizeof(dump.reason));
    updateRestartSnapshotCrc(crc, dump.taskName, sizeof(dump.taskName));
    updateRestartSnapshotCrc(crc, &dump.uptimeS, sizeof(dump.uptimeS));
    updateRestartSnapshotCrc(crc, &dump.freeHeap, sizeof(dump.freeHeap));
    updateRestartSnapshotCrc(crc, dump.backtrace, sizeof(dump.backtrace));
    updateRestartSnapshotCrc(crc, &dump.backtraceDepth, sizeof(dump.backtraceDepth));
    updateRestartSnapshotCrc(crc, &dump.bootCount, sizeof(dump.bootCount));
    updateRestartSnapshotCrc(crc, dump.firmwareVersion, sizeof(dump.firmwareVersion));
    updateRestartSnapshotCrc(crc, dump.elfSha256, sizeof(dump.elfSha256));
    return domes::crc32Finalize(crc);
}

inline bool isSupportedRestartSnapshotFormat(uint8_t formatVersion) {
    return formatVersion == kRestartSnapshotFormatVersion;
}

inline bool restartSnapshotCrcMatches(const CrashDumpData& dump) {
    return calculateRestartSnapshotCrc(dump) == dump.recordCrc;
}

/**
 * @brief Remove metadata whose semantics were not defined by legacy records
 */
inline void sanitizeLegacyRestartSnapshot(CrashDumpData& dump) {
    dump.formatVersion = 0;
    std::memset(dump.firmwareVersion, 0, sizeof(dump.firmwareVersion));
    std::memset(dump.elfSha256, 0, sizeof(dump.elfSha256));
    dump.recordCrc = 0;
    dump.valid = true;
}

}  // namespace domes::infra
