/**
 * @file mutex.hpp
 * @brief RAII mutex wrapper for FreeRTOS
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace utils {

/**
 * @brief FreeRTOS mutex wrapper
 *
 * Provides a C++ interface for FreeRTOS mutexes with optional
 * static allocation for no-heap-after-init policy.
 */
class Mutex {
public:
    /**
     * @brief Construct mutex with static allocation
     */
    Mutex() {
        handle_ = xSemaphoreCreateMutexStatic(&buffer_);
    }

    ~Mutex() = default;

    // Non-copyable, non-movable
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(Mutex&&) = delete;

    /**
     * @brief Lock the mutex (blocking)
     */
    void lock() {
        xSemaphoreTake(handle_, portMAX_DELAY);
    }

    /**
     * @brief Try to lock with timeout
     * @param timeoutMs Timeout in milliseconds
     * @return true if lock acquired, false on timeout
     */
    bool tryLock(uint32_t timeoutMs) {
        return xSemaphoreTake(handle_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
    }

    /**
     * @brief Try to lock without waiting
     * @return true if lock acquired, false if already locked
     */
    bool tryLock() {
        return xSemaphoreTake(handle_, 0) == pdTRUE;
    }

    /**
     * @brief Unlock the mutex
     */
    void unlock() {
        xSemaphoreGive(handle_);
    }

    /**
     * @brief Get the native FreeRTOS handle
     */
    SemaphoreHandle_t native_handle() const { return handle_; }

private:
    StaticSemaphore_t buffer_;
    SemaphoreHandle_t handle_;
};

/**
 * @brief RAII lock guard for Mutex
 *
 * Automatically unlocks mutex when guard goes out of scope.
 */
class MutexGuard {
public:
    /**
     * @brief Lock mutex on construction
     * @param mutex Mutex to lock
     */
    explicit MutexGuard(Mutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }

    /**
     * @brief Unlock mutex on destruction
     */
    ~MutexGuard() {
        mutex_.unlock();
    }

    // Non-copyable, non-movable
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
    MutexGuard(MutexGuard&&) = delete;
    MutexGuard& operator=(MutexGuard&&) = delete;

private:
    Mutex& mutex_;
};

/**
 * @brief RAII lock guard with optional unlock
 *
 * Similar to std::unique_lock, allows early unlock and re-lock.
 */
class UniqueLock {
public:
    explicit UniqueLock(Mutex& mutex) : mutex_(&mutex), locked_(true) {
        mutex_->lock();
    }

    ~UniqueLock() {
        if (locked_) {
            mutex_->unlock();
        }
    }

    // Non-copyable
    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;

    // Movable
    UniqueLock(UniqueLock&& other) noexcept
        : mutex_(other.mutex_), locked_(other.locked_) {
        other.mutex_ = nullptr;
        other.locked_ = false;
    }

    UniqueLock& operator=(UniqueLock&& other) noexcept {
        if (this != &other) {
            if (locked_) {
                mutex_->unlock();
            }
            mutex_ = other.mutex_;
            locked_ = other.locked_;
            other.mutex_ = nullptr;
            other.locked_ = false;
        }
        return *this;
    }

    void unlock() {
        if (locked_) {
            mutex_->unlock();
            locked_ = false;
        }
    }

    void lock() {
        if (!locked_ && mutex_) {
            mutex_->lock();
            locked_ = true;
        }
    }

    bool ownsLock() const { return locked_; }

private:
    Mutex* mutex_;
    bool locked_;
};

}  // namespace utils
