/**
 * @file test_constants.cpp
 * @brief Tests for firmware constants
 */

#include "unity.h"
#include "config/constants.hpp"

extern "C" {

void test_version_constants(void) {
    /* Version should be valid numbers */
    TEST_ASSERT_GREATER_OR_EQUAL(0, config::kVersionMajor);
    TEST_ASSERT_GREATER_OR_EQUAL(0, config::kVersionMinor);
    TEST_ASSERT_GREATER_OR_EQUAL(0, config::kVersionPatch);

    /* Version should be reasonable (not overflow) */
    TEST_ASSERT_LESS_THAN(100, config::kVersionMajor);
    TEST_ASSERT_LESS_THAN(100, config::kVersionMinor);
    TEST_ASSERT_LESS_THAN(100, config::kVersionPatch);
}

void test_led_constants(void) {
    /* LED count should be valid for DOMES pod */
    TEST_ASSERT_EQUAL(16, config::kLedCount);

    /* Default brightness should be in valid range */
    TEST_ASSERT_GREATER_THAN(0, config::kLedDefaultBrightness);
    TEST_ASSERT_LESS_OR_EQUAL(255, config::kLedDefaultBrightness);

    /* Refresh rate should be reasonable */
    TEST_ASSERT_GREATER_OR_EQUAL(30, config::kLedRefreshRateHz);
    TEST_ASSERT_LESS_OR_EQUAL(120, config::kLedRefreshRateHz);
}

void test_audio_constants(void) {
    /* Sample rate should be valid */
    TEST_ASSERT_EQUAL(16000, config::kAudioSampleRate);

    /* Bits per sample */
    TEST_ASSERT_EQUAL(16, config::kAudioBitsPerSample);

    /* Buffer size should be reasonable */
    TEST_ASSERT_GREATER_THAN(0, config::kAudioBufferSizeBytes);
    TEST_ASSERT_LESS_THAN(65536, config::kAudioBufferSizeBytes);
}

void test_task_constants(void) {
    /* Stack sizes should be reasonable for ESP32-S3 */
    TEST_ASSERT_GREATER_OR_EQUAL(2048, config::kAudioTaskStackSize);
    TEST_ASSERT_GREATER_OR_EQUAL(2048, config::kGameTaskStackSize);
    TEST_ASSERT_GREATER_OR_EQUAL(2048, config::kCommTaskStackSize);
    TEST_ASSERT_GREATER_OR_EQUAL(1024, config::kLedTaskStackSize);

    /* Stack sizes should not be excessive */
    TEST_ASSERT_LESS_THAN(32768, config::kAudioTaskStackSize);

    /* Priorities should be valid FreeRTOS range (0-24 typically) */
    TEST_ASSERT_GREATER_THAN(0, config::kAudioTaskPriority);
    TEST_ASSERT_LESS_THAN(25, config::kAudioTaskPriority);
}

}  /* extern "C" */
