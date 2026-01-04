/**
 * @file test_color.cpp
 * @brief Tests for Color struct in LED driver interface
 */

#include "unity.h"
#include "interfaces/iLedDriver.hpp"

extern "C" {

void test_color_black(void) {
    Color black = Color::black();

    TEST_ASSERT_EQUAL_UINT8(0, black.r);
    TEST_ASSERT_EQUAL_UINT8(0, black.g);
    TEST_ASSERT_EQUAL_UINT8(0, black.b);
    TEST_ASSERT_EQUAL_UINT8(0, black.w);
}

void test_color_white(void) {
    /* Default white (full brightness) */
    Color white = Color::white();
    TEST_ASSERT_EQUAL_UINT8(0, white.r);
    TEST_ASSERT_EQUAL_UINT8(0, white.g);
    TEST_ASSERT_EQUAL_UINT8(0, white.b);
    TEST_ASSERT_EQUAL_UINT8(255, white.w);

    /* White with custom brightness */
    Color dimWhite = Color::white(128);
    TEST_ASSERT_EQUAL_UINT8(0, dimWhite.r);
    TEST_ASSERT_EQUAL_UINT8(0, dimWhite.g);
    TEST_ASSERT_EQUAL_UINT8(0, dimWhite.b);
    TEST_ASSERT_EQUAL_UINT8(128, dimWhite.w);
}

void test_color_rgb(void) {
    /* Red */
    Color red = Color::red();
    TEST_ASSERT_EQUAL_UINT8(255, red.r);
    TEST_ASSERT_EQUAL_UINT8(0, red.g);
    TEST_ASSERT_EQUAL_UINT8(0, red.b);
    TEST_ASSERT_EQUAL_UINT8(0, red.w);

    /* Green */
    Color green = Color::green();
    TEST_ASSERT_EQUAL_UINT8(0, green.r);
    TEST_ASSERT_EQUAL_UINT8(255, green.g);
    TEST_ASSERT_EQUAL_UINT8(0, green.b);
    TEST_ASSERT_EQUAL_UINT8(0, green.w);

    /* Blue */
    Color blue = Color::blue();
    TEST_ASSERT_EQUAL_UINT8(0, blue.r);
    TEST_ASSERT_EQUAL_UINT8(0, blue.g);
    TEST_ASSERT_EQUAL_UINT8(255, blue.b);
    TEST_ASSERT_EQUAL_UINT8(0, blue.w);
}

void test_color_brightness(void) {
    /* Colors with custom brightness */
    Color dimRed = Color::red(100);
    TEST_ASSERT_EQUAL_UINT8(100, dimRed.r);
    TEST_ASSERT_EQUAL_UINT8(0, dimRed.g);

    Color dimGreen = Color::green(50);
    TEST_ASSERT_EQUAL_UINT8(50, dimGreen.g);
    TEST_ASSERT_EQUAL_UINT8(0, dimGreen.r);

    Color dimBlue = Color::blue(200);
    TEST_ASSERT_EQUAL_UINT8(200, dimBlue.b);
    TEST_ASSERT_EQUAL_UINT8(0, dimBlue.r);
}

}  /* extern "C" */
