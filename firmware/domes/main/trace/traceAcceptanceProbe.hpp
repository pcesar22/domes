#pragma once

#include <cstdint>

namespace domes::trace {

struct TraceAcceptanceResult {
    bool passed = false;
    uint32_t causalId = 0;
    uint32_t eventCount = 0;
    uint32_t droppedCount = 0;
    uint32_t discontinuityCount = 0;
    uint32_t disabledRecordUs = 0;
    uint32_t enabledRecordUs = 0;
};

// Runs one bounded target-timer -> ISR callback -> queue/semaphore -> caller chain.
TraceAcceptanceResult runTraceAcceptanceProbe();

}  // namespace domes::trace
