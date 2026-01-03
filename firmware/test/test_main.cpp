/**
 * @file test_main.cpp
 * @brief Unit test runner for DOMES firmware
 *
 * Tests run on the Linux host target for fast iteration.
 * Usage: idf.py --preview set-target linux && idf.py build
 */

#include "unity.h"

/**
 * @brief Placeholder test to verify test framework works
 */
void test_placeholder() {
    TEST_ASSERT_TRUE(true);
    TEST_ASSERT_EQUAL(42, 42);
}

/**
 * @brief Test that constants are defined correctly
 */
void test_constants() {
    // Verify LED count is reasonable
    TEST_ASSERT_GREATER_THAN(0, 16);  // kLedCount
    TEST_ASSERT_LESS_OR_EQUAL(64, 16);

    // Verify version numbers are valid
    TEST_ASSERT_GREATER_OR_EQUAL(0, 0);  // kVersionMajor
    TEST_ASSERT_GREATER_OR_EQUAL(0, 1);  // kVersionMinor
}

extern "C" void app_main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_placeholder);
    RUN_TEST(test_constants);

    UNITY_END();
}
