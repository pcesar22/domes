#pragma once

/**
 * @file imuService.hpp
 * @brief IMU service with triage mode support
 *
 * Polls the IMU for tap events and flashes LEDs when in triage mode.
 * Used for hardware bring-up and debugging.
 */

#include "interfaces/iHapticDriver.hpp"
#include "interfaces/iImuDriver.hpp"
#include "services/audioService.hpp"
#include "services/ledService.hpp"
#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cmath>

namespace domes {

/**
 * @brief IMU service with triage mode
 *
 * Provides triage mode where the LED ring flashes white on tap detection.
 * Useful for hardware bring-up and verifying IMU functionality.
 *
 * @code
 * ImuService imu(driver, ledDriver);
 * imu.start();
 * imu.setTriageMode(true);  // Flash on tap
 * @endcode
 */
class ImuService {
public:
    /**
     * @brief Construct IMU service
     *
     * @param imu IMU driver reference (must outlive service)
     * @param led LED service reference (must outlive service)
     */
    ImuService(IImuDriver& imu, LedService& led)
        : imu_(imu), led_(led), audio_(nullptr), haptic_(nullptr), taskHandle_(nullptr), running_(false) {}

    ~ImuService() { stop(); }

    // Non-copyable
    ImuService(const ImuService&) = delete;
    ImuService& operator=(const ImuService&) = delete;

    /**
     * @brief Start the IMU polling task
     *
     * Initializes IMU tap detection and starts the polling task.
     *
     * @return ESP_OK on success
     */
    esp_err_t start() {
        if (running_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Enable single-tap detection
        esp_err_t err = imu_.enableTapDetection(true, false);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to enable tap detection: %s", esp_err_to_name(err));
            return err;
        }

        running_ = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            taskEntry, "imu_svc", 3072, this, 5, &taskHandle_, 1  // Core 1
        );

        if (ret != pdPASS) {
            running_ = false;
            ESP_LOGE(kTag, "Failed to create IMU task");
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI(kTag, "IMU service started");
        return ESP_OK;
    }

    /**
     * @brief Stop the IMU polling task
     */
    void stop() {
        if (taskHandle_) {
            running_ = false;
            vTaskDelay(pdMS_TO_TICKS(50));  // Let task exit gracefully
            vTaskDelete(taskHandle_);
            taskHandle_ = nullptr;
            ESP_LOGI(kTag, "IMU service stopped");
        }
    }

    /**
     * @brief Enable or disable triage mode
     *
     * In triage mode, the LED ring flashes white on every tap detection.
     *
     * @param enabled True to enable triage mode
     */
    void setTriageMode(bool enabled) {
        triageMode_ = enabled;
        ESP_LOGI(kTag, "Triage mode %s", enabled ? "enabled" : "disabled");
    }

    /**
     * @brief Check if triage mode is enabled
     * @return True if triage mode is enabled
     */
    bool isTriageModeEnabled() const { return triageMode_.load(); }

    /**
     * @brief Set audio service for tap feedback
     *
     * When set, a beep will be played on tap detection.
     *
     * @param audio Audio service pointer (can be nullptr to disable)
     */
    void setAudioService(AudioService* audio) {
        audio_ = audio;
        ESP_LOGI(kTag, "Audio service %s", audio ? "connected" : "disconnected");
    }

    /**
     * @brief Set haptic driver for tap feedback
     *
     * When set, haptic feedback will be triggered on tap detection.
     *
     * @param haptic Haptic driver pointer (can be nullptr to disable)
     */
    void setHapticDriver(IHapticDriver* haptic) {
        haptic_ = haptic;
        ESP_LOGI(kTag, "Haptic driver %s", haptic ? "connected" : "disconnected");
    }

private:
    static constexpr const char* kTag = "imu_svc";

    static void taskEntry(void* arg) { static_cast<ImuService*>(arg)->taskLoop(); }

    void taskLoop() {
        const TickType_t pollDelay = pdMS_TO_TICKS(10);  // 100 Hz polling
        uint32_t loopCount = 0;

        ESP_LOGI(kTag, "IMU task loop starting");

        while (running_) {
            // In triage mode, detect taps via software (more reliable than hardware tap detection)
            if (triageMode_.load()) {
                AccelData accel;
                if (imu_.readAccel(accel) == ESP_OK) {
                    float magnitude = sqrtf(accel.x*accel.x + accel.y*accel.y + accel.z*accel.z);

                    // Log every 200ms (20 loops) to catch movement
                    if (loopCount % 20 == 0) {
                        ESP_LOGI(kTag, "mag=%.2fg X=%.2f Y=%.2f Z=%.2f", magnitude, accel.x, accel.y, accel.z);
                    }

                    // Tap = magnitude deviates from 1g (more sensitive: ±0.15g)
                    if ((magnitude > 1.15f || magnitude < 0.85f) && !tapCooldown_) {
                        TRACE_INSTANT(TRACE_ID("Imu.TapDetected"), domes::trace::Category::kTouch);
                        ESP_LOGI(kTag, ">>> TAP! mag=%.2fg <<<", magnitude);
                        flashWhite();
                        tapCooldown_ = true;
                        cooldownCounter_ = 0;
                    }

                    // Cooldown to prevent retriggering (50 loops = 500ms)
                    if (tapCooldown_) {
                        cooldownCounter_++;
                        if (cooldownCounter_ > 50) {
                            tapCooldown_ = false;
                        }
                    }
                }
            }

            vTaskDelay(pollDelay);
            loopCount++;
        }

        ESP_LOGI(kTag, "IMU task loop exiting");
    }

    /**
     * @brief Request LED flash, audio beep, and haptic feedback (thread-safe)
     *
     * Requests LedService to flash white, AudioService to play a beep,
     * and haptic driver to buzz. All are executed by their respective tasks.
     */
    void flashWhite() {
        TRACE_INSTANT(TRACE_ID("Imu.FlashRequested"), domes::trace::Category::kLed);
        led_.requestFlash(100);

        // Play beep sound if audio service is available
        if (audio_) {
            TRACE_INSTANT(TRACE_ID("Imu.BeepRequested"), domes::trace::Category::kAudio);
            audio_->playAsset("beep");
        }

        // Trigger haptic feedback if available
        if (haptic_) {
            TRACE_INSTANT(TRACE_ID("Imu.HapticRequested"), domes::trace::Category::kHaptic);
            haptic_->playEffect(47);  // Long buzz - more noticeable
        }
    }

    IImuDriver& imu_;
    LedService& led_;
    AudioService* audio_;
    IHapticDriver* haptic_;
    TaskHandle_t taskHandle_;
    std::atomic<bool> running_;
    std::atomic<bool> triageMode_{true};  // Enabled by default
    bool tapCooldown_ = false;
    uint32_t cooldownCounter_ = 0;
};

}  // namespace domes
