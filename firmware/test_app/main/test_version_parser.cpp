/**
 * @file test_version_parser.cpp
 * @brief Unit tests for firmware version parsing
 */

#include "services/firmwareVersion.hpp"

#include <gtest/gtest.h>

using namespace domes;

// =============================================================================
// parseVersion Tests
// =============================================================================

TEST(ParseVersion, HandlesSimpleVersion) {
    FirmwareVersion v = parseVersion("v1.2.3");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
    EXPECT_FALSE(v.dirty);
    EXPECT_TRUE(v.valid);
    EXPECT_STREQ("", v.gitHash);
}

TEST(ParseVersion, HandlesVersionWithoutVPrefix) {
    FirmwareVersion v = parseVersion("1.2.3");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
}

TEST(ParseVersion, HandlesDirtyFlag) {
    FirmwareVersion v = parseVersion("v1.2.3-dirty");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
    EXPECT_TRUE(v.dirty);
}

TEST(ParseVersion, HandlesGitDescribeOutput) {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
    EXPECT_FALSE(v.dirty);
    EXPECT_STREQ("a1b2c3d", v.gitHash);
}

TEST(ParseVersion, HandlesHardwareRollbackBuildVersion) {
    FirmwareVersion v = parseVersion("v0.0.0-1-g0123456789ab");

    EXPECT_TRUE(v.valid);
    EXPECT_EQ(0u, v.major);
    EXPECT_EQ(0u, v.minor);
    EXPECT_EQ(0u, v.patch);
    EXPECT_FALSE(v.dirty);
    EXPECT_STREQ("0123456789ab", v.gitHash);
}

TEST(ParseVersion, HandlesGitDescribeWithDirty) {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d-dirty");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
    EXPECT_TRUE(v.dirty);
    EXPECT_STREQ("a1b2c3d", v.gitHash);
}

TEST(ParseVersion, HandlesFullLengthGitHash) {
    FirmwareVersion v = parseVersion("v1.2.3-5-g0123456789abcdef0123456789abcdef01234567");

    EXPECT_TRUE(v.valid);
    EXPECT_STREQ("0123456789abcdef0123456789abcdef01234567", v.gitHash);
}

TEST(ParseVersion, HandlesZeroVersion) {
    FirmwareVersion v = parseVersion("v0.0.0");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
}

TEST(ParseVersion, HandlesLargeVersionNumbers) {
    FirmwareVersion v = parseVersion("v4294967295.65536.256");

    EXPECT_EQ(UINT32_MAX, v.major);
    EXPECT_EQ(65536u, v.minor);
    EXPECT_EQ(256u, v.patch);
    EXPECT_TRUE(v.valid);
}

TEST(ParseVersion, HandlesNullInput) {
    FirmwareVersion v = parseVersion(nullptr);

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
    EXPECT_FALSE(v.valid);
}

TEST(ParseVersion, HandlesEmptyString) {
    FirmwareVersion v = parseVersion("");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
    EXPECT_FALSE(v.valid);
}

TEST(ParseVersion, HandlesInvalidFormat) {
    FirmwareVersion v = parseVersion("not-a-version");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
    EXPECT_FALSE(v.valid);
}

TEST(ParseVersion, RejectsOverflowAndMalformedSuffixes) {
    EXPECT_FALSE(parseVersion("v4294967296.0.0").valid);
    EXPECT_FALSE(parseVersion("v1.2").valid);
    EXPECT_FALSE(parseVersion("v1.2.3junk").valid);
    EXPECT_FALSE(parseVersion("v1.2.3-5-gxyz").valid);
    EXPECT_FALSE(parseVersion("v1.2.3-5-gabcdef-extra").valid);
    EXPECT_FALSE(parseVersion("v1.2.3-5-g0123456789abcdef0123456789abcdef012345678").valid);
}

TEST(FirmwareVersionIntegrity, RequiresExactParserValidMatch) {
    EXPECT_TRUE(firmwareVersionsMatchExactly("v1.2.3-4-g0123456789ab", "v1.2.3-4-g0123456789ab"));
    EXPECT_FALSE(firmwareVersionsMatchExactly("v1.2.3", "1.2.3"));
    EXPECT_FALSE(firmwareVersionsMatchExactly("v1.2.3", "v1.2.4"));
    EXPECT_FALSE(firmwareVersionsMatchExactly("invalid", "invalid"));
    EXPECT_FALSE(firmwareVersionsMatchExactly(nullptr, "v1.2.3"));
}

// =============================================================================
// Version Comparison Tests
// =============================================================================

TEST(FirmwareVersionCompare, EqualVersions) {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.3");

    EXPECT_EQ(0, v1.compare(v2));
}

TEST(FirmwareVersionCompare, MajorDifference) {
    FirmwareVersion v1 = parseVersion("v1.0.0");
    FirmwareVersion v2 = parseVersion("v2.0.0");

    EXPECT_LT(v1.compare(v2), 0);
    EXPECT_GT(v2.compare(v1), 0);
}

TEST(FirmwareVersionCompare, MinorDifference) {
    FirmwareVersion v1 = parseVersion("v1.2.0");
    FirmwareVersion v2 = parseVersion("v1.3.0");

    EXPECT_LT(v1.compare(v2), 0);
    EXPECT_GT(v2.compare(v1), 0);
}

TEST(FirmwareVersionCompare, PatchDifference) {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.4");

    EXPECT_LT(v1.compare(v2), 0);
    EXPECT_GT(v2.compare(v1), 0);
}

TEST(FirmwareVersionCompare, IsUpdateAvailable) {
    FirmwareVersion current = parseVersion("v1.0.0");
    FirmwareVersion newer = parseVersion("v1.0.1");
    FirmwareVersion older = parseVersion("v0.9.9");
    FirmwareVersion same = parseVersion("v1.0.0");

    EXPECT_TRUE(current.isUpdateAvailable(newer));
    EXPECT_FALSE(current.isUpdateAvailable(older));
    EXPECT_FALSE(current.isUpdateAvailable(same));
}

TEST(FirmwareVersionCompare, IgnoresDirtyFlag) {
    FirmwareVersion clean = parseVersion("v1.0.0");
    FirmwareVersion dirty = parseVersion("v1.0.0-dirty");

    EXPECT_EQ(0, clean.compare(dirty));
}

TEST(FirmwareVersionCompare, IgnoresGitHash) {
    FirmwareVersion v1 = parseVersion("v1.0.0-5-ga1b2c3d");
    FirmwareVersion v2 = parseVersion("v1.0.0-10-gd9e8f7a");

    EXPECT_EQ(0, v1.compare(v2));
}

TEST(FirmwareVersionCompare, InvalidVersionsNeverOfferUpdates) {
    FirmwareVersion invalid = parseVersion("not-a-version");
    FirmwareVersion valid = parseVersion("v1.0.0");

    EXPECT_FALSE(invalid.isUpdateAvailable(valid));
    EXPECT_FALSE(valid.isUpdateAvailable(invalid));
    EXPECT_LT(invalid.compare(valid), 0);
}
