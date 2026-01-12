#pragma once

/**
 * @file watchdog.hpp
 * @brief Task Watchdog Timer (TWDT) management wrapper
 *
 * Provides convenient API for watchdog subscription and reset.
 * Each task that performs long-running operations should subscribe
 * and periodically reset.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace domes::infra {

/**
 * @brief Watchdog timer management wrapper
 *
 * Provides TWDT subscription and reset API. Configuration is in sdkconfig:
 * - CONFIG_ESP_TASK_WDT_TIMEOUT_S (default 10s in this project)
 *
 * @code
 * // In task setup:
 * Watchdog::subscribe();
 *
 * // In task loop:
 * while (running) {
 *     doWork();
 *     Watchdog::reset();
 *     vTaskDelay(pdMS_TO_TICKS(100));
 * }
 *
 * // On task exit:
 * Watchdog::unsubscribe();
 * @endcode
 */
class Watchdog {
public:
    /**
     * @brief Initialize Task Watchdog Timer
     *
     * Must be called once at startup before subscribing tasks.
     *
     * @param timeoutSec Watchdog timeout in seconds
     * @param panicOnTimeout If true, triggers panic on timeout (recommended for production)
     * @return ESP_OK on success
     */
    static esp_err_t init(uint32_t timeoutSec = 10, bool panicOnTimeout = true);

    /**
     * @brief Deinitialize watchdog
     *
     * Use for testing or controlled shutdown.
     *
     * @return ESP_OK on success
     */
    static esp_err_t deinit();

    /**
     * @brief Subscribe current task to watchdog
     * @return ESP_OK on success
     */
    static esp_err_t subscribe();

    /**
     * @brief Subscribe a specific task to watchdog
     * @param taskHandle Task to subscribe
     * @return ESP_OK on success
     */
    static esp_err_t subscribe(TaskHandle_t taskHandle);

    /**
     * @brief Unsubscribe current task from watchdog
     * @return ESP_OK on success
     */
    static esp_err_t unsubscribe();

    /**
     * @brief Unsubscribe a specific task
     * @param taskHandle Task to unsubscribe
     * @return ESP_OK on success
     */
    static esp_err_t unsubscribe(TaskHandle_t taskHandle);

    /**
     * @brief Reset watchdog timer for current task
     *
     * Must be called periodically in task loop before timeout.
     *
     * @return ESP_OK on success
     */
    static esp_err_t reset();

    /**
     * @brief Check if watchdog is initialized
     * @return true if initialized
     */
    static bool isInitialized();

private:
    static bool initialized_;
};

/**
 * @brief RAII guard for watchdog subscription
 *
 * Automatically subscribes on construction, unsubscribes on destruction.
 *
 * @code
 * void myTask(void* param) {
 *     WatchdogGuard guard;  // Subscribes current task
 *     if (!guard.isSubscribed()) {
 *         // Handle subscription failure
 *         return;
 *     }
 *
 *     while (running) {
 *         doWork();
 *         Watchdog::reset();  // Still need to reset manually
 *         vTaskDelay(pdMS_TO_TICKS(100));
 *     }
 * }  // Unsubscribes automatically on exit
 * @endcode
 */
class WatchdogGuard {
public:
    WatchdogGuard();
    ~WatchdogGuard();

    // Non-copyable
    WatchdogGuard(const WatchdogGuard&) = delete;
    WatchdogGuard& operator=(const WatchdogGuard&) = delete;

    /**
     * @brief Check if subscription was successful
     * @return true if subscribed
     */
    bool isSubscribed() const { return subscribed_; }

private:
    bool subscribed_;
};

}  // namespace domes::infra
