#pragma once

#include "sim/podInstance.hpp"
#include "sim/simClock.hpp"
#include "sim/simLog.hpp"

#include <memory>
#include <vector>

namespace sim {

class SimOrchestrator {
public:
    SimOrchestrator() : log_(clock_) {}

    PodInstance& addPod(uint16_t podId) {
        pods_.push_back(std::make_unique<PodInstance>(podId, log_));
        return *pods_.back();
    }

    void tickAll() {
        for (auto& pod : pods_) {
            pod->tick();
        }
    }

    void advanceTimeMs(uint64_t ms) { clock_.advanceMs(ms); }

    void advanceTimeUs(uint64_t us) { clock_.advanceUs(us); }

    SimClock& clock() { return clock_; }
    SimLog& log() { return log_; }
    PodInstance& pod(size_t index) { return *pods_[index]; }
    size_t podCount() const { return pods_.size(); }

private:
    std::vector<std::unique_ptr<PodInstance>> pods_;
    SimClock clock_;
    SimLog log_;
};

}  // namespace sim
