#pragma once

/**
 * @file ledService.hpp
 * @brief LED animation service with pattern support
 *
 * Provides LED pattern control via protocol commands:
 * - Solid color
 * - Breathing (pulsing brightness)
 * - Color cycle (automatic color transitions)
 */

#include "config.pb.h"
#include "config/featureManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "interfaces/iLedDriver.hpp"
#include "utils/ledAnimator.hpp"

#include <atomic>
#include <cstring>

namespace domes {

/**
 * @brief LED pattern configuration
 */
struct LedPatternConfig {
    domes_config_LedPatternType type = domes_config_LedPatternType_LED_PATTERN_OFF;
    Color primaryColor = Color::off();
    Color colors[8] = {};
    uint8_t colorCount = 0;
    uint32_t periodMs = 2000;
    uint8_t brightness = 128;
};

/**
 * @brief LED animation service
 *
 * Manages LED patterns and runs animation updates in a FreeRTOS task.
 * Checks FEATURE_LED_EFFECTS to determine if effects are enabled.
 *
 * @code
 * LedService led(driver, featureManager);
 * led.start();
 * led.setPattern(pattern);
 * @endcode
 */
class LedService {
public:
    /**
     * @brief Construct LED service
     *
     * @param driver LED driver reference (must outlive service)
     * @param features Feature manager for checking LED_EFFECTS enabled
     */
    LedService(ILedDriver& driver, config::FeatureManager& features)
        : driver_(driver),
          features_(features),
          animator_(driver),
          taskHandle_(nullptr),
          running_(false),
          colorCycleIndex_(0),
          lastColorChangeMs_(0) {}

    ~LedService() { stop(); }

    // Non-copyable
    LedService(const LedService&) = delete;
    LedService& operator=(const LedService&) = delete;

    /**
     * @brief Start the LED animation task
     * @return ESP_OK on success
     */
    esp_err_t start() {
        if (running_) {
            return ESP_ERR_INVALID_STATE;
        }

        running_ = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            taskEntry, "led_svc", 2048, this, 5, &taskHandle_, 1  // Core 1 for responsive LEDs
        );

        if (ret != pdPASS) {
            running_ = false;
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI("LedService", "Started LED animation task");
        return ESP_OK;
    }

    /**
     * @brief Stop the LED animation task
     */
    void stop() {
        if (taskHandle_) {
            running_ = false;
            vTaskDelay(pdMS_TO_TICKS(50));  // Let task exit gracefully
            vTaskDelete(taskHandle_);
            taskHandle_ = nullptr;
            ESP_LOGI("LedService", "Stopped LED animation task");
        }
    }

    /**
     * @brief Set LED pattern from protobuf message
     * @param pattern Pattern configuration from protocol
     * @return ESP_OK on success
     */
    esp_err_t setPattern(const domes_config_LedPattern& pattern) {
        ESP_LOGI("LedService", "setPattern called: type=%d, has_color=%d, period=%lu, brightness=%lu",
                 pattern.type, pattern.has_color, pattern.period_ms, pattern.brightness);

        if (pattern.has_color) {
            ESP_LOGI("LedService", "  Color: R=%lu G=%lu B=%lu W=%lu",
                     pattern.color.r, pattern.color.g, pattern.color.b, pattern.color.w);
        }

        LedPatternConfig config;
        config.type = pattern.type;
        config.periodMs = pattern.period_ms > 0 ? pattern.period_ms : 2000;
        config.brightness = pattern.brightness > 0 ? static_cast<uint8_t>(pattern.brightness) : 128;

        // Convert primary color - always try to get it even if has_color is false
        config.primaryColor = Color::rgbw(
            static_cast<uint8_t>(pattern.color.r),
            static_cast<uint8_t>(pattern.color.g),
            static_cast<uint8_t>(pattern.color.b),
            static_cast<uint8_t>(pattern.color.w)
        );

        ESP_LOGI("LedService", "  Config color: R=%d G=%d B=%d W=%d",
                 config.primaryColor.r, config.primaryColor.g,
                 config.primaryColor.b, config.primaryColor.w);

        // Convert color array for color cycle
        config.colorCount = pattern.colors_count > 8 ? 8 : pattern.colors_count;
        for (uint8_t i = 0; i < config.colorCount; i++) {
            config.colors[i] = Color::rgbw(
                static_cast<uint8_t>(pattern.colors[i].r),
                static_cast<uint8_t>(pattern.colors[i].g),
                static_cast<uint8_t>(pattern.colors[i].b),
                static_cast<uint8_t>(pattern.colors[i].w)
            );
        }

        // Default colors for color cycle if none provided
        if (config.type == domes_config_LedPatternType_LED_PATTERN_COLOR_CYCLE && config.colorCount == 0) {
            config.colors[0] = Color::red();
            config.colors[1] = Color::green();
            config.colors[2] = Color::blue();
            config.colors[3] = Color::yellow();
            config.colors[4] = Color::cyan();
            config.colors[5] = Color::magenta();
            config.colorCount = 6;
        }

        return applyPattern(config);
    }

