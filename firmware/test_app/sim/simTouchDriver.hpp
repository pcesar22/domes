#pragma once

#include "interfaces/iTouchDriver.hpp"

namespace sim {

class SimTouchDriver : public domes::ITouchDriver {
public:
    esp_err_t init() override { return ESP_OK; }

    esp_err_t update() override {
        updateCount_++;
        return ESP_OK;
    }

    bool isTouched(uint8_t padIndex) const override {
        if (padIndex < kPadCount) {
            return touchState_[padIndex];
        }
        return false;
    }

    domes::TouchPadState getPadState(uint8_t padIndex) const override {
        domes::TouchPadState state;
        if (padIndex < kPadCount) {
            state.touched = touchState_[padIndex];
        }
        return state;
    }

    uint8_t getPadCount() const override { return kPadCount; }

    esp_err_t calibrate() override { return ESP_OK; }

    // Test helpers
    void setTouched(uint8_t padIndex, bool touched) {
        if (padIndex < kPadCount) {
            touchState_[padIndex] = touched;
        }
    }

    void clearAll() {
        for (auto& s : touchState_) s = false;
    }

    int updateCount() const { return updateCount_; }

private:
    static constexpr uint8_t kPadCount = 4;
    bool touchState_[kPadCount] = {};
    int updateCount_ = 0;
};

}  // namespace sim
