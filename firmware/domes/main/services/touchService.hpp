#pragma once

/**
 * @file touchService.hpp
 * @brief Touch pad monitoring service with LED feedback
 *
 * Monitors 4 capacitive touch pads and controls all 16 LEDs
 * with a unique color for each pad:
 * - Pad 1: Red
 * - Pad 2: Green
 * - Pad 3: Blue
 * - Pad 4: Yellow
 */

#include "config/featureManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "interfaces/iTouchDriver.hpp"
#include "services/ledService.hpp"

#include <atomic>

namespace domes {

/**
 * @brief Touch monitoring service with LED color feedback
 *
 * Runs a FreeRTOS task that polls touch pads at 100Hz and
 * updates LED colors based on which pad is touched.
 *
 * @code
 * TouchService touch(touchDriver, ledDriver, featureManager);
 * touch.start();
 * // LEDs will automatically change color when pads are touched
 * @endcode
 */
class TouchService {
public:
    /**
     * @brief Construct touch service
     *
     * @param touchDriver Touch driver reference (must outlive service)
     * @param ledService LED service reference (must outlive service)
     * @param features Feature manager for checking TOUCH feature enabled
     */
    TouchService(ITouchDriver& touchDriver, LedService& ledService, config::FeatureManager& features)
        : touchDriver_(touchDriver),
          ledService_(ledService),
          features_(features),
          taskHandle_(nullptr),
          running_(false),
          lastActivepad_(-1) {}

    ~TouchService() { stop(); }

    // Non-copyable
    TouchService(const TouchService&) = delete;
    TouchService& operator=(const TouchService&) = delete;

    /**
     * @brief Start the touch monitoring task
     * @return ESP_OK on success
     */
    esp_err_t start() {
        if (running_) {
            return ESP_ERR_INVALID_STATE;
        }

        running_ = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            taskEntry, "touch_svc", 3072, this, 5, &taskHandle_, 1  // Core 1 for responsive input
        );

        if (ret != pdPASS) {
            running_ = false;
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI(kTag, "Touch service started");
        return ESP_OK;
    }

    /**
     * @brief Stop the touch monitoring task
     */
    void stop() {
        if (taskHandle_) {
            running_ = false;
            vTaskDelay(pdMS_TO_TICKS(50));  // Let task exit gracefully
            vTaskDelete(taskHandle_);
            taskHandle_ = nullptr;
            ESP_LOGI(kTag, "Touch service stopped");
        }
    }

    /**
     * @brief Get the currently active touch pad
     * @return Pad index (0-3) or -1 if no pad is touched
     */
    int8_t getActivePad() const { return lastActivepad_; }

private:
    static constexpr const char* kTag = "TouchService";

    // Colors for each touch pad
    static constexpr Color kPadColors[4] = {
        Color::red(),     // Pad 1 (GPIO1)
        Color::green(),   // Pad 2 (GPIO2)
        Color::blue(),    // Pad 3 (GPIO4)
        Color::yellow(),  // Pad 4 (GPIO6)
    };

    static void taskEntry(void* arg) { static_cast<TouchService*>(arg)->taskLoop(); }

    void taskLoop() {
        const TickType_t delay = pdMS_TO_TICKS(10);  // 100 Hz polling
        uint32_t loopCount = 0;

        ESP_LOGI(kTag, "Touch monitoring task started");

        while (running_) {
            // Check if touch feature is enabled
            if (!features_.isEnabled(config::Feature::kTouch)) {
                if (loopCount % 100 == 0) {
                    ESP_LOGD(kTag, "Touch feature disabled, skipping");
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                loopCount++;
                continue;
            }

            // Update touch readings
            touchDriver_.update();

            // Find which pad (if any) is being touched
            int8_t activePad = -1;
            for (uint8_t i = 0; i < touchDriver_.getPadCount() && i < 4; i++) {
                if (touchDriver_.isTouched(i)) {
                    activePad = static_cast<int8_t>(i);
                    break;  // First touched pad wins (priority order)
                }
            }

            // Update LEDs if touch state changed
            if (activePad != lastActivepad_) {
                lastActivepad_ = activePad;

                if (activePad >= 0) {
                    // Pad touched - set all LEDs to the pad's color via LedService
                    ESP_LOGI(kTag, "Pad %d touched - setting LEDs to %s",
                             activePad, getColorName(activePad));
                    ledService_.setSolidColor(kPadColors[activePad]);
                } else {
                    // No pad touched - turn off LEDs
                    ESP_LOGI(kTag, "No touch - clearing LEDs");
                    ledService_.setOff();
                }
            }

            // Log touch readings periodically for debugging
            if (loopCount % 50 == 0) {  // Every 500ms
                for (uint8_t i = 0; i < touchDriver_.getPadCount() && i < 4; i++) {
                    auto state = touchDriver_.getPadState(i);
                    ESP_LOGI(kTag, "Pad %d: raw=%lu, thresh=%lu, touched=%d",
                             i, state.rawValue, state.threshold, state.touched);
                }
            }

            vTaskDelay(delay);
            loopCount++;
        }

        ESP_LOGI(kTag, "Touch monitoring task exiting");
    }

    static const char* getColorName(int8_t padIndex) {
        switch (padIndex) {
            case 0: return "RED";
            case 1: return "GREEN";
            case 2: return "BLUE";
            case 3: return "YELLOW";
            default: return "UNKNOWN";
        }
    }

    ITouchDriver& touchDriver_;
    LedService& ledService_;
    config::FeatureManager& features_;
    TaskHandle_t taskHandle_;
    std::atomic<bool> running_;
    std::atomic<int8_t> lastActivepad_;
};

}  // namespace domes