    /**
     * @brief Get current pattern configuration
     * @param pattern Output pattern message
     */
    void getPattern(domes_config_LedPattern& pattern) const {
        pattern.type = currentPattern_.type;
        pattern.period_ms = currentPattern_.periodMs;
        pattern.brightness = currentPattern_.brightness;

        pattern.has_color = true;
        pattern.color.r = currentPattern_.primaryColor.r;
        pattern.color.g = currentPattern_.primaryColor.g;
        pattern.color.b = currentPattern_.primaryColor.b;
        pattern.color.w = currentPattern_.primaryColor.w;

        pattern.colors_count = currentPattern_.colorCount;
        for (uint8_t i = 0; i < currentPattern_.colorCount; i++) {
            pattern.colors[i].r = currentPattern_.colors[i].r;
            pattern.colors[i].g = currentPattern_.colors[i].g;
            pattern.colors[i].b = currentPattern_.colors[i].b;
            pattern.colors[i].w = currentPattern_.colors[i].w;
        }
    }

private:
    static void taskEntry(void* arg) { static_cast<LedService*>(arg)->taskLoop(); }

    void taskLoop() {
        const TickType_t delay = pdMS_TO_TICKS(16);  // ~60fps
        uint32_t loopCount = 0;

        ESP_LOGI("LedService", "Task loop starting");

        while (running_) {
            // Check if LED effects feature is enabled
            if (!features_.isEnabled(config::Feature::kLedEffects)) {
                // Turn off LEDs when feature is disabled
                if (loopCount % 100 == 0) {
                    ESP_LOGD("LedService", "LED effects disabled, clearing LEDs");
                }
                driver_.clear();
                driver_.refresh();
                vTaskDelay(pdMS_TO_TICKS(100));
                loopCount++;
                continue;
            }

            // Log every 60 iterations (~1 second)
            if (loopCount % 60 == 0) {
                ESP_LOGD("LedService", "Task running, pattern type=%d, brightness=%d",
                         currentPattern_.type, currentPattern_.brightness);
            }

            updateAnimation();
            vTaskDelay(delay);
            loopCount++;
        }
        ESP_LOGI("LedService", "Task loop exiting");
    }

    void updateAnimation() {
        switch (currentPattern_.type) {
            case domes_config_LedPatternType_LED_PATTERN_OFF:
                driver_.clear();
                driver_.refresh();
                break;

            case domes_config_LedPatternType_LED_PATTERN_SOLID:
                driver_.setAll(currentPattern_.primaryColor);
                driver_.refresh();
                break;

            case domes_config_LedPatternType_LED_PATTERN_BREATHING:
                animator_.update();
                break;

            case domes_config_LedPatternType_LED_PATTERN_COLOR_CYCLE:
                updateColorCycle();
                break;

            default:
                break;
        }
    }

    void updateColorCycle() {
        if (currentPattern_.colorCount == 0) return;

        uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        uint32_t elapsed = now - lastColorChangeMs_;

        // Time to transition to next color?
        if (elapsed >= currentPattern_.periodMs) {
            colorCycleIndex_ = (colorCycleIndex_ + 1) % currentPattern_.colorCount;
            Color nextColor = currentPattern_.colors[colorCycleIndex_];
            animator_.transitionTo(nextColor, currentPattern_.periodMs / 2);
            lastColorChangeMs_ = now;
        }

        animator_.update();
    }

    esp_err_t applyPattern(const LedPatternConfig& config) {
        ESP_LOGI("LedService", "applyPattern: type=%d, brightness=%d, color=(%d,%d,%d,%d)",
                 config.type, config.brightness,
                 config.primaryColor.r, config.primaryColor.g,
                 config.primaryColor.b, config.primaryColor.w);

        currentPattern_ = config;
        driver_.setBrightness(config.brightness);

        // Reset animation state
        animator_.stopBreathing();
        colorCycleIndex_ = 0;
        lastColorChangeMs_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);

        switch (config.type) {
            case domes_config_LedPatternType_LED_PATTERN_OFF:
                ESP_LOGI("LedService", "Applying OFF pattern");
                driver_.clear();
                driver_.refresh();
                break;

            case domes_config_LedPatternType_LED_PATTERN_SOLID:
                ESP_LOGI("LedService", "Applying SOLID pattern with color (%d,%d,%d,%d)",
                         config.primaryColor.r, config.primaryColor.g,
                         config.primaryColor.b, config.primaryColor.w);
                driver_.setAll(config.primaryColor);
                driver_.refresh();
                break;

            case domes_config_LedPatternType_LED_PATTERN_BREATHING:
                ESP_LOGI("LedService", "Applying BREATHING pattern");
                animator_.startBreathing(config.primaryColor, config.periodMs);
                break;

            case domes_config_LedPatternType_LED_PATTERN_COLOR_CYCLE:
                ESP_LOGI("LedService", "Applying COLOR_CYCLE pattern with %d colors", config.colorCount);
                if (config.colorCount > 0) {
                    animator_.transitionTo(config.colors[0], 0);
                }
                break;

            default:
                ESP_LOGE("LedService", "Unknown pattern type: %d", config.type);
                return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI("LedService", "Pattern applied successfully");
        return ESP_OK;
    }

    ILedDriver& driver_;
    config::FeatureManager& features_;
    LedAnimator animator_;
    TaskHandle_t taskHandle_;
    std::atomic<bool> running_;

    LedPatternConfig currentPattern_;
    uint8_t colorCycleIndex_;
    uint32_t lastColorChangeMs_;
};

}  // namespace domes
