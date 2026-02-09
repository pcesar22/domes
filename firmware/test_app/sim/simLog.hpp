#pragma once

#include "esp_timer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

struct SimLogEntry {
    uint64_t timestampUs;
    uint16_t podId;
    std::string category;
    std::string message;
};

class SimLog {
public:
    void log(uint16_t podId, const std::string& category, const std::string& message) {
        entries_.push_back({
            static_cast<uint64_t>(test_stubs::mock_time_us.load()),
            podId, category, message
        });
    }

    const std::vector<SimLogEntry>& entries() const { return entries_; }

    std::vector<SimLogEntry> filter(const std::string& category) const {
        std::vector<SimLogEntry> result;
        for (const auto& e : entries_) {
            if (e.category == category) result.push_back(e);
        }
        return result;
    }

    std::vector<SimLogEntry> filterByPod(uint16_t podId) const {
        std::vector<SimLogEntry> result;
        for (const auto& e : entries_) {
            if (e.podId == podId) result.push_back(e);
        }
        return result;
    }

    void clear() { entries_.clear(); }

private:
    std::vector<SimLogEntry> entries_;
};

}  // namespace sim
