#pragma once

#include "freertos/FreeRTOS.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>

struct StaticSemaphore_t {
    std::mutex mutex;
    std::condition_variable condition;
    uint32_t count = 1;
    uint32_t maximum = 1;
    bool mutexKind = true;
    bool dynamicallyAllocated = false;
};

using SemaphoreHandle_t = StaticSemaphore_t*;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
    storage->count = 1;
    storage->maximum = 1;
    storage->mutexKind = true;
    return storage;
}

inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t* storage) {
    storage->count = 0;
    storage->maximum = 1;
    storage->mutexKind = false;
    return storage;
}

inline SemaphoreHandle_t xSemaphoreCreateCounting(uint32_t maximum, uint32_t initial) {
    auto* semaphore = new StaticSemaphore_t;
    semaphore->count = initial;
    semaphore->maximum = maximum;
    semaphore->mutexKind = false;
    semaphore->dynamicallyAllocated = true;
    return semaphore;
}

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return xSemaphoreCreateCounting(1, 0);
}

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto* semaphore = xSemaphoreCreateCounting(1, 1);
    semaphore->mutexKind = true;
    return semaphore;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t) {
    if (!semaphore) {
        return pdFALSE;
    }
    std::unique_lock lock(semaphore->mutex);
    if (semaphore->mutexKind) {
        semaphore->condition.wait(lock, [semaphore] { return semaphore->count > 0; });
    }
    if (semaphore->count == 0) {
        return pdFALSE;
    }
    --semaphore->count;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    if (!semaphore) {
        return pdFALSE;
    }
    std::lock_guard lock(semaphore->mutex);
    if (semaphore->count == semaphore->maximum) {
        return pdFALSE;
    }
    ++semaphore->count;
    semaphore->condition.notify_one();
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) {
    if (semaphore && semaphore->dynamicallyAllocated) {
        delete semaphore;
    }
}
