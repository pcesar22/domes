/**
 * @file traceRecorder.cpp
 * @brief Singleton trace recorder implementation
 */

#include "traceRecorder.hpp"

#include "esp_log.h"
#include "freertos/semphr.h"
#include "kernelTrace.hpp"

#include <cstring>

namespace {
constexpr const char* kTag = "trace_rec";
StaticSemaphore_t gLifecycleMutexStorage;
SemaphoreHandle_t gLifecycleMutex = nullptr;

class LifecycleLock {
public:
    LifecycleLock() {
        acquired_ =
            gLifecycleMutex != nullptr && xSemaphoreTake(gLifecycleMutex, portMAX_DELAY) == pdTRUE;
    }
    ~LifecycleLock() {
        if (acquired_) {
            xSemaphoreGive(gLifecycleMutex);
        }
    }
    bool acquired() const { return acquired_; }

private:
    bool acquired_ = false;
};
}  // namespace

namespace domes::trace {

// Static member definitions
std::unique_ptr<TraceBuffer> Recorder::buffer_;
std::atomic<bool> Recorder::enabled_{false};
std::atomic<bool> Recorder::initialized_{false};
std::atomic<bool> Recorder::taskCatalogReady_{false};
std::atomic<const void*> Recorder::sessionOwner_{nullptr};
std::array<TaskNameEntry, kMaxRegisteredTasks> Recorder::taskNames_{};
size_t Recorder::taskNameCount_{0};
std::atomic<Recorder::StreamCallback> Recorder::streamCallback_{nullptr};

esp_err_t Recorder::init(size_t bufferSize) {
    if (initialized_.load()) {
        ESP_LOGW(kTag, "Trace recorder already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (gLifecycleMutex == nullptr) {
        gLifecycleMutex = xSemaphoreCreateMutexStatic(&gLifecycleMutexStorage);
        if (gLifecycleMutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    // Create and initialize buffer
    buffer_ = std::make_unique<TraceBuffer>(bufferSize);
    esp_err_t err = buffer_->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to initialize trace buffer: %s", esp_err_to_name(err));
        buffer_.reset();
        return err;
    }

    // Clear task name table
    for (auto& entry : taskNames_) {
        entry.handle = nullptr;
        entry.valid = false;
        entry.taskId = 0;
        entry.priority = 0;
        entry.coreAffinityMask = 0;
        entry.name[0] = '\0';
    }
    taskNameCount_ = 0;
    KernelTrace::clearTaskHandles();

    initialized_.store(true);
    taskCatalogReady_.store(false, std::memory_order_release);
    sessionOwner_.store(nullptr, std::memory_order_release);
    enabled_.store(false);  // Start disabled by default

    ESP_LOGI(kTag, "Trace recorder initialized");
    return ESP_OK;
}

void Recorder::shutdown() {
    LifecycleLock lifecycleLock;
#ifdef ESP_PLATFORM
    if (lifecycleLock.acquired() && initialized_.load(std::memory_order_acquire) && buffer_) {
        enabled_.store(false, std::memory_order_release);
        KernelTrace::stopAndFlush(*buffer_);
    }
#endif
    KernelTrace::clearTaskHandles();
    streamCallback_.store(nullptr, std::memory_order_release);
    enabled_.store(false);
    initialized_.store(false);
    taskCatalogReady_.store(false, std::memory_order_release);
    sessionOwner_.store(nullptr, std::memory_order_release);

    // Release buffer
    buffer_.reset();
    ESP_LOGI(kTag, "Trace recorder shut down");
}

bool Recorder::setEnabled(bool enabled) {
    return setEnabledForLease(enabled, nullptr);
}

bool Recorder::acquireSessionLease(const void* owner, bool allowEnabled) {
    LifecycleLock lifecycleLock;
    if (!lifecycleLock.acquired() || owner == nullptr || !initialized_.load()) {
        return false;
    }
    const void* currentOwner = sessionOwner_.load(std::memory_order_acquire);
    if (currentOwner == owner) {
        return true;
    }
    if (currentOwner != nullptr || (!allowEnabled && enabled_.load(std::memory_order_acquire))) {
        return false;
    }
    sessionOwner_.store(owner, std::memory_order_release);
    return true;
}

bool Recorder::releaseSessionLease(const void* owner) {
    LifecycleLock lifecycleLock;
    if (!lifecycleLock.acquired() || owner == nullptr ||
        sessionOwner_.load(std::memory_order_acquire) != owner) {
        return false;
    }
    sessionOwner_.store(nullptr, std::memory_order_release);
    return true;
}

bool Recorder::isSessionLeased() {
    return sessionOwner_.load(std::memory_order_acquire) != nullptr;
}

bool Recorder::setEnabledForLease(bool enabled, const void* owner) {
    LifecycleLock lifecycleLock;
    if (!lifecycleLock.acquired()) {
        ESP_LOGE(kTag, "Cannot change trace state - lifecycle mutex unavailable");
        return false;
    }
    const void* sessionOwner = sessionOwner_.load(std::memory_order_acquire);
    if (sessionOwner != owner) {
        ESP_LOGW(kTag, "Cannot change trace state - session is exclusively owned");
        return false;
    }
    if (!initialized_.load()) {
        ESP_LOGW(kTag, "Cannot enable/disable - not initialized");
        return false;
    }
    if (enabled && !taskCatalogReady_.load(std::memory_order_acquire)) {
        ESP_LOGW(kTag, "Cannot enable tracing before task catalog is finalized");
        return false;
    }

    const bool wasEnabled = enabled_.load(std::memory_order_acquire);
    if (wasEnabled == enabled) {
        return true;
    }
#ifdef ESP_PLATFORM
    if (enabled) {
        buffer_->clear();
        KernelTrace::start();
        // Bound each session with an immutable task-catalog preamble. These
        // events mean "present when capture started" and make task identity
        // explicit even though production tasks predate host TRACE_START.
        KernelTrace::beginTaskSnapshot();
        for (const auto& entry : taskNames_) {
            if (entry.valid && KernelTrace::isTaskActive(entry.taskId)) {
                KernelTrace::recordPreamble(
                    KernelTrace::makeKernelEvent(EventType::kSchedTaskCreate, entry.taskId,
                                                 entry.priority, entry.coreAffinityMask));
            }
        }
        KernelTrace::enable();
        KernelTrace::endTaskSnapshot();
        enabled_.store(true, std::memory_order_release);
    } else {
        enabled_.store(false, std::memory_order_release);
        KernelTrace::stopAndFlush(*buffer_);
    }
#else
    enabled_.store(enabled, std::memory_order_release);
#endif
    if (wasEnabled != enabled) {
        ESP_LOGI(kTag, "Tracing %s", enabled ? "enabled" : "disabled");
    }
    return true;
}

bool Recorder::isEnabled() {
    return initialized_.load() && enabled_.load();
}

bool Recorder::isInitialized() {
    return initialized_.load();
}

void Recorder::finalizeTaskCatalog() {
    LifecycleLock lifecycleLock;
    if (lifecycleLock.acquired() && initialized_.load(std::memory_order_acquire)) {
        taskCatalogReady_.store(true, std::memory_order_release);
    }
}

bool Recorder::isTaskCatalogReady() {
    return initialized_.load(std::memory_order_acquire) &&
           taskCatalogReady_.load(std::memory_order_acquire);
}

void Recorder::record(const TraceEvent& event) {
    if (!isEnabled() || !buffer_) {
        return;
    }
#ifdef ESP_PLATFORM
    KernelTrace::record(event);
#else
    buffer_->record(event);
#endif
    // Kernel hooks are intentionally retained for post-capture export. Live
    // streaming remains application-event only so hooks never format or
    // enqueue into a second channel from ISR context.
    auto cb = streamCallback_.load(std::memory_order_acquire);
    if (cb) {
        cb(event);
    }
}

void Recorder::recordFromIsr(const TraceEvent& event) {
    if (!initialized_.load() || !enabled_.load() || !buffer_) {
        return;
    }
#ifdef ESP_PLATFORM
    KernelTrace::recordFromIsr(event);
#else
    buffer_->recordFromIsr(event);
#endif
}

TraceBuffer& Recorder::buffer() {
    return *buffer_;
}

bool Recorder::registerTask(TaskHandle_t handle, const char* name, uint16_t stableId,
                            UBaseType_t priority, BaseType_t coreAffinity) {
    LifecycleLock lifecycleLock;
    if (!lifecycleLock.acquired()) {
        return false;
    }
    if (!initialized_.load() || handle == nullptr || name == nullptr || name[0] == '\0' ||
        std::strlen(name) >= kMaxTaskNameLength || stableId == 0 || stableId > 31 ||
        priority > UINT8_MAX ||
        (coreAffinity != 0 && coreAffinity != 1 && coreAffinity != tskNO_AFFINITY) ||
        taskCatalogReady_.load(std::memory_order_acquire)) {
        return false;
    }

    const uint8_t coreMask = coreAffinity == 0 ? 0x01 : (coreAffinity == 1 ? 0x02 : 0x03);
    TaskNameEntry* selected = nullptr;
    for (auto& entry : taskNames_) {
        if (!entry.valid || (entry.handle != handle && entry.taskId != stableId)) {
            continue;
        }
        if (entry.taskId != stableId) {
            return false;
        }
        const bool metadataMatches = entry.priority == priority &&
                                     entry.coreAffinityMask == coreMask &&
                                     std::strcmp(entry.name, name) == 0;
        if (!metadataMatches || (entry.handle != nullptr && entry.handle != handle &&
                                 KernelTrace::taskId(entry.handle) != 0)) {
            return false;
        }
        selected = &entry;
        break;
    }

    if (selected == nullptr) {
        for (auto& entry : taskNames_) {
            if (!entry.valid) {
                selected = &entry;
                break;
            }
        }
    }
    if (selected == nullptr) {
        ESP_LOGW(kTag, "Task name table full, cannot register '%s'", name);
        return false;
    }

    if (!KernelTrace::registerTaskHandle(handle, stableId)) {
        return false;
    }
    const bool newEntry = !selected->valid;
    selected->handle = handle;
    selected->taskId = stableId;
    selected->priority = static_cast<uint8_t>(priority);
    selected->coreAffinityMask = coreMask;
    strncpy(selected->name, name, kMaxTaskNameLength - 1);
    selected->name[kMaxTaskNameLength - 1] = '\0';
    selected->valid = true;
    KernelTrace::setTaskActive(stableId, true);
    if (newEntry) {
        ++taskNameCount_;
    }
    ESP_LOGD(kTag, "Registered task '%s' with ID %u", name, stableId);
    return true;
}

void Recorder::unregisterTask(TaskHandle_t handle) {
    LifecycleLock lifecycleLock;
    if (!lifecycleLock.acquired()) {
        return;
    }
    if (!initialized_.load() || handle == nullptr) {
        return;
    }

    for (auto& entry : taskNames_) {
        if (entry.valid && entry.handle == handle) {
            entry.handle = nullptr;
            KernelTrace::setTaskActive(entry.taskId, false);
            KernelTrace::unregisterTaskHandle(handle);
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

uint32_t Recorder::eventCount() {
#ifdef ESP_PLATFORM
    if (isEnabled()) {
        return KernelTrace::eventCount();
    }
#endif
    return buffer_ ? static_cast<uint32_t>(buffer_->count()) : 0;
}

uint32_t Recorder::droppedCount() {
#ifdef ESP_PLATFORM
    return KernelTrace::droppedCount() + (buffer_ ? buffer_->droppedCount() : 0);
#else
    return buffer_ ? buffer_->droppedCount() : 0;
#endif
}

uint32_t Recorder::discontinuityCount() {
#ifdef ESP_PLATFORM
    return KernelTrace::discontinuityCount();
#else
    return 0;
#endif
}

void Recorder::setStreamCallback(StreamCallback cb) {
    streamCallback_.store(cb, std::memory_order_release);
}

bool Recorder::isStreaming() {
    return streamCallback_.load(std::memory_order_relaxed) != nullptr;
}

}  // namespace domes::trace
