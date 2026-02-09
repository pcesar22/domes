#pragma once

#include "interfaces/iImuDriver.hpp"

namespace sim {

class SimImuDriver : public domes::IImuDriver {
public:
    esp_err_t init() override { return ESP_OK; }
    esp_err_t enableTapDetection(bool /*singleTap*/, bool /*doubleTap*/) override { return ESP_OK; }

    esp_err_t readAccel(domes::AccelData& data) override {
        data = {0.0f, 0.0f, 1.0f};
        return ESP_OK;
    }

    bool isTapDetected() override { return false; }
    esp_err_t clearInterrupt() override { return ESP_OK; }
};

}  // namespace sim
