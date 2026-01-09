#pragma once

/**
 * @file mutex.hpp
 * @brief RAII mutex wrapper for FreeRTOS
 *
 * Uses static allocation to avoid heap usage after init.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace domes::utils {

/**
 * @brief RAII wrapper for FreeRTOS mutex
 *
 * Uses static allocation to comply with GUIDELINES.md requirement
 * of no heap allocation after app_main() initialization.
 */
class Mutex {
public:
    Mutex() {
        handle_ = xSemaphoreCreateMutexStatic(&buffer_);
    }

    ~Mutex() = default;

    // Non-copyable
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    /**
     * @brief Acquire mutex (blocking)
     */
    void lock() {
        xSemaphoreTake(handle_, portMAX_DELAY);
    }

    /**
     * @brief Try to acquire mutex with timeout
     * @param timeout Timeout in ticks (0 for non-blocking)
     * @return true if acquired, false if timeout
     */
    bool tryLock(TickType_t timeout = 0) {
        return xSemaphoreTake(handle_, timeout) == pdTRUE;
    }

    /**
     * @brief Release mutex
     */
    void unlock() {
        xSemaphoreGive(handle_);
    }

    /**
     * @brief Get underlying handle for advanced use
     */
    SemaphoreHandle_t handle() const { return handle_; }

private:
    StaticSemaphore_t buffer_;
    SemaphoreHandle_t handle_;
};

/**
 * @brief RAII lock guard for Mutex
 *
 * Automatically locks on construction and unlocks on destruction.
 *
 * @code
 * void criticalSection() {
 *     MutexGuard guard(myMutex);
 *     // Protected code here
 * }  // Automatically unlocks
 * @endcode
 */
class MutexGuard {
public:
    explicit MutexGuard(Mutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }

    ~MutexGuard() {
        mutex_.unlock();
    }

    // Non-copyable
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

private:
    Mutex& mutex_;
};

} // namespace domes::utils
