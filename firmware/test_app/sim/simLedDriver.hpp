#pragma once

#include "interfaces/iLedDriver.hpp"
#include "sim/simLog.hpp"

#include <sstream>

namespace sim {

class SimLedDriver : public domes::ILedDriver {
public:
    SimLedDriver(uint16_t podId, SimLog& log)
        : podId_(podId), log_(log) {}

    esp_err_t init() override { return ESP_OK; }

    esp_err_t setPixel(uint8_t index, domes::Color color) override {
        if (index >= kLedCount) return ESP_ERR_INVALID_ARG;
        pixels_[index] = color;
        return ESP_OK;
    }

    esp_err_t setAll(domes::Color color) override {
        for (auto& p : pixels_) p = color;
        lastColor_ = color;
        std::ostringstream oss;
        oss << "setAll rgb(" << (int)color.r << "," << (int)color.g << "," << (int)color.b << ")";
        log_.log(podId_, "led", oss.str());
        return ESP_OK;
    }

    esp_err_t clear() override {
        for (auto& p : pixels_) p = domes::Color::off();
        log_.log(podId_, "led", "clear");
        return ESP_OK;
    }

    esp_err_t refresh() override {
        refreshCount_++;
        log_.log(podId_, "led", "refresh");
        return ESP_OK;
    }

    void setBrightness(uint8_t brightness) override {
        brightness_ = brightness;
    }

    uint8_t getLedCount() const override { return kLedCount; }

    // Test accessors
    domes::Color lastColor() const { return lastColor_; }
    int refreshCount() const { return refreshCount_; }
    domes::Color pixel(uint8_t index) const {
        if (index < kLedCount) return pixels_[index];
        return domes::Color::off();
    }

private:
    static constexpr uint8_t kLedCount = 16;
    uint16_t podId_;
    SimLog& log_;
    domes::Color pixels_[kLedCount] = {};
    domes::Color lastColor_ = domes::Color::off();
    int refreshCount_ = 0;
    uint8_t brightness_ = 255;
};

}  // namespace sim
