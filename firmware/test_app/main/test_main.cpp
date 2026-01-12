/**
 * @file test_main.cpp
 * @brief Unity test runner entry point
 *
 * This file provides the entry point for running unit tests.
 * On Linux target, it runs all tests and exits with the result code.
 */

#include "unity.h"

#include <cstdio>

// External test registration - Unity finds these at link time
// No need to manually list them; Unity's TEST_CASE macro handles registration

void setUp() {
    // Called before each test - can be used for common setup
}

void tearDown() {
    // Called after each test - can be used for common cleanup
}

/**
 * @brief Application entry point
 *
 * Unity automatically discovers all TEST_CASE() macros at link time.
 * This just provides the main() and calls the Unity runner.
 */
extern "C" void app_main(void) {
    printf("\n");
    printf("========================================\n");
    printf("DOMES Firmware Unit Tests\n");
    printf("========================================\n");
    printf("\n");

    UNITY_BEGIN();

    // Unity runs all registered tests automatically
    // Tests are registered via TEST_CASE() macro

    int failures = UNITY_END();

    printf("\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }

#if defined(CONFIG_IDF_TARGET_LINUX)
    // On Linux target, exit with appropriate code for CI
    exit(failures);
#endif
}

#if defined(CONFIG_IDF_TARGET_LINUX) || !defined(ESP_PLATFORM)
/**
 * @brief Main entry point for Linux host target
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    app_main();
    return 0;
}
#endif
