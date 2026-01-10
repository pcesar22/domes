/**
 * @file test_version_parser.cpp
 * @brief Unit tests for firmware version parsing
 */

#include "unity.h"
#include "services/githubClient.hpp"

using namespace domes;

void setUp() {}
void tearDown() {}

TEST_CASE("parseVersion handles simple version", "[version]") {
    FirmwareVersion v = parseVersion("v1.2.3");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_FALSE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("", v.gitHash);
}

TEST_CASE("parseVersion handles version without v prefix", "[version]") {
    FirmwareVersion v = parseVersion("1.2.3");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
}

TEST_CASE("parseVersion handles dirty flag", "[version]") {
    FirmwareVersion v = parseVersion("v1.2.3-dirty");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_TRUE(v.dirty);
}

TEST_CASE("parseVersion handles git describe output", "[version]") {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_FALSE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d", v.gitHash);
}

TEST_CASE("parseVersion handles git describe with dirty", "[version]") {
    FirmwareVersion v = parseVersion("v1.2.3-5-ga1b2c3d-dirty");

    TEST_ASSERT_EQUAL_UINT8(1, v.major);
    TEST_ASSERT_EQUAL_UINT8(2, v.minor);
    TEST_ASSERT_EQUAL_UINT8(3, v.patch);
    TEST_ASSERT_TRUE(v.dirty);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d", v.gitHash);
}

TEST_CASE("parseVersion handles zero version", "[version]") {
    FirmwareVersion v = parseVersion("v0.0.0");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

TEST_CASE("parseVersion handles large version numbers", "[version]") {
    FirmwareVersion v = parseVersion("v255.255.255");

    TEST_ASSERT_EQUAL_UINT8(255, v.major);
    TEST_ASSERT_EQUAL_UINT8(255, v.minor);
    TEST_ASSERT_EQUAL_UINT8(255, v.patch);
}

TEST_CASE("parseVersion handles null input", "[version]") {
    FirmwareVersion v = parseVersion(nullptr);

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

TEST_CASE("parseVersion handles empty string", "[version]") {
    FirmwareVersion v = parseVersion("");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

TEST_CASE("parseVersion handles invalid format", "[version]") {
    FirmwareVersion v = parseVersion("not-a-version");

    TEST_ASSERT_EQUAL_UINT8(0, v.major);
    TEST_ASSERT_EQUAL_UINT8(0, v.minor);
    TEST_ASSERT_EQUAL_UINT8(0, v.patch);
}

// Version comparison tests

TEST_CASE("FirmwareVersion compare equal versions", "[version]") {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.3");

    TEST_ASSERT_EQUAL_INT(0, v1.compare(v2));
}

TEST_CASE("FirmwareVersion compare major difference", "[version]") {
    FirmwareVersion v1 = parseVersion("v1.0.0");
    FirmwareVersion v2 = parseVersion("v2.0.0");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

TEST_CASE("FirmwareVersion compare minor difference", "[version]") {
    FirmwareVersion v1 = parseVersion("v1.2.0");
    FirmwareVersion v2 = parseVersion("v1.3.0");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

TEST_CASE("FirmwareVersion compare patch difference", "[version]") {
    FirmwareVersion v1 = parseVersion("v1.2.3");
    FirmwareVersion v2 = parseVersion("v1.2.4");

    TEST_ASSERT_TRUE(v1.compare(v2) < 0);
    TEST_ASSERT_TRUE(v2.compare(v1) > 0);
}

TEST_CASE("FirmwareVersion isUpdateAvailable", "[version]") {
    FirmwareVersion current = parseVersion("v1.0.0");
    FirmwareVersion newer = parseVersion("v1.0.1");
    FirmwareVersion older = parseVersion("v0.9.9");
    FirmwareVersion same = parseVersion("v1.0.0");

    TEST_ASSERT_TRUE(current.isUpdateAvailable(newer));
    TEST_ASSERT_FALSE(current.isUpdateAvailable(older));
    TEST_ASSERT_FALSE(current.isUpdateAvailable(same));
}

TEST_CASE("FirmwareVersion compare ignores dirty flag", "[version]") {
    FirmwareVersion clean = parseVersion("v1.0.0");
    FirmwareVersion dirty = parseVersion("v1.0.0-dirty");

    TEST_ASSERT_EQUAL_INT(0, clean.compare(dirty));
}

TEST_CASE("FirmwareVersion compare ignores git hash", "[version]") {
    FirmwareVersion v1 = parseVersion("v1.0.0-5-ga1b2c3d");
    FirmwareVersion v2 = parseVersion("v1.0.0-10-gx9y8z7w");

    TEST_ASSERT_EQUAL_INT(0, v1.compare(v2));
}
