#pragma once
#include <cstdint>

namespace sim_trace {
inline uint16_t currentPodId = 0;
}

typedef void* TaskHandle_t;

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    static int dummy;
    return &dummy;
}

inline uint32_t uxTaskGetTaskNumber(TaskHandle_t) {
    return sim_trace::currentPodId;
}
