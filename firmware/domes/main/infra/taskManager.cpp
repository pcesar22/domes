/**
 * @file taskManager.cpp
 * @brief Task manager implementation
 */

#include "infra/taskManager.hpp"

#include "infra/logging.hpp"
#include "infra/watchdog.hpp"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kTask;

namespace domes::infra {

TaskManager::~TaskManager() {
    stopAllTasks(1000);
}

esp_err_t TaskManager::createTask(const TaskConfig& config, ITaskRunner& runner) {
    size_t slotIdx = findFreeSlot();
    if (slotIdx >= kMaxManagedTasks) {
        ESP_LOGE(kTag, "No free task slots (max=%zu)", kMaxManagedTasks);
        return ESP_ERR_NO_MEM;
    }

    TaskSlot& slot = slots_[slotIdx];
    slot.runner = &runner;
    slot.name = config.name;
    slot.watchdogSubscribed = config.subscribeToWatchdog;
    slot.owner = this;
    slot.active.store(true, std::memory_order_release);
    activeCount_.fetch_add(1, std::memory_order_acq_rel);

    BaseType_t result;
    if (config.coreAffinity == core::kAny) {
        // Create unpinned task
        result = xTaskCreate(taskEntryPoint, config.name, config.stackSize, &slot, config.priority,
                             &slot.handle);
    } else {
        // Create pinned task
        result = xTaskCreatePinnedToCore(taskEntryPoint, config.name, config.stackSize, &slot,
                                         config.priority, &slot.handle, config.coreAffinity);
    }

    if (result != pdPASS) {
        ESP_LOGE(kTag, "Failed to create task '%s'", config.name);
        slot.active.store(false, std::memory_order_release);
        activeCount_.fetch_sub(1, std::memory_order_acq_rel);
        slot.runner = nullptr;
        slot.owner = nullptr;
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Created task '%s' (stack=%lu, prio=%u, core=%s, wdt=%s)", config.name,
             static_cast<unsigned long>(config.stackSize), static_cast<unsigned>(config.priority),
             config.coreAffinity == core::kAny
                 ? "any"
                 : (config.coreAffinity == core::kProtocol ? "0" : "1"),
             config.subscribeToWatchdog ? "yes" : "no");

    return ESP_OK;
}

esp_err_t TaskManager::stopAllTasks(uint32_t timeoutMs) {
    // Request stop on all tasks
    for (auto& slot : slots_) {
        if (slot.active.load(std::memory_order_acquire) && slot.runner) {
            const esp_err_t err = slot.runner->requestStop();
            if (err != ESP_OK) {
                ESP_LOGW(kTag, "Task '%s' rejected stop request: %s", slot.name,
                         esp_err_to_name(err));
            }
        }
    }

    // Each task clears its slot and decrements activeCount_ immediately before
    // deleting itself.
    TickType_t startTick = xTaskGetTickCount();
    TickType_t timeoutTicks = pdMS_TO_TICKS(timeoutMs);

    while (activeCount_.load(std::memory_order_acquire) > 0) {
        // Check for timeout
        if ((xTaskGetTickCount() - startTick) > timeoutTicks) {
            ESP_LOGW(kTag, "Timeout waiting for tasks to stop (%zu still active)",
                     activeCount_.load(std::memory_order_acquire));
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (auto& slot : slots_) {
        slot.handle = nullptr;
        slot.runner = nullptr;
        slot.name = nullptr;
        slot.owner = nullptr;
    }

    ESP_LOGI(kTag, "All tasks stopped");
    return ESP_OK;
}

TaskHandle_t TaskManager::getTaskHandle(const char* name) const {
    for (const auto& slot : slots_) {
        if (slot.active.load(std::memory_order_acquire) && slot.name &&
            strcmp(slot.name, name) == 0) {
            return slot.handle;
        }
    }
    return nullptr;
}

size_t TaskManager::getActiveTaskCount() const {
    return activeCount_.load(std::memory_order_acquire);
}

size_t TaskManager::findFreeSlot() const {
    for (size_t i = 0; i < kMaxManagedTasks; i++) {
        if (!slots_[i].active.load(std::memory_order_acquire)) {
            return i;
        }
    }
    return kMaxManagedTasks;
}

void TaskManager::taskEntryPoint(void* param) {
    TaskSlot* slot = static_cast<TaskSlot*>(param);

    // Subscribe to watchdog if requested
    if (slot->watchdogSubscribed && Watchdog::isInitialized()) {
        Watchdog::subscribe();
        ESP_LOGD(kTag, "Task '%s' subscribed to watchdog", slot->name);
    }

    // Run the task
    ESP_LOGD(kTag, "Task '%s' starting", slot->name);
    slot->runner->run();
    ESP_LOGD(kTag, "Task '%s' exiting", slot->name);

    // Unsubscribe from watchdog
    if (slot->watchdogSubscribed && Watchdog::isInitialized()) {
        Watchdog::unsubscribe();
    }

    // Publish completion before deleting this task. The manager owns the
    // bounded slot storage for the firmware lifetime.
    if (slot->active.exchange(false, std::memory_order_acq_rel) && slot->owner) {
        slot->owner->activeCount_.fetch_sub(1, std::memory_order_acq_rel);
    }

    // Delete self
    vTaskDelete(nullptr);
}

}  // namespace domes::infra
