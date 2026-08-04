#pragma once

#include "esp_timer.h"

#include <cstdint>

namespace sim {

class SimClock {
public:
    explicit SimClock(uint64_t timeUs = 0) { reset(timeUs); }

    uint64_t nowUs() const { return nowUs_; }

    void reset(uint64_t timeUs = 0) {
        nowUs_ = timeUs;
        test_stubs::mock_time_us.store(static_cast<int64_t>(timeUs));
    }

    void advanceUs(uint64_t us) {
        nowUs_ += us;
        test_stubs::mock_time_us.store(static_cast<int64_t>(nowUs_));
    }

    void advanceMs(uint64_t ms) { advanceUs(ms * 1000); }

private:
    uint64_t nowUs_ = 0;
};

}  // namespace sim
