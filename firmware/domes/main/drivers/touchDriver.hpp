#pragma once

#include "driver/touch_pad.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "interfaces/iTouchDriver.hpp"

#include <array>
#include <cstdint>

namespace domes {

/**
 * @brief Touch driver using ESP32-S3's capacitive touch sensor peripheral
 *
 * Supports up to 4 touch pads. Uses ESP-IDF legacy touch_pad driver.
 *
 * @tparam kNumPads Number of touch pads to configure (max 4)
 */
template <uint8_t kNumPads>
class TouchDriver : public ITouchDriver {
    static_assert(kNumPads <= 4, "Maximum 4 touch pads supported");

public:
    /**
     * @brief Construct touch driver
     *
     * @param pins Array of GPIO pins for each touch pad
     */
    explicit TouchDriver(const std::array<gpio_num_t, kNumPads>& pins)
        : pins_(pins), initialized_(false) {
        states_.fill({});
        touchChannels_.fill(TOUCH_PAD_MAX);
    }

    ~TouchDriver() override {
        if (initialized_) {
            touch_pad_deinit();
        }
    }

    // Non-copyable
    TouchDriver(const TouchDriver&) = delete;
    TouchDriver& operator=(const TouchDriver&) = delete;

    esp_err_t init() override {
        if (initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(kTag, "Initializing touch driver with %d pads", kNumPads);

        // Initialize touch pad peripheral
        esp_err_t err = touch_pad_init();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "touch_pad_init failed: %s", esp_err_to_name(err));
            return err;
        }

        // Set voltage reference for touch sensor
        err = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "touch_pad_set_voltage failed: %s", esp_err_to_name(err));
            return err;
        }

        // Configure each touch channel
        for (uint8_t i = 0; i < kNumPads; i++) {
            touch_pad_t channel = gpioToTouchChannel(pins_[i]);
            if (channel == TOUCH_PAD_MAX) {
                ESP_LOGE(kTag, "Invalid touch GPIO: %d", pins_[i]);
                return ESP_ERR_INVALID_ARG;
            }

            touchChannels_[i] = channel;

            err = touch_pad_config(channel);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "touch_pad_config failed for channel %d: %s", channel, esp_err_to_name(err));
                return err;
            }

            ESP_LOGI(kTag, "Touch pad %d: GPIO%d -> channel %d", i, pins_[i], channel);
        }

        // Denoise feature (ESP32-S3 specific)
        touch_pad_denoise_t denoise = {
            .grade = TOUCH_PAD_DENOISE_BIT4,
            .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
        };
        err = touch_pad_denoise_set_config(&denoise);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "touch_pad_denoise_set_config failed (non-fatal): %s", esp_err_to_name(err));
        }
        touch_pad_denoise_enable();

        // Set FSM mode to timer-triggered
        err = touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "touch_pad_set_fsm_mode failed: %s", esp_err_to_name(err));
            return err;
        }

        // Start the FSM
        err = touch_pad_fsm_start();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "touch_pad_fsm_start failed: %s", esp_err_to_name(err));
            return err;
        }

        initialized_ = true;

        // Wait for initial readings to stabilize
        vTaskDelay(pdMS_TO_TICKS(200));

        // Initial calibration
        return calibrate();
    }

    esp_err_t update() override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        for (uint8_t i = 0; i < kNumPads; i++) {
            uint32_t rawValue = 0;
            uint32_t benchmark = 0;

            // Read both raw and benchmark values
            touch_pad_read_raw_data(touchChannels_[i], &rawValue);
            touch_pad_read_benchmark(touchChannels_[i], &benchmark);

            states_[i].rawValue = rawValue;

            // Touch detected when raw value differs significantly from benchmark
            // The benchmark tracks the "untouched" baseline automatically
            if (benchmark > 0) {
                // If raw is significantly higher than benchmark, touch detected
                uint32_t diff = (rawValue > benchmark) ? (rawValue - benchmark) : 0;
                uint32_t diffThreshold = benchmark / 10;  // 10% change threshold
                states_[i].touched = (diff > diffThreshold);
                states_[i].threshold = benchmark + diffThreshold;  // For logging
            }
        }

        return ESP_OK;
    }

    bool isTouched(uint8_t padIndex) const override {
        if (padIndex >= kNumPads) {
            return false;
        }
        return states_[padIndex].touched;
    }

    TouchPadState getPadState(uint8_t padIndex) const override {
        if (padIndex >= kNumPads) {
            return {};
        }
        return states_[padIndex];
    }

    uint8_t getPadCount() const override { return kNumPads; }

    esp_err_t calibrate() override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(kTag, "Calibrating touch pads (using benchmark)...");

        // Read benchmark values which auto-track untouched baseline
        for (uint8_t i = 0; i < kNumPads; i++) {
            uint32_t rawValue = 0;
            uint32_t benchmark = 0;

            touch_pad_read_raw_data(touchChannels_[i], &rawValue);
            touch_pad_read_benchmark(touchChannels_[i], &benchmark);

            states_[i].rawValue = rawValue;
            states_[i].threshold = benchmark + (benchmark / 10);  // 10% above benchmark

            ESP_LOGI(kTag, "Pad %d: raw=%lu, benchmark=%lu, threshold=%lu",
                     i, rawValue, benchmark, states_[i].threshold);
        }

        return ESP_OK;
    }

private:
    static constexpr const char* kTag = "TouchDriver";
    static constexpr uint32_t kThresholdPercent = 120;  // Touch threshold as % of baseline (ESP32-S3 value rises on touch)

    /**
     * @brief Convert GPIO number to ESP32-S3 touch pad channel
     *
     * ESP32-S3 touch channel mapping (from datasheet):
     * TOUCH_PAD_NUM1 = GPIO1, TOUCH_PAD_NUM2 = GPIO2, etc.
     */
    static touch_pad_t gpioToTouchChannel(gpio_num_t gpio) {
        switch (gpio) {
            case GPIO_NUM_1:  return TOUCH_PAD_NUM1;
            case GPIO_NUM_2:  return TOUCH_PAD_NUM2;
            case GPIO_NUM_3:  return TOUCH_PAD_NUM3;
            case GPIO_NUM_4:  return TOUCH_PAD_NUM4;
            case GPIO_NUM_5:  return TOUCH_PAD_NUM5;
            case GPIO_NUM_6:  return TOUCH_PAD_NUM6;
            case GPIO_NUM_7:  return TOUCH_PAD_NUM7;
            case GPIO_NUM_8:  return TOUCH_PAD_NUM8;
            case GPIO_NUM_9:  return TOUCH_PAD_NUM9;
            case GPIO_NUM_10: return TOUCH_PAD_NUM10;
            case GPIO_NUM_11: return TOUCH_PAD_NUM11;
            case GPIO_NUM_12: return TOUCH_PAD_NUM12;
            case GPIO_NUM_13: return TOUCH_PAD_NUM13;
            case GPIO_NUM_14: return TOUCH_PAD_NUM14;
            default:          return TOUCH_PAD_MAX;  // Invalid
        }
    }

    std::array<gpio_num_t, kNumPads> pins_;
    std::array<TouchPadState, kNumPads> states_;
    std::array<touch_pad_t, kNumPads> touchChannels_;
    bool initialized_;
};

}  // namespace domes
