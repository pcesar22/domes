/**
 * @file traceHooks.cpp
 * @brief FreeRTOS trace hook implementations
 *
 * This file provides the C-linkage hook functions that are called by
 * FreeRTOS when trace events occur. These hooks are enabled via the
 * CONFIG_FREERTOS_USE_TRACE_FACILITY Kconfig option.
 *
 * Note: ESP-IDF's FreeRTOS port calls these hooks automatically when
 * the trace facility is enabled. The hooks are defined as weak symbols
 * in the FreeRTOS port, allowing us to override them here.
 */

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "traceEvent.hpp"
#include "traceRecorder.hpp"

namespace {

/**
 * @brief Create a kernel trace event
 *
 * Helper to create trace events for FreeRTOS hooks.
 * Called from ISR context, must be fast.
 */
inline domes::trace::TraceEvent makeKernelEvent(domes::trace::TraceEventType type, uint16_t taskId,
                                                uint32_t arg1 = 0, uint32_t arg2 = 0) {
    domes::trace::TraceEvent event;
    event.timestamp = static_cast<uint32_t>(esp_timer_get_time());
    event.taskId = taskId;
    event.eventType = static_cast<uint8_t>(type);
    event.flags = static_cast<uint8_t>(domes::trace::TraceCategory::kKernel) << 4;
    event.arg1 = arg1;
    event.arg2 = arg2;
    return event;
}

}  // namespace

// ESP-IDF FreeRTOS hooks are called via trace macros defined in FreeRTOSConfig.h
// We implement the actual tracing functions that can be called from those macros

extern "C" {

/**
 * @brief Called when a task is switched in (started running)
 *
 * This is called from the scheduler, in ISR context.
 */
void domes_trace_task_switched_in(void) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(task));

    domes::trace::Recorder::recordFromIsr(
        makeKernelEvent(domes::trace::TraceEventType::kTaskSwitchIn, taskId));
}

/**
 * @brief Called when a task is switched out (stopped running)
 *
 * This is called from the scheduler, in ISR context.
 */
void domes_trace_task_switched_out(void) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(task));

    domes::trace::Recorder::recordFromIsr(
        makeKernelEvent(domes::trace::TraceEventType::kTaskSwitchOut, taskId));
}

/**
 * @brief Called when entering an ISR
 *
 * @param isrId ISR identifier (interrupt number)
 */
void domes_trace_isr_enter(uint32_t isrId) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    // ISRs are tracked on task ID 0 (reserved for ISR context)
    domes::trace::Recorder::recordFromIsr(
        makeKernelEvent(domes::trace::TraceEventType::kIsrEnter, 0, isrId));
}

/**
 * @brief Called when exiting an ISR
 *
 * @param isrId ISR identifier (interrupt number)
 */
void domes_trace_isr_exit(uint32_t isrId) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    domes::trace::Recorder::recordFromIsr(
        makeKernelEvent(domes::trace::TraceEventType::kIsrExit, 0, isrId));
}

/**
 * @brief Called when a task is created
 *
 * @param taskHandle Handle of the newly created task
 */
void domes_trace_task_create(TaskHandle_t taskHandle) {
    if (!domes::trace::Recorder::isEnabled() || taskHandle == nullptr) {
        return;
    }

    uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(taskHandle));

    // Get current task ID for context
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    uint16_t currentTaskId =
        currentTask ? static_cast<uint16_t>(uxTaskGetTaskNumber(currentTask)) : 0;

    domes::trace::Recorder::record(makeKernelEvent(domes::trace::TraceEventType::kTaskCreate,
                                                   currentTaskId,
                                                   taskId  // New task ID in arg1
                                                   ));
}

/**
 * @brief Called when a task is deleted
 *
 * @param taskHandle Handle of the task being deleted
 */
void domes_trace_task_delete(TaskHandle_t taskHandle) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    uint16_t taskId = taskHandle ? static_cast<uint16_t>(uxTaskGetTaskNumber(taskHandle)) : 0;

    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    uint16_t currentTaskId =
        currentTask ? static_cast<uint16_t>(uxTaskGetTaskNumber(currentTask)) : 0;

    domes::trace::Recorder::record(makeKernelEvent(domes::trace::TraceEventType::kTaskDelete,
                                                   currentTaskId,
                                                   taskId  // Deleted task ID in arg1
                                                   ));
}

/**
 * @brief Called when a queue send operation completes
 *
 * @param queueHandle Handle of the queue
 */
void domes_trace_queue_send(void* queueHandle) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    uint16_t taskId = task ? static_cast<uint16_t>(uxTaskGetTaskNumber(task)) : 0;

    domes::trace::Recorder::record(makeKernelEvent(domes::trace::TraceEventType::kQueueSend, taskId,
                                                   reinterpret_cast<uint32_t>(queueHandle)));
}

/**
 * @brief Called when a queue receive operation completes
 *
 * @param queueHandle Handle of the queue
 */
void domes_trace_queue_receive(void* queueHandle) {
    if (!domes::trace::Recorder::isEnabled()) {
        return;
    }

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    uint16_t taskId = task ? static_cast<uint16_t>(uxTaskGetTaskNumber(task)) : 0;

    domes::trace::Recorder::record(makeKernelEvent(domes::trace::TraceEventType::kQueueReceive,
                                                   taskId,
                                                   reinterpret_cast<uint32_t>(queueHandle)));
}

}  // extern "C"
