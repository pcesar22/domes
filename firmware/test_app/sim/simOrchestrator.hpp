#pragma once

#include "sim/simLog.hpp"
#include "sim/podInstance.hpp"
#include "esp_timer.h"

#include <memory>
#include <vector>

namespace sim {

class SimOrchestrator {
public:
    PodInstance& addPod(uint16_t podId) {
        pods_.push_back(std::make_unique<PodInstance>(podId, log_));
        return *pods_.back();
    }

    void tickAll() {
        for (auto& pod : pods_) {
            pod->tick();
        }
    }

    void advanceTimeMs(int64_t ms) {
        test_stubs::mock_time_us.fetch_add(ms * 1000);
    }

    void advanceTimeUs(int64_t us) {
        test_stubs::mock_time_us.fetch_add(us);
    }

    SimLog& log() { return log_; }
    PodInstance& pod(size_t index) { return *pods_[index]; }
    size_t podCount() const { return pods_.size(); }

private:
    std::vector<std::unique_ptr<PodInstance>> pods_;
    SimLog log_;
};

}  // namespace sim
