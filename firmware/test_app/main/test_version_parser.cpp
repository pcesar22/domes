/**
 * @file test_version_parser.cpp
 * @brief Unit tests for firmware version parsing
 */

#include <gtest/gtest.h>

#include <cstring>
#include <cstdint>
#include <cstdlib>

namespace domes {

struct FirmwareVersion {
    uint8_t major = 0;
    uint8_t minor = 0;
    uint8_t patch = 0;
    bool dirty = false;
    char gitHash[16] = {0};

    int compare(const FirmwareVersion& other) const {
        if (major != other.major) return major - other.major;
        if (minor != other.minor) return minor - other.minor;
        return patch - other.patch;
    }

    bool isUpdateAvailable(const FirmwareVersion& remote) const {
        return compare(remote) < 0;
    }
};

FirmwareVersion parseVersion(const char* versionStr) {
    FirmwareVersion v;

    if (versionStr == nullptr || versionStr[0] == '\0') {
        return v;
    }

    const char* p = versionStr;

    if (*p == 'v' || *p == 'V') {
        p++;
    }

    char* end = nullptr;
    long val = strtol(p, &end, 10);
    if (end == p || *end != '.') {
        return v;
    }
    v.major = static_cast<uint8_t>(val);
    p = end + 1;

    val = strtol(p, &end, 10);
    if (end == p || *end != '.') {
        return v;
    }
    v.minor = static_cast<uint8_t>(val);
    p = end + 1;

    val = strtol(p, &end, 10);
    if (end == p) {
        return v;
    }
    v.patch = static_cast<uint8_t>(val);
    p = end;

    if (*p == '-') {
        p++;
        if (strncmp(p, "dirty", 5) == 0) {
            v.dirty = true;
        } else {
            while (*p && *p != '-' && *p != 'g') p++;
            if (*p == '-') p++;
            if (*p == 'g') {
                p++;
                size_t i = 0;
                while (*p && *p != '-' && i < sizeof(v.gitHash) - 1) {
                    v.gitHash[i++] = *p++;
                }
                v.gitHash[i] = '\0';
            }
            if (*p == '-' && strncmp(p + 1, "dirty", 5) == 0) {
                v.dirty = true;
            }
        }
    }

    return v;
}

}  // namespace domes

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

TEST(ParseVersion, HandlesGitDescribeWithDirty) {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d-dirty");

    EXPECT_EQ(1, v.major);
    EXPECT_EQ(2, v.minor);
    EXPECT_EQ(3, v.patch);
    EXPECT_TRUE(v.dirty);
    EXPECT_STREQ("a1b2c3d", v.gitHash);
}

TEST(ParseVersion, HandlesZeroVersion) {
    FirmwareVersion v = parseVersion("v0.0.0");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
}

TEST(ParseVersion, HandlesLargeVersionNumbers) {
    FirmwareVersion v = parseVersion("v255.255.255");

    EXPECT_EQ(255, v.major);
    EXPECT_EQ(255, v.minor);
    EXPECT_EQ(255, v.patch);
}

TEST(ParseVersion, HandlesNullInput) {
    FirmwareVersion v = parseVersion(nullptr);

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
}

TEST(ParseVersion, HandlesEmptyString) {
    FirmwareVersion v = parseVersion("");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
}

TEST(ParseVersion, HandlesInvalidFormat) {
    FirmwareVersion v = parseVersion("not-a-version");

    EXPECT_EQ(0, v.major);
    EXPECT_EQ(0, v.minor);
    EXPECT_EQ(0, v.patch);
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
    FirmwareVersion v2 = parseVersion("v1.0.0-10-gx9y8z7w");

    EXPECT_EQ(0, v1.compare(v2));
}
