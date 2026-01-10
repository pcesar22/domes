/**
 * @file main.cpp
 * @brief DOMES Firmware - Phase 4 Infrastructure
 *
 * Infrastructure layer validated on ESP32-S3-DevKitC-1 v1.1
 * - NVS configuration storage
 * - Task Watchdog Timer (TWDT)
 * - Managed task lifecycle
 * - LED demo using TaskManager
 */

#include "config.hpp"
#include "infra/logging.hpp"
#include "infra/watchdog.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/taskManager.hpp"
#include "interfaces/iTaskRunner.hpp"
#include "drivers/ledStrip.hpp"
#include "utils/ledAnimator.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <cstdint>
#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kMain;

using namespace domes::config;

// Global instances (static allocation per GUIDELINES.md)
static domes::LedStripDriver<pins::kLedCount>* ledDriver = nullptr;
static domes::LedAnimator* ledAnimator = nullptr;
static domes::infra::TaskManager taskManager;
static domes::infra::NvsConfig configStorage;
static domes::infra::NvsConfig statsStorage;

/**
 * @brief LED demo task runner with smooth transitions
 *
 * Demonstrates smooth color transitions and breathing effects.
 * Cycles through colors with 500ms transitions, switches to
 * breathing mode periodically.
 */
class LedDemoRunner : public domes::ITaskRunner {
public:
    explicit LedDemoRunner(domes::LedAnimator& animator)
        : animator_(animator), running_(true) {}

    void run() override {
        ESP_LOGI(kTag, "LED demo task started (smooth transitions)");

        static constexpr domes::Color kColors[] = {
            domes::Color::red(),
            domes::Color::green(),
            domes::Color::blue(),
            domes::Color::white(),
            domes::Color::yellow(),
            domes::Color::cyan(),
            domes::Color::magenta(),
            domes::Color::orange(),
        };
        static constexpr size_t kNumColors = sizeof(kColors) / sizeof(kColors[0]);
        static constexpr const char* kColorNames[] = {
            "RED", "GREEN", "BLUE", "WHITE", "YELLOW", "CYAN", "MAGENTA", "ORANGE"
        };

        static constexpr uint32_t kTransitionMs = 500;
        static constexpr uint32_t kHoldMs = 500;
        static constexpr uint32_t kBreathingDurationMs = 10000;  // 10s breathing
        static constexpr uint32_t kCycleDurationMs = 20000;      // 20s color cycle

        size_t colorIdx = 0;
        uint32_t modeStartTime = 0;
        bool breathingMode = false;

        // Start with first color
        animator_.transitionTo(kColors[0], kTransitionMs);
        ESP_LOGI(kTag, "LED: %s (transitioning)", kColorNames[0]);

        while (shouldRun()) {
            // Update animator (this refreshes LEDs)
            animator_.update();

            // Check mode switching
            uint32_t elapsed = xTaskGetTickCount() * portTICK_PERIOD_MS - modeStartTime;

            if (breathingMode) {
                // In breathing mode
                if (elapsed >= kBreathingDurationMs) {
                    // Switch to color cycle
                    breathingMode = false;
                    modeStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    animator_.stopBreathing();
                    colorIdx = 0;
                    animator_.transitionTo(kColors[0], kTransitionMs);
                    ESP_LOGI(kTag, "Mode: Color cycle");
                }
            } else {
                // In color cycle mode
                if (elapsed >= kCycleDurationMs) {
                    // Switch to breathing
                    breathingMode = true;
                    modeStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    animator_.startBreathing(domes::Color::cyan(), 2000);
                    ESP_LOGI(kTag, "Mode: Breathing (cyan)");
                } else if (!animator_.isAnimating()) {
                    // Transition complete, wait then move to next color
                    vTaskDelay(pdMS_TO_TICKS(kHoldMs));

                    colorIdx = (colorIdx + 1) % kNumColors;
                    animator_.transitionTo(kColors[colorIdx], kTransitionMs);
                    ESP_LOGI(kTag, "LED: %s", kColorNames[colorIdx]);
                }
            }

            // Reset watchdog and yield
            domes::infra::Watchdog::reset();
            vTaskDelay(pdMS_TO_TICKS(16));  // ~60fps update rate
        }

        // Clear LED on exit
        animator_.stopBreathing();
        animator_.transitionTo(domes::Color::off(), 200);
        for (int i = 0; i < 15; ++i) {
            animator_.update();
            vTaskDelay(pdMS_TO_TICKS(16));
        }
        ESP_LOGI(kTag, "LED demo task stopped");
    }

