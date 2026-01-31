/**
 * @file esp_timer.h
 * @brief Minimal ESP-IDF timer stubs for host unit tests
 *
 * Provides a controllable mock time for testing timeout logic.
 */
#pragma once

#include <cstdint>
#include <atomic>

// Global mock time (microseconds) - tests can set this directly
namespace test_stubs {
inline std::atomic<int64_t> mock_time_us{0};
}

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t esp_timer_get_time(void) {
    return test_stubs::mock_time_us.load();
}

#ifdef __cplusplus
}
#endif
