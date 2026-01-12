#pragma once

/**
 * @file taskManager.hpp
 * @brief FreeRTOS task lifecycle management with core pinning
 *
 * Provides structured task creation and management following
 * the ITaskRunner pattern.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "infra/taskConfig.hpp"
#include "interfaces/iTaskRunner.hpp"

#include <array>

namespace domes::infra {

/**
 * @brief Maximum number of managed tasks
 *
 * Uses static allocation to avoid runtime heap usage.
 */
constexpr size_t kMaxManagedTasks = 8;

/**
 * @brief Task slot for tracking managed tasks
 */
struct TaskSlot {
    TaskHandle_t handle = nullptr;
    ITaskRunner* runner = nullptr;
    const char* name = nullptr;
    bool watchdogSubscribed = false;
    bool active = false;
};

/**
 * @brief Manages FreeRTOS task lifecycle with core pinning
 *
 * Provides:
 * - Static task creation with core affinity
 * - Optional watchdog subscription
 * - Task handle management
 * - Graceful shutdown support
 *
 * @note All tasks should be created during app_main() init phase
 * @warning Task creation is not thread-safe - call only from app_main
 *
 * @code
 * // Define task runner
 * class MyTask : public ITaskRunner {
 *     void run() override { ... }
 *     esp_err_t requestStop() override { running_ = false; return ESP_OK; }
 *     bool shouldRun() const override { return running_; }
 * private:
 *     volatile bool running_ = true;
 * };
 *
 * // In app_main:
 * static TaskManager taskMgr;
 * static MyTask myTask;
 *
 * TaskConfig config = {
 *     .name = "my_task",
 *     .stackSize = stack::kStandard,
 *     .priority = priority::kMedium,
 *     .coreAffinity = core::kAny,
 *     .subscribeToWatchdog = true
 * };
 *
 * taskMgr.createTask(config, myTask);
 * @endcode
 */
class TaskManager {
public:
    TaskManager() = default;
    ~TaskManager();

    // Non-copyable
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    /**
     * @brief Create and start a managed task
     *
     * @param config Task configuration (priority, stack, core)
     * @param runner Task runner implementing ITaskRunner
     * @return ESP_OK on success
     * @return ESP_ERR_NO_MEM if max tasks reached
     * @return ESP_FAIL if task creation fails
     */
    esp_err_t createTask(const TaskConfig& config, ITaskRunner& runner);

    /**
     * @brief Request all tasks to stop gracefully
     *
     * Calls requestStop() on each task and waits for them to exit.
     *
     * @param timeoutMs Maximum time to wait for tasks to stop
     * @return ESP_OK if all stopped, ESP_ERR_TIMEOUT otherwise
     */
    esp_err_t stopAllTasks(uint32_t timeoutMs = 5000);

    /**
     * @brief Get handle for a named task
     * @param name Task name
     * @return TaskHandle_t or nullptr if not found
     */
    TaskHandle_t getTaskHandle(const char* name) const;

    /**
     * @brief Get number of active managed tasks
     */
    size_t getActiveTaskCount() const;

    /**
     * @brief Check if task manager has room for more tasks
     */
    bool hasCapacity() const { return activeCount_ < kMaxManagedTasks; }

private:
    /**
     * @brief Static task entry point
     *
     * Called by FreeRTOS, invokes runner->run().
     */
    static void taskEntryPoint(void* param);

    /**
     * @brief Find first free slot
     * @return Slot index or kMaxManagedTasks if full
     */
    size_t findFreeSlot() const;

    std::array<TaskSlot, kMaxManagedTasks> slots_;
    size_t activeCount_ = 0;
};

}  // namespace domes::infra
