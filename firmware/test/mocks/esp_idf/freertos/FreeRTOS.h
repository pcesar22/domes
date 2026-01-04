/**
 * @file freertos/FreeRTOS.h
 * @brief Mock FreeRTOS header for host testing
 */

#ifndef FREERTOS_FREERTOS_H
#define FREERTOS_FREERTOS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic FreeRTOS types */
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;

/* Constants */
#define pdTRUE      1
#define pdFALSE     0
#define pdPASS      1
#define pdFAIL      0
#define portMAX_DELAY   0xFFFFFFFF

/* Task macros */
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portYIELD_FROM_ISR(x) (void)(x)

/* Static allocation types */
typedef struct {
    uint32_t dummy;
} StaticTask_t;

typedef struct {
    uint32_t dummy;
} StaticSemaphore_t;

typedef struct {
    uint32_t dummy;
} StaticQueue_t;

typedef uint32_t StackType_t;

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_FREERTOS_H */
