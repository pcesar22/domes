#pragma once

/**
 * @file traceRecorder.hpp
 * @brief Singleton trace recorder for coordinating trace operations
 *
 * Provides the main interface for trace recording, including:
 * - Initialization and shutdown
 * - Enable/disable recording
 * - Task name registration
 * - Access to the trace buffer for dump operations
 */

#include "traceBuffer.hpp"
#include "traceEvent.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace domes::trace {

/// Maximum number of tasks that can have registered names
constexpr size_t kMaxRegisteredTasks = 32;

/// Maximum task name length (including null terminator)
constexpr size_t kMaxTaskNameLength = 16;

/**
 * @brief Task name entry for trace metadata
 */
struct TaskNameEntry {
    uint16_t taskId;                      ///< FreeRTOS task number
    char name[kMaxTaskNameLength];        ///< Task name (null-terminated)
    bool valid;                           ///< Entry is valid
};

/**
 * @brief Singleton trace recorder
 *
 * Coordinates all trace operations including:
 * - Ring buffer management
 * - FreeRTOS hook callbacks
 * - Task name registration for trace output
 *
 * Usage:
 * @code
 * // During initialization
 * domes::trace::Recorder::init();
 * domes::trace::Recorder::setEnabled(true);
 *
 * // Register task names for better trace output
 * domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main");
 *
 * // Recording events
 * domes::trace::Recorder::record(event);
 * @endcode
 */
class Recorder {
public:
    // Delete copy/move constructors (singleton)
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    /**
     * @brief Initialize the trace recorder
     *
     * Must be called once during startup before any trace operations.
     *
     * @param bufferSize Size of the ring buffer in bytes (default 32KB)
     * @return ESP_OK on success, error code otherwise
     */
    static esp_err_t init(size_t bufferSize = TraceBuffer::kDefaultBufferSize);

    /**
     * @brief Shut down the trace recorder
     *
     * Releases all resources. After this, init() must be called again.
     */
    static void shutdown();

    /**
     * @brief Enable or disable trace recording
     *
     * When disabled, all trace events are silently dropped.
     *
     * @param enabled true to enable, false to disable
     */
    static void setEnabled(bool enabled);

    /**
     * @brief Check if tracing is enabled
     */
    static bool isEnabled();

    /**
     * @brief Check if recorder is initialized
     */
    static bool isInitialized();

    /**
     * @brief Record a trace event (task context)
     *
     * Thread-safe. If tracing is disabled or buffer is full,
     * the event is silently dropped.
     *
     * @param event Event to record
     */
    static void record(const TraceEvent& event);

    /**
     * @brief Record a trace event (ISR context)
     *
     * Non-blocking. Safe to call from interrupt handlers.
     *
     * @param event Event to record
     */
    static void recordFromIsr(const TraceEvent& event);

    /**
     * @brief Get the trace buffer
     *
     * Used for dump operations.
     *
     * @return Reference to the trace buffer
     */
    static TraceBuffer& buffer();

    /**
     * @brief Register a task name for trace output
     *
     * Task names are included in trace metadata for human-readable output.
     *
     * @param handle FreeRTOS task handle
     * @param name Task name (will be truncated if too long)
     */
    static void registerTask(TaskHandle_t handle, const char* name);

    /**
     * @brief Unregister a task
     *
     * @param handle FreeRTOS task handle to unregister
     */
    static void unregisterTask(TaskHandle_t handle);

    /**
     * @brief Get task name by task ID
     *
     * @param taskId FreeRTOS task number
     * @return Task name or nullptr if not registered
     */
    static const char* getTaskName(uint16_t taskId);

    /**
     * @brief Get all registered task names
     *
     * @return Reference to the task name table
     */
    static const std::array<TaskNameEntry, kMaxRegisteredTasks>& getTaskNames();

    /**
     * @brief Get count of registered tasks
     */
    static size_t getRegisteredTaskCount();

private:
    Recorder() = default;
    ~Recorder() = default;

    static std::unique_ptr<TraceBuffer> buffer_;
    static std::atomic<bool> enabled_;
    static std::atomic<bool> initialized_;
    static std::array<TaskNameEntry, kMaxRegisteredTasks> taskNames_;
    static size_t taskNameCount_;
};

}  // namespace domes::trace
