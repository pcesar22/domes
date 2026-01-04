/**
 * @file freertos/task.h
 * @brief Mock FreeRTOS task header for host testing
 */

#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Task function type */
typedef void (*TaskFunction_t)(void*);

/* Mock task functions */
static inline void vTaskDelay(TickType_t ticks) {
    (void)ticks;
}

static inline TaskHandle_t xTaskCreateStatic(
    TaskFunction_t func,
    const char* name,
    uint32_t stackSize,
    void* params,
    UBaseType_t priority,
    StackType_t* stack,
    StaticTask_t* tcb
) {
    (void)func;
    (void)name;
    (void)stackSize;
    (void)params;
    (void)priority;
    (void)stack;
    (void)tcb;
    return (TaskHandle_t)0x12345678;
}

static inline BaseType_t xTaskCreate(
    TaskFunction_t func,
    const char* name,
    uint32_t stackSize,
    void* params,
    UBaseType_t priority,
    TaskHandle_t* handle
) {
    (void)func;
    (void)name;
    (void)stackSize;
    (void)params;
    (void)priority;
    if (handle) *handle = (TaskHandle_t)0x12345678;
    return pdPASS;
}

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_TASK_H */
