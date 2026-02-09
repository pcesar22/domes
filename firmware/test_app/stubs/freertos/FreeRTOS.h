#pragma once
#include <cstdint>

typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef uint32_t UBaseType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portYIELD_FROM_ISR(x) (void)(x)
