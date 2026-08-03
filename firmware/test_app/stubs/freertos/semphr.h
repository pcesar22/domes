#pragma once

#include "freertos/FreeRTOS.h"

#include <mutex>

struct StaticSemaphore_t {
    std::mutex mutex;
};

using SemaphoreHandle_t = StaticSemaphore_t*;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
    return storage;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t) {
    semaphore->mutex.lock();
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    semaphore->mutex.unlock();
    return pdTRUE;
}
