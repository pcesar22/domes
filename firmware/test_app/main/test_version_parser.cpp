/**
 * @file test_version_parser.cpp
 * @brief Unit tests for firmware version parsing
 */

#include "unity.h"

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

extern "C" void test_parseVersion_handles_simple_version(void) {
    FirmwareVersion v = parseVersion("v1.2.3");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_FALSE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("", v.gitHash);
}

extern "C" void test_parseVersion_handles_version_without_v_prefix(void) {
    FirmwareVersion v = parseVersion("1.2.3");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
}

extern "C" void test_parseVersion_handles_dirty_flag(void) {
    FirmwareVersion v = parseVersion("v1.2.3-dirty");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_TRUE(v.dirty);
}

extern "C" void test_parseVersion_handles_git_describe_output(void) {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_FALSE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d", v.gitHash);
}

extern "C" void test_parseVersion_handles_git_describe_with_dirty(void) {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d-dirty");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_TRUE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d", v.gitHash);
}

extern "C" void test_parseVersion_handles_zero_version(void) {
    FirmwareVersion v = parseVersion("v0.0.0");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

extern "C" void test_parseVersion_handles_large_version_numbers(void) {
    FirmwareVersion v = parseVersion("v255.255.255");

    TEST_ASSERT_EQUAL_UINT8(255, v.major);
    TEST_ASSERT_EQUAL_UINT8(255, v.minor);
    TEST_ASSERT_EQUAL_UINT8(255, v.patch);
}

extern "C" void test_parseVersion_handles_null_input(void) {
    FirmwareVersion v = parseVersion(nullptr);

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

extern "C" void test_parseVersion_handles_empty_string(void) {
    FirmwareVersion v = parseVersion("");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

extern "C" void test_parseVersion_handles_invalid_format(void) {
    FirmwareVersion v = parseVersion("not-a-version");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

// =============================================================================
// Version Comparison Tests
// =============================================================================

extern "C" void test_FirmwareVersion_compare_equal_versions(void) {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.3");

    TEST_ASSERT_EQUAL_INT(0, v1.compare(v2));
}

extern "C" void test_FirmwareVersion_compare_major_difference(void) {
    FirmwareVersion v1 = parseVersion("v1.0.0");
    FirmwareVersion v2 = parseVersion("v2.0.0");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

extern "C" void test_FirmwareVersion_compare_minor_difference(void) {
    FirmwareVersion v1 = parseVersion("v1.2.0");
    FirmwareVersion v2 = parseVersion("v1.3.0");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

extern "C" void test_FirmwareVersion_compare_patch_difference(void) {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.4");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

extern "C" void test_FirmwareVersion_isUpdateAvailable(void) {
    FirmwareVersion current = parseVersion("v1.0.0");
    FirmwareVersion newer = parseVersion("v1.0.1");
    FirmwareVersion older = parseVersion("v0.9.9");
    FirmwareVersion same = parseVersion("v1.0.0");

    TEST_ASSERT_TRUE(current.isUpdateAvailable(newer));
    TEST_ASSERT_FALSE(current.isUpdateAvailable(older));
    TEST_ASSERT_FALSE(current.isUpdateAvailable(same));
}

extern "C" void test_FirmwareVersion_compare_ignores_dirty_flag(void) {
    FirmwareVersion clean = parseVersion("v1.0.0");
    FirmwareVersion dirty = parseVersion("v1.0.0-dirty");

    TEST_ASSERT_EQUAL_INT(0, clean.compare(dirty));
}

extern "C" void test_FirmwareVersion_compare_ignores_git_hash(void) {
    FirmwareVersion v1 = parseVersion("v1.0.0-5-ga1b2c3d");
    FirmwareVersion v2 = parseVersion("v1.0.0-10-gx9y8z7w");

    TEST_ASSERT_EQUAL_INT(0, v1.compare(v2));
}
