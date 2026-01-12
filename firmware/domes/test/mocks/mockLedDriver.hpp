#pragma once

/**
 * @file mockLedDriver.hpp
 * @brief Mock LED driver for unit testing
 *
 * Provides controllable LED behavior for testing services
 * that depend on LED output.
 */

#include "interfaces/iLedDriver.hpp"

#include <array>
#include <cstdint>

namespace domes {

/**
 * @brief Mock LED driver for unit testing
 *
 * Allows tests to verify LED operations and inspect state.
 *
 * @code
 * MockLedDriver mockLed;
 * mockLed.initReturnValue = ESP_OK;
 *
 * SomeService service(mockLed);
 * service.showFeedback();
 *
 * TEST_ASSERT_EQUAL(Color::green(), mockLed.pixels[0]);
 * TEST_ASSERT_TRUE(mockLed.refreshCalled);
 * @endcode
 */
class MockLedDriver : public ILedDriver {
public:
    static constexpr uint8_t kMaxLeds = 32;

    MockLedDriver(uint8_t ledCount = 16) : ledCount_(ledCount) {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        initCalled = false;
        setPixelCalled = false;
        setAllCalled = false;
        clearCalled = false;
        refreshCalled = false;
        setBrightnessCalled = false;

        initReturnValue = ESP_OK;
        setPixelReturnValue = ESP_OK;
        setAllReturnValue = ESP_OK;
        clearReturnValue = ESP_OK;
        refreshReturnValue = ESP_OK;

        initialized_ = false;
        brightness_ = 255;
        refreshCount = 0;

        for (auto& pixel : pixels) {
            pixel = Color::off();
        }
    }

    // ILedDriver implementation
    esp_err_t init() override {
        initCalled = true;
        if (initReturnValue == ESP_OK) {
            initialized_ = true;
        }
        return initReturnValue;
    }

    esp_err_t setPixel(uint8_t index, Color color) override {
        setPixelCalled = true;
        lastPixelIndex = index;
        lastPixelColor = color;

        if (index >= ledCount_) {
            return ESP_ERR_INVALID_ARG;
        }
        if (setPixelReturnValue == ESP_OK) {
            pixels[index] = color;
        }
        return setPixelReturnValue;
    }

    esp_err_t setAll(Color color) override {
        setAllCalled = true;
        lastSetAllColor = color;

        if (setAllReturnValue == ESP_OK) {
            for (uint8_t i = 0; i < ledCount_; ++i) {
                pixels[i] = color;
            }
        }
        return setAllReturnValue;
    }

    esp_err_t clear() override {
        clearCalled = true;

        if (clearReturnValue == ESP_OK) {
            for (uint8_t i = 0; i < ledCount_; ++i) {
                pixels[i] = Color::off();
            }
        }
        return clearReturnValue;
    }

    esp_err_t refresh() override {
        refreshCalled = true;
        refreshCount++;

        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }
        return refreshReturnValue;
    }

    void setBrightness(uint8_t brightness) override {
        setBrightnessCalled = true;
        brightness_ = brightness;
        lastBrightness = brightness;
    }

    uint8_t getLedCount() const override {
        return ledCount_;
    }

    // Test inspection - method calls
    bool initCalled;
    bool setPixelCalled;
    bool setAllCalled;
    bool clearCalled;
    bool refreshCalled;
    bool setBrightnessCalled;

    // Test control - return values
    esp_err_t initReturnValue;
    esp_err_t setPixelReturnValue;
    esp_err_t setAllReturnValue;
    esp_err_t clearReturnValue;
    esp_err_t refreshReturnValue;

    // Test inspection - captured arguments
    uint8_t lastPixelIndex = 0;
    Color lastPixelColor;
    Color lastSetAllColor;
    uint8_t lastBrightness = 255;

    // Test inspection - state
    std::array<Color, kMaxLeds> pixels;
    uint32_t refreshCount;

private:
    uint8_t ledCount_;
    uint8_t brightness_ = 255;
    bool initialized_ = false;
};

}  // namespace domes
