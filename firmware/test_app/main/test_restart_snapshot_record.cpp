/**
 * @file test_restart_snapshot_record.cpp
 * @brief Unit tests for clean-restart snapshot integrity semantics
 */

#include "infra/restartSnapshotRecord.hpp"

#include <cstring>

#include <gtest/gtest.h>

namespace {

domes::infra::CrashDumpData makeRecord() {
    domes::infra::CrashDumpData record;
    record.valid = true;
    record.formatVersion = domes::infra::kRestartSnapshotFormatVersion;
    std::strncpy(record.reason, "shutdown/restart", sizeof(record.reason) - 1);
    std::strncpy(record.taskName, "serial_ota", sizeof(record.taskName) - 1);
    record.uptimeS = 42;
    record.freeHeap = 49152;
    record.backtrace[0] = 0x42001234;
    record.backtrace[1] = 0x42005678;
    record.backtraceDepth = 2;
    record.bootCount = 7;
    std::strncpy(record.firmwareVersion, "v1.2.3", sizeof(record.firmwareVersion) - 1);
    std::memset(record.elfSha256, 'a', sizeof(record.elfSha256) - 1);
    record.recordCrc = domes::infra::calculateRestartSnapshotCrc(record);
    return record;
}

void expectCrcMismatch(domes::infra::CrashDumpData record) {
    EXPECT_FALSE(domes::infra::restartSnapshotCrcMatches(record));
}

}  // namespace

TEST(RestartSnapshotRecord, AcceptsUnchangedCurrentRecord) {
    const auto record = makeRecord();
    EXPECT_TRUE(domes::infra::isSupportedRestartSnapshotFormat(record.formatVersion));
    EXPECT_TRUE(domes::infra::restartSnapshotCrcMatches(record));
}

TEST(RestartSnapshotRecord, RejectsEveryPersistedFieldClassAfterCorruption) {
    auto record = makeRecord();
    record.formatVersion ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.reason[0] ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.taskName[0] ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.uptimeS ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.freeHeap ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.backtrace[0] ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.backtraceDepth ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.bootCount ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.firmwareVersion[0] ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.elfSha256[0] ^= 1;
    expectCrcMismatch(record);

    record = makeRecord();
    record.recordCrc ^= 1;
    expectCrcMismatch(record);
}

TEST(RestartSnapshotRecord, RejectsUnsupportedFormats) {
    EXPECT_FALSE(domes::infra::isSupportedRestartSnapshotFormat(0));
    EXPECT_FALSE(domes::infra::isSupportedRestartSnapshotFormat(1));
    EXPECT_TRUE(domes::infra::isSupportedRestartSnapshotFormat(2));
    EXPECT_FALSE(domes::infra::isSupportedRestartSnapshotFormat(3));
}

TEST(RestartSnapshotRecord, LegacySanitizationRemovesUntrustedIdentityMetadata) {
    auto record = makeRecord();
    domes::infra::sanitizeLegacyRestartSnapshot(record);

    EXPECT_TRUE(record.valid);
    EXPECT_EQ(record.formatVersion, 0);
    EXPECT_EQ(record.recordCrc, 0u);
    EXPECT_EQ(record.firmwareVersion[0], '\0');
    EXPECT_EQ(record.elfSha256[0], '\0');
    EXPECT_STREQ(record.reason, "shutdown/restart");
    EXPECT_STREQ(record.taskName, "serial_ota");
}