    esp_err_t requestStop() override {
        running_ = false;
        return ESP_OK;
    }

    bool shouldRun() const override {
        return running_;
    }

private:
    domes::LedAnimator& animator_;
    volatile bool running_;
};

// Static task runner instance
static LedDemoRunner* ledDemoRunner = nullptr;

/**
 * @brief Initialize the LED strip hardware and animator
 */
static esp_err_t initLedStrip() {
    ESP_LOGI(kTag, "Initializing LED on GPIO%d", static_cast<int>(pins::kLedData));

    // Create LED driver (static allocation)
    static domes::LedStripDriver<pins::kLedCount> driver(pins::kLedData, false);
    ledDriver = &driver;

    esp_err_t err = ledDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init LED driver: %s", esp_err_to_name(err));
        return err;
    }

    // Set default brightness
    ledDriver->setBrightness(led::kDefaultBrightness);

    // Create animator
    static domes::LedAnimator animator(*ledDriver);
    ledAnimator = &animator;

    ESP_LOGI(kTag, "LED strip initialized with animator");
    return ESP_OK;
}

/**
 * @brief Initialize infrastructure services
 *
 * Must be called before hardware initialization.
 */
static esp_err_t initInfrastructure() {
    esp_err_t err;

    // 1. Initialize NVS flash (must be first for config access)
    err = domes::infra::NvsConfig::initFlash();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NVS init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Open config namespace
    err = configStorage.open(domes::infra::nvs_ns::kConfig);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Config namespace not found, will use defaults");
    }

    // 3. Initialize watchdog
    err = domes::infra::Watchdog::init(timing::kWatchdogTimeoutS, true);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Watchdog init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(kTag, "Watchdog initialized (%lu second timeout)",
             static_cast<unsigned long>(timing::kWatchdogTimeoutS));

    // 4. Increment and log boot counter
    err = statsStorage.open(domes::infra::nvs_ns::kStats);
    if (err == ESP_OK) {
        uint32_t bootCount = statsStorage.getOrDefault<uint32_t>(
            domes::infra::stats_key::kBootCount, 0);
        bootCount++;
        statsStorage.setU32(domes::infra::stats_key::kBootCount, bootCount);
        statsStorage.commit();
        ESP_LOGI(kTag, "Boot count: %lu", static_cast<unsigned long>(bootCount));
    }

    return ESP_OK;
}

extern "C" void app_main() {
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "  DOMES Firmware - Phase 4");
    ESP_LOGI(kTag, "  Infrastructure Layer");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "");

    // Initialize infrastructure first
    if (initInfrastructure() != ESP_OK) {
        ESP_LOGE(kTag, "Infrastructure init failed, halting");
        return;
    }
    ESP_LOGI(kTag, "Infrastructure initialized");

    // Initialize hardware drivers
    if (initLedStrip() != ESP_OK) {
        ESP_LOGW(kTag, "LED init failed, continuing without LED");
    }

    // Quick RGB flash to confirm hardware (using smooth transitions)
    if (ledAnimator) {
        ESP_LOGI(kTag, "Quick RGB test (smooth)...");
        ledAnimator->transitionTo(domes::Color::red(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::green(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::blue(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::off(), 150);
        for (int i = 0; i < 10; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
    }

    // Create LED demo task via TaskManager
    if (ledAnimator) {
        // Allocate runner (during init, heap is allowed per GUIDELINES.md)
        static LedDemoRunner runner(*ledAnimator);
        ledDemoRunner = &runner;

        domes::infra::TaskConfig ledConfig = {
            .name = "led_demo",
            .stackSize = domes::infra::stack::kMinimal,
            .priority = domes::infra::priority::kLow,
            .coreAffinity = domes::infra::core::kAny,
            .subscribeToWatchdog = true
        };

        esp_err_t err = taskManager.createTask(ledConfig, runner);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to create LED demo task");
        }
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "Initialization complete");
    ESP_LOGI(kTag, "Active tasks: %zu", taskManager.getActiveTaskCount());
    ESP_LOGI(kTag, "");
}
