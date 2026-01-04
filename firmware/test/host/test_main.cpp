/**
 * @file test_main.cpp
 * @brief Host test runner for DOMES firmware
 */

#include "unity.h"

/* Forward declarations of test functions */
extern "C" {
    /* test_constants.cpp */
    void test_version_constants(void);
    void test_led_constants(void);
    void test_audio_constants(void);
    void test_task_constants(void);

    /* test_expected.cpp */
    void test_expected_value(void);
    void test_expected_error(void);
    void test_expected_value_or(void);
    void test_expected_void(void);

    /* test_color.cpp */
    void test_color_black(void);
    void test_color_white(void);
    void test_color_rgb(void);
    void test_color_brightness(void);
}

int main() {
    UNITY_BEGIN();

    /* Constants tests */
    RUN_TEST(test_version_constants);
    RUN_TEST(test_led_constants);
    RUN_TEST(test_audio_constants);
    RUN_TEST(test_task_constants);

    /* Expected tests */
    RUN_TEST(test_expected_value);
    RUN_TEST(test_expected_error);
    RUN_TEST(test_expected_value_or);
    RUN_TEST(test_expected_void);

    /* Color tests */
    RUN_TEST(test_color_black);
    RUN_TEST(test_color_white);
    RUN_TEST(test_color_rgb);
    RUN_TEST(test_color_brightness);

    return UNITY_END();
}
