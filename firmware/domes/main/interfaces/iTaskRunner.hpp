#pragma once

/**
 * @file iTaskRunner.hpp
 * @brief Abstract interface for runnable task objects
 *
 * Allows dependency injection of task implementations for testing
 * and provides a consistent pattern for task lifecycle management.
 */

#include "esp_err.h"

namespace domes {

/**
 * @brief Interface for runnable task objects
 *
 * Task implementations inherit this and implement run().
 * Used by TaskManager for managed task creation.
 *
 * @code
 * class MyTask : public ITaskRunner {
 * public:
 *     void run() override {
 *         while (shouldRun()) {
 *             doWork();
 *             Watchdog::reset();
 *             vTaskDelay(pdMS_TO_TICKS(100));
 *         }
 *     }
 *
 *     esp_err_t requestStop() override {
 *         running_ = false;
 *         return ESP_OK;
 *     }
 *
 *     bool shouldRun() const override { return running_; }
 *
 * private:
 *     volatile bool running_ = true;
 * };
 * @endcode
 */
class ITaskRunner {
public:
    virtual ~ITaskRunner() = default;

    /**
     * @brief Main task entry point
     *
     * Called by TaskManager after task creation. Should contain
     * the main loop or finite work of the task.
     *
     * @note Must call Watchdog::reset() if subscribed to TWDT
     * @note Should check shouldRun() periodically for graceful shutdown
     */
    virtual void run() = 0;

    /**
     * @brief Request graceful task termination
     *
     * Sets internal flag to signal the task should exit.
     * Task should check shouldRun() and exit cleanly.
     *
     * @return ESP_OK if request accepted
     */
    virtual esp_err_t requestStop() = 0;

    /**
     * @brief Check if task should continue running
     * @return true if task should continue, false if stop requested
     */
    virtual bool shouldRun() const = 0;
};

} // namespace domes
