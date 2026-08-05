#pragma once
#include <cstdint>

#define tskNO_AFFINITY (-1)

namespace sim_trace {
inline uint16_t currentPodId = 0;
inline int32_t currentCoreId = 0;
}  // namespace sim_trace

typedef void* TaskHandle_t;

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    static int dummy;
    return &dummy;
}

inline uint32_t uxTaskGetTaskNumber(TaskHandle_t) {
    return sim_trace::currentPodId;
}

inline int32_t xPortGetCoreID() {
    return sim_trace::currentCoreId;
}

inline void vTaskDelay(uint32_t) {}
