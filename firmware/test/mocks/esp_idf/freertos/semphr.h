/**
 * @file freertos/semphr.h
 * @brief Mock FreeRTOS semaphore header for host testing
 */

#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mock semaphore functions */
static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* buffer) {
    (void)buffer;
    return (SemaphoreHandle_t)0x12345678;  /* Return dummy handle */
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
    (void)sem;
    (void)timeout;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    (void)sem;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t* woken) {
    (void)sem;
    if (woken) *woken = pdFALSE;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* woken) {
    (void)sem;
    if (woken) *woken = pdFALSE;
    return pdTRUE;
}

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_SEMPHR_H */
