/**
 * @file traceRecorder.cpp
 * @brief Singleton trace recorder implementation
 */

#include "traceRecorder.hpp"

#include "esp_log.h"

#include <cstring>

namespace {
constexpr const char* kTag = "trace_rec";
}

namespace domes::trace {

// Static member definitions
std::unique_ptr<TraceBuffer> Recorder::buffer_;
std::atomic<bool> Recorder::enabled_{false};
std::atomic<bool> Recorder::initialized_{false};
std::array<TaskNameEntry, kMaxRegisteredTasks> Recorder::taskNames_{};
size_t Recorder::taskNameCount_{0};

esp_err_t Recorder::init(size_t bufferSize) {
    if (initialized_.load()) {
        ESP_LOGW(kTag, "Trace recorder already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Create and initialize buffer
    buffer_ = std::make_unique<TraceBuffer>(bufferSize);
    esp_err_t err = buffer_->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to initialize trace buffer: %s",
                 esp_err_to_name(err));
        buffer_.reset();
        return err;
    }

    // Clear task name table
    for (auto& entry : taskNames_) {
        entry.valid = false;
        entry.taskId = 0;
        entry.name[0] = '\0';
    }
    taskNameCount_ = 0;

    initialized_.store(true);
    enabled_.store(false);  // Start disabled by default

    ESP_LOGI(kTag, "Trace recorder initialized");
    return ESP_OK;
}

void Recorder::shutdown() {
    enabled_.store(false);
    initialized_.store(false);

    // Release buffer
    buffer_.reset();
    ESP_LOGI(kTag, "Trace recorder shut down");
}

void Recorder::setEnabled(bool enabled) {
    if (!initialized_.load()) {
        ESP_LOGW(kTag, "Cannot enable/disable - not initialized");
        return;
    }

    bool wasEnabled = enabled_.exchange(enabled);
    if (wasEnabled != enabled) {
        ESP_LOGI(kTag, "Tracing %s", enabled ? "enabled" : "disabled");
    }
}

bool Recorder::isEnabled() {
    return initialized_.load() && enabled_.load();
}

bool Recorder::isInitialized() {
    return initialized_.load();
}

void Recorder::record(const TraceEvent& event) {
    if (!isEnabled() || !buffer_) {
        return;
    }
    buffer_->record(event);
}

void Recorder::recordFromIsr(const TraceEvent& event) {
    if (!initialized_.load() || !enabled_.load() || !buffer_) {
        return;
    }
    buffer_->recordFromIsr(event);
}

TraceBuffer& Recorder::buffer() {
    return *buffer_;
}

void Recorder::registerTask(TaskHandle_t handle, const char* name) {
    if (!initialized_.load() || handle == nullptr || name == nullptr) {
        return;
    }

    uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(handle));

    // Check if task is already registered
    for (auto& entry : taskNames_) {
        if (entry.valid && entry.taskId == taskId) {
            // Update existing entry
            strncpy(entry.name, name, kMaxTaskNameLength - 1);
            entry.name[kMaxTaskNameLength - 1] = '\0';
            return;
        }
    }

    // Find empty slot
    for (auto& entry : taskNames_) {
        if (!entry.valid) {
            entry.taskId = taskId;
            strncpy(entry.name, name, kMaxTaskNameLength - 1);
            entry.name[kMaxTaskNameLength - 1] = '\0';
            entry.valid = true;
            taskNameCount_++;
            ESP_LOGD(kTag, "Registered task '%s' with ID %u", name, taskId);
            return;
        }
    }

    ESP_LOGW(kTag, "Task name table full, cannot register '%s'", name);
}

void Recorder::unregisterTask(TaskHandle_t handle) {
    if (!initialized_.load() || handle == nullptr) {
        return;
    }

    uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(handle));

    for (auto& entry : taskNames_) {
        if (entry.valid && entry.taskId == taskId) {
            entry.valid = false;
            entry.taskId = 0;
            entry.name[0] = '\0';
            taskNameCount_--;
            return;
        }
    }
}

const char* Recorder::getTaskName(uint16_t taskId) {
    for (const auto& entry : taskNames_) {
        if (entry.valid && entry.taskId == taskId) {
            return entry.name;
        }
    }
    return nullptr;
}

const std::array<TaskNameEntry, kMaxRegisteredTasks>& Recorder::getTaskNames() {
    return taskNames_;
}

size_t Recorder::getRegisteredTaskCount() {
    return taskNameCount_;
}

}  // namespace domes::trace
