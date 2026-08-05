#pragma once

#include "infra/taskConfig.hpp"

#include <atomic>
#include <cstdint>

namespace domes::infra {

/** Task-entry handshake with duplicate detection; this is not a scheduler trace. */
class TaskStartEvidence {
public:
    static void markStarted(const TaskConfig& config) { markStarted(config.evidenceMask); }
    static void markStarted(uint32_t evidenceMask);

    static uint32_t startedMask() { return startedMask_.load(std::memory_order_acquire); }
    static uint32_t duplicateMask() { return duplicateMask_.load(std::memory_order_acquire); }
    static uint32_t coreMask(uint8_t core);

private:
    static std::atomic<uint32_t> startedMask_;
    static std::atomic<uint32_t> duplicateMask_;
    static std::atomic<uint32_t> core0Mask_;
    static std::atomic<uint32_t> core1Mask_;
};

}  // namespace domes::infra
