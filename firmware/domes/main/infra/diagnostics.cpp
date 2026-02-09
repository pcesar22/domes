/**
 * @file diagnostics.cpp
 * @brief Runtime diagnostics implementation
 */

#include "infra/diagnostics.hpp"
#include "infra/taskConfig.hpp"
#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char* kTag = "diag";

namespace domes::infra {

// Static member definitions
std::atomic<uint32_t> Diagnostics::crcErrors_{0};
std::atomic<uint32_t> Diagnostics::lengthErrors_{0};
std::atomic<uint32_t> Diagnostics::frameTimeouts_{0};
bool Diagnostics::initialized_ = false;

void Diagnostics::init() {
    if (initialized_) return;
    initialized_ = true;
    ESP_LOGI(kTag, "Diagnostics initialized");
}

void Diagnostics::startTask() {
    if (!initialized_) {
        ESP_LOGW(kTag, "Diagnostics not initialized, skipping task start");
        return;
    }

    xTaskCreatePinnedToCore(
        taskFunc,
        "diagnostics",
        3072,  // Stack size
        nullptr,
        priority::kIdle,  // Lowest priority - background task
        nullptr,
        core::kAny
    );

    ESP_LOGI(kTag, "Diagnostics task started");
}

void Diagnostics::taskFunc(void* /*param*/) {
    // Delay initial report to let system settle
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (true) {
        // Heap metrics
        uint32_t freeHeap = static_cast<uint32_t>(esp_get_free_heap_size());
        uint32_t minHeap = static_cast<uint32_t>(esp_get_minimum_free_heap_size());

        TRACE_COUNTER(TRACE_ID("Diag.FreeHeap"), freeHeap, domes::trace::Category::kKernel);
        TRACE_COUNTER(TRACE_ID("Diag.MinHeap"), minHeap, domes::trace::Category::kKernel);

        ESP_LOGI(kTag, "Heap: free=%lu min=%lu",
                 static_cast<unsigned long>(freeHeap),
                 static_cast<unsigned long>(minHeap));

        // Protocol error counters
        uint32_t crc = crcErrors_.load(std::memory_order_relaxed);
        uint32_t len = lengthErrors_.load(std::memory_order_relaxed);
        uint32_t timeout = frameTimeouts_.load(std::memory_order_relaxed);

        if (crc > 0 || len > 0 || timeout > 0) {
            TRACE_COUNTER(TRACE_ID("Diag.CrcErrors"), crc, domes::trace::Category::kTransport);
            TRACE_COUNTER(TRACE_ID("Diag.LengthErrors"), len, domes::trace::Category::kTransport);
            TRACE_COUNTER(TRACE_ID("Diag.FrameTimeouts"), timeout, domes::trace::Category::kTransport);

            ESP_LOGW(kTag, "Frame errors: crc=%lu len=%lu timeout=%lu",
                     static_cast<unsigned long>(crc),
                     static_cast<unsigned long>(len),
                     static_cast<unsigned long>(timeout));
        }

        // Stack watermarks for key tasks
        TaskHandle_t task;

        task = xTaskGetHandle("serial_ota");
        if (task) {
            uint32_t watermark = uxTaskGetStackHighWaterMark(task);
            TRACE_COUNTER(TRACE_ID("Diag.Stack.SerialOta"), watermark, domes::trace::Category::kKernel);
        }

        task = xTaskGetHandle("ble_ota");
        if (task) {
            uint32_t watermark = uxTaskGetStackHighWaterMark(task);
            TRACE_COUNTER(TRACE_ID("Diag.Stack.BleOta"), watermark, domes::trace::Category::kKernel);
        }

        task = xTaskGetHandle("game_tick");
        if (task) {
            uint32_t watermark = uxTaskGetStackHighWaterMark(task);
            TRACE_COUNTER(TRACE_ID("Diag.Stack.GameTick"), watermark, domes::trace::Category::kKernel);
        }

        task = xTaskGetHandle("led_svc");
        if (task) {
            uint32_t watermark = uxTaskGetStackHighWaterMark(task);
            TRACE_COUNTER(TRACE_ID("Diag.Stack.LedSvc"), watermark, domes::trace::Category::kKernel);
        }

        task = xTaskGetHandle("touch_svc");
        if (task) {
            uint32_t watermark = uxTaskGetStackHighWaterMark(task);
            TRACE_COUNTER(TRACE_ID("Diag.Stack.TouchSvc"), watermark, domes::trace::Category::kKernel);
        }

        // Report every 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

}  // namespace domes::infra
