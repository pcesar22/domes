#include "taskStartEvidence.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace domes::infra {

std::atomic<uint32_t> TaskStartEvidence::startedMask_{0};
std::atomic<uint32_t> TaskStartEvidence::duplicateMask_{0};
std::atomic<uint32_t> TaskStartEvidence::core0Mask_{0};
std::atomic<uint32_t> TaskStartEvidence::core1Mask_{0};

void TaskStartEvidence::markStarted(uint32_t evidenceMask) {
    const uint32_t previous = startedMask_.fetch_or(evidenceMask, std::memory_order_acq_rel);
    duplicateMask_.fetch_or(previous & evidenceMask, std::memory_order_acq_rel);
    const BaseType_t core = xPortGetCoreID();
    if (core == 0) {
        core0Mask_.fetch_or(evidenceMask, std::memory_order_acq_rel);
    } else if (core == 1) {
        core1Mask_.fetch_or(evidenceMask, std::memory_order_acq_rel);
    }
}

uint32_t TaskStartEvidence::coreMask(uint8_t core) {
    if (core == 0) {
        return core0Mask_.load(std::memory_order_acquire);
    }
    return core == 1 ? core1Mask_.load(std::memory_order_acquire) : 0;
}

}  // namespace domes::infra
