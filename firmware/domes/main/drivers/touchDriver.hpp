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
        baselines_.fill(0);
        lastRawValues_.fill(0);
        stuckCount_.fill(0);
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

            // Read raw value
            touch_pad_read_raw_data(touchChannels_[i], &rawValue);

            states_[i].rawValue = rawValue;

            // Use our calibrated baseline for comparison
            uint32_t baseline = baselines_[i];
            if (baseline == 0) {
                // Not calibrated yet - skip
                states_[i].touched = false;
                continue;
            }

            // Touch detected when raw value is significantly higher than baseline
            // High values during touch are NORMAL - don't treat as saturation
            uint32_t diffThreshold = baseline / 20;  // 5% change threshold
            states_[i].threshold = baseline + diffThreshold;

            if (rawValue > baseline + diffThreshold) {
                states_[i].touched = true;
            } else {
                states_[i].touched = false;
            }

            // Detect STUCK state: exact same high value for many consecutive reads
            // This indicates FSM is frozen, not a normal touch
            if (rawValue == lastRawValues_[i] && rawValue > baseline * 3) {
                stuckCount_[i]++;
                if (stuckCount_[i] > kStuckResetThreshold) {
                    ESP_LOGW(kTag, "Pad %d stuck at %lu, resetting FSM...", i, rawValue);
                    resetFsm();
                    stuckCount_[i] = 0;
                    return ESP_OK;  // Exit early, will re-read next cycle
                }
            } else {
                stuckCount_[i] = 0;
            }

            lastRawValues_[i] = rawValue;
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

        ESP_LOGI(kTag, "Calibrating touch pads (capturing baseline)...");

        // Take multiple readings and average to get stable baseline
        for (uint8_t i = 0; i < kNumPads; i++) {
            uint32_t sum = 0;
            uint8_t validSamples = 0;
            constexpr int kSamples = 10;
            constexpr uint32_t kMaxValidReading = 100000;  // Reject readings above this

            for (int s = 0; s < kSamples; s++) {
                uint32_t rawValue = 0;
                touch_pad_read_raw_data(touchChannels_[i], &rawValue);

                // Only use sane readings for calibration
                if (rawValue > 0 && rawValue < kMaxValidReading) {
                    sum += rawValue;
                    validSamples++;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            if (validSamples > 0) {
                baselines_[i] = sum / validSamples;
                states_[i].rawValue = baselines_[i];
                states_[i].threshold = baselines_[i] + (baselines_[i] / 20);  // 5% above baseline
                states_[i].touched = false;

                ESP_LOGI(kTag, "Pad %d: baseline=%lu, threshold=%lu (%d samples)",
                         i, baselines_[i], states_[i].threshold, validSamples);
            } else {
                // All readings were saturated - keep old baseline or set to 0
                ESP_LOGW(kTag, "Pad %d: all readings saturated, keeping old baseline", i);
            }
        }

        return ESP_OK;
    }

private:
    static constexpr const char* kTag = "TouchDriver";
    static constexpr uint32_t kStuckResetThreshold = 100;  // ~1 second of identical readings at 100Hz

    /**
     * @brief Reset the touch FSM to recover from saturated state
     *
     * This performs a full deinit/reinit of the touch peripheral to
     * clear hardware saturation state.
     */
    void resetFsm() {
        ESP_LOGI(kTag, "Full touch peripheral reset...");

        // Full deinit
        touch_pad_fsm_stop();
        touch_pad_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));

        // Full reinit
        touch_pad_init();
        touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

        // Reconfigure all channels
        for (uint8_t i = 0; i < kNumPads; i++) {
            touch_pad_config(touchChannels_[i]);
        }

        // Re-enable denoise
        touch_pad_denoise_t denoise = {
            .grade = TOUCH_PAD_DENOISE_BIT4,
            .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
        };
        touch_pad_denoise_set_config(&denoise);
        touch_pad_denoise_enable();

        // Restart FSM
        touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
        touch_pad_fsm_start();

        // Wait for hardware to settle
        vTaskDelay(pdMS_TO_TICKS(300));

        // Clear last values so we don't detect as "stuck"
        lastRawValues_.fill(0);

        // Recalibrate baselines
        calibrate();
    }

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
    std::array<uint32_t, kNumPads> baselines_;      // Captured at calibration time
    std::array<uint32_t, kNumPads> lastRawValues_;  // For stuck detection
    std::array<uint32_t, kNumPads> stuckCount_;     // Per-pad stuck counter
    bool initialized_;
};

}  // namespace domes
