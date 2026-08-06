#pragma once
#include <cstdint>
#include <mutex>

typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef uint32_t UBaseType_t;

#define configMAX_PRIORITIES 25
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portYIELD_FROM_ISR(x) (void)(x)
#define taskYIELD() ((void)0)
#define portMAX_DELAY UINT32_MAX
struct portMUX_TYPE {
    std::recursive_mutex mutex;
};

#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL_SAFE(mux) (mux)->mutex.lock()
#define portEXIT_CRITICAL_SAFE(mux) (mux)->mutex.unlock()
