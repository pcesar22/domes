#pragma once

/**
 * @file memoryProfiler.hpp
 * @brief Periodic heap sampling with circular buffer and trace counters
 *
 * Samples free heap, largest block, and min free heap every N seconds.
 * Stores last 60 samples (5 minutes at 5s interval) in a circular buffer.
 * Also records trace counter events for Perfetto visualization.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace domes::infra {

/// Default sampling interval in seconds
constexpr uint32_t kDefaultSampleIntervalS = 5;

/// Number of samples in circular buffer (5 min at 5s)
constexpr size_t kMaxHeapSamples = 60;

/**
 * @brief Single heap sample
 */
struct HeapSample {
    uint32_t timestampS = 0;     ///< Uptime when sampled (seconds)
    uint32_t freeHeap = 0;       ///< Free heap bytes
    uint32_t largestBlock = 0;   ///< Largest free contiguous block
    uint32_t minFreeHeap = 0;    ///< Historical minimum free heap
};

/**
 * @brief Memory profiler with periodic sampling
 *
 * Runs as a FreeRTOS task that periodically samples heap statistics
 * and stores them in a circular buffer for CLI retrieval.
 */
class MemoryProfiler {
public:
    /**
     * @brief Initialize the memory profiler
     *
     * @param intervalS Sampling interval in seconds (default 5)
     * @return ESP_OK on success
     */
    static esp_err_t init(uint32_t intervalS = kDefaultSampleIntervalS);

    /**
     * @brief Start the sampling task
     */
    static void startTask();

    /**
     * @brief Get the current number of samples stored
     */
    static size_t sampleCount();

    /**
     * @brief Get samples from the circular buffer
     *
     * @param out Output array to fill
     * @param maxCount Maximum samples to return
     * @return Number of samples copied
     */
    static size_t getSamples(HeapSample* out, size_t maxCount);

    /**
     * @brief Get current heap statistics (immediate, not from buffer)
     */
    static HeapSample currentStats();

    /**
     * @brief Get total heap size
     */
    static uint32_t totalHeapSize();

private:
    static void taskFunc(void* param);

    static std::array<HeapSample, kMaxHeapSamples> samples_;
    static size_t writeIndex_;
    static size_t count_;
    static uint32_t intervalS_;
    static bool initialized_;
    static portMUX_TYPE spinlock_;
};

}  // namespace domes::infra
