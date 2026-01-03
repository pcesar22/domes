/**
 * @file mockLedDriver.hpp
 * @brief Mock LED driver for unit testing
 */

#pragma once

#include "interfaces/iLedDriver.hpp"
#include <array>

/**
 * @brief Mock LED driver for testing
 *
 * Stores LED state in memory for verification.
 */
class MockLedDriver : public ILedDriver {
public:
    static constexpr uint8_t kMaxLeds = 64;

    MockLedDriver(uint8_t ledCount = 16)
        : ledCount_(ledCount), brightness_(255), showCalled_(false) {
        pixels_.fill(Color::black());
    }

    esp_err_t init() override {
        initCalled_ = true;
        return ESP_OK;
    }

    esp_err_t setPixel(uint8_t index, Color color) override {
        if (index >= ledCount_) {
            return ESP_ERR_INVALID_ARG;
        }
        pixels_[index] = color;
        return ESP_OK;
    }

    esp_err_t fill(Color color) override {
        for (uint8_t i = 0; i < ledCount_; ++i) {
            pixels_[i] = color;
        }
        return ESP_OK;
    }

    esp_err_t clear() override {
        return fill(Color::black());
    }

    esp_err_t show() override {
        showCalled_ = true;
        showCallCount_++;
        return ESP_OK;
    }

    void setBrightness(uint8_t brightness) override {
        brightness_ = brightness;
    }

    uint8_t getLedCount() const override {
        return ledCount_;
    }

    // Test helpers
    bool wasInitCalled() const { return initCalled_; }
    bool wasShowCalled() const { return showCalled_; }
    uint32_t getShowCallCount() const { return showCallCount_; }
    Color getPixel(uint8_t index) const {
        return (index < kMaxLeds) ? pixels_[index] : Color::black();
    }
    uint8_t getBrightness() const { return brightness_; }

    void reset() {
        initCalled_ = false;
        showCalled_ = false;
        showCallCount_ = 0;
        pixels_.fill(Color::black());
    }

private:
    uint8_t ledCount_;
    uint8_t brightness_;
    bool initCalled_ = false;
    bool showCalled_;
    uint32_t showCallCount_ = 0;
    std::array<Color, kMaxLeds> pixels_;
};
