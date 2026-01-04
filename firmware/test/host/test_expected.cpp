/**
 * @file test_expected.cpp
 * @brief Tests for tl::expected error handling
 */

#include "unity.h"
#include "utils/expected.hpp"
#include "esp_err.h"

extern "C" {

void test_expected_value(void) {
    /* Create expected with value */
    tl::expected<int, esp_err_t> result = 42;

    /* Should have value */
    TEST_ASSERT_TRUE(result.has_value());
    TEST_ASSERT_TRUE(static_cast<bool>(result));

    /* Value should be accessible */
    TEST_ASSERT_EQUAL(42, result.value());
    TEST_ASSERT_EQUAL(42, *result);
}

void test_expected_error(void) {
    /* Create expected with error */
    tl::expected<int, esp_err_t> result = tl::unexpected(ESP_ERR_INVALID_ARG);

    /* Should not have value */
    TEST_ASSERT_FALSE(result.has_value());
    TEST_ASSERT_FALSE(static_cast<bool>(result));

    /* Error should be accessible */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result.error());
}

void test_expected_value_or(void) {
    /* Expected with value should return value */
    tl::expected<int, esp_err_t> success = 42;
    TEST_ASSERT_EQUAL(42, success.value_or(0));

    /* Expected with error should return default */
    tl::expected<int, esp_err_t> error = tl::unexpected(ESP_FAIL);
    TEST_ASSERT_EQUAL(99, error.value_or(99));
}

void test_expected_void(void) {
    /* Void expected with success */
    tl::expected<void, esp_err_t> success;
    TEST_ASSERT_TRUE(success.has_value());

    /* Void expected with error */
    tl::expected<void, esp_err_t> error = tl::unexpected(ESP_ERR_TIMEOUT);
    TEST_ASSERT_FALSE(error.has_value());
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, error.error());
}

}  /* extern "C" */
