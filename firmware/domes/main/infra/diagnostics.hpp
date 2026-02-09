#pragma once

/**
 * @file diagnostics.hpp
 * @brief Runtime diagnostics for heap, stack, and protocol errors
 *
 * Low-priority FreeRTOS task that periodically reports system health
 * via trace counters. Also provides atomic error counters for frame
 * decode errors.
 */

#include "trace/traceApi.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cstdint>

namespace domes::infra {

class Diagnostics {
public:
    /// Initialize diagnostics (call once at startup)
    static void init();

    /// Start periodic reporting task
    static void startTask();

    /// Record a CRC error in frame decoding
    static void recordCrcError() { crcErrors_.fetch_add(1, std::memory_order_relaxed); }

    /// Record a length error in frame decoding
    static void recordLengthError() { lengthErrors_.fetch_add(1, std::memory_order_relaxed); }

    /// Record a frame timeout
    static void recordFrameTimeout() { frameTimeouts_.fetch_add(1, std::memory_order_relaxed); }

    /// Get error counts
    static uint32_t crcErrors() { return crcErrors_.load(std::memory_order_relaxed); }
    static uint32_t lengthErrors() { return lengthErrors_.load(std::memory_order_relaxed); }
    static uint32_t frameTimeouts() { return frameTimeouts_.load(std::memory_order_relaxed); }

private:
    static void taskFunc(void* param);

    static std::atomic<uint32_t> crcErrors_;
    static std::atomic<uint32_t> lengthErrors_;
    static std::atomic<uint32_t> frameTimeouts_;
    static bool initialized_;
};

}  // namespace domes::infra
