/**
 * @file memoryProfiler.cpp
 * @brief Memory profiler implementation
 */

#include "memoryProfiler.hpp"

#include "trace/traceApi.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <algorithm>

namespace {
constexpr const char* kTag = "mem_prof";
constexpr size_t kTaskStackSize = 2048;
constexpr UBaseType_t kTaskPriority = 1;  // Low priority
}

namespace domes::infra {

std::array<HeapSample, kMaxHeapSamples> MemoryProfiler::samples_ = {};
size_t MemoryProfiler::writeIndex_ = 0;
size_t MemoryProfiler::count_ = 0;
uint32_t MemoryProfiler::intervalS_ = kDefaultSampleIntervalS;
bool MemoryProfiler::initialized_ = false;

esp_err_t MemoryProfiler::init(uint32_t intervalS) {
    if (initialized_) {
        return ESP_OK;
    }

    intervalS_ = (intervalS > 0) ? intervalS : kDefaultSampleIntervalS;
    writeIndex_ = 0;
    count_ = 0;

    initialized_ = true;
    ESP_LOGI(kTag, "Memory profiler initialized (interval: %lus)",
             static_cast<unsigned long>(intervalS_));
    return ESP_OK;
}

void MemoryProfiler::startTask() {
    if (!initialized_) {
        return;
    }

    xTaskCreatePinnedToCore(
        taskFunc,
        "mem_prof",
        kTaskStackSize,
        nullptr,
        kTaskPriority,
        nullptr,
        tskNO_AFFINITY
    );
}

size_t MemoryProfiler::sampleCount() {
    return count_;
}

size_t MemoryProfiler::getSamples(HeapSample* out, size_t maxCount) {
    size_t toRead = std::min(count_, maxCount);
    if (toRead == 0) {
        return 0;
    }

    // Read from oldest to newest
    size_t startIdx;
    if (count_ >= kMaxHeapSamples) {
        // Buffer is full, oldest is at writeIndex
        startIdx = writeIndex_;
    } else {
        // Buffer not full, oldest is at 0
        startIdx = 0;
    }

    for (size_t i = 0; i < toRead; ++i) {
        size_t idx = (startIdx + i) % kMaxHeapSamples;
        out[i] = samples_[idx];
    }

    return toRead;
}

HeapSample MemoryProfiler::currentStats() {
    HeapSample sample;
    sample.timestampS = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);
    sample.freeHeap = static_cast<uint32_t>(esp_get_free_heap_size());
    sample.largestBlock = static_cast<uint32_t>(
        heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    sample.minFreeHeap = static_cast<uint32_t>(esp_get_minimum_free_heap_size());
    return sample;
}

uint32_t MemoryProfiler::totalHeapSize() {
    return static_cast<uint32_t>(heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
}

void MemoryProfiler::taskFunc(void* param) {
    (void)param;

    ESP_LOGI(kTag, "Memory profiler task started");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(intervalS_ * 1000));

        // Take a sample
        HeapSample sample = currentStats();

        // Store in circular buffer
        samples_[writeIndex_] = sample;
        writeIndex_ = (writeIndex_ + 1) % kMaxHeapSamples;
        if (count_ < kMaxHeapSamples) {
            count_++;
        }

        // Emit trace counters for Perfetto
        TRACE_COUNTER(TRACE_ID("Heap.Free"), sample.freeHeap,
                      domes::trace::Category::kKernel);
        TRACE_COUNTER(TRACE_ID("Heap.LargestBlock"), sample.largestBlock,
                      domes::trace::Category::kKernel);
        TRACE_COUNTER(TRACE_ID("Heap.MinFree"), sample.minFreeHeap,
                      domes::trace::Category::kKernel);
    }
}

}  // namespace domes::infra
