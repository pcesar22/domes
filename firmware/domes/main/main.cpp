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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include <cstdint>
#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kMain;

using namespace domes::config;

// Global instances (static allocation per GUIDELINES.md)
static led_strip_handle_t ledStrip = nullptr;
static domes::infra::TaskManager taskManager;
static domes::infra::NvsConfig configStorage;
static domes::infra::NvsConfig statsStorage;

/**
 * @brief LED demo task runner
 *
 * Demonstrates TaskManager integration with watchdog support.
 */
class LedDemoRunner : public domes::ITaskRunner {
public:
    explicit LedDemoRunner(led_strip_handle_t strip)
        : strip_(strip), running_(true) {}

    void run() override {
        ESP_LOGI(kTag, "LED demo task started");

        struct ColorDef {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            const char* name;
        };

        static constexpr ColorDef kColors[] = {
            {255,   0,   0, "RED"},
            {  0, 255,   0, "GREEN"},
            {  0,   0, 255, "BLUE"},
            {255, 255, 255, "WHITE"},
            {255, 255,   0, "YELLOW"},
            {  0, 255, 255, "CYAN"},
            {255,   0, 255, "MAGENTA"},
            {255, 128,   0, "ORANGE"},
        };
        static constexpr size_t kNumColors = sizeof(kColors) / sizeof(kColors[0]);

        size_t idx = 0;
        while (shouldRun()) {
            const auto& c = kColors[idx];
            led_strip_set_pixel(strip_, 0, c.r, c.g, c.b);
            led_strip_refresh(strip_);
            ESP_LOGI(kTag, "LED: %s", c.name);

            idx = (idx + 1) % kNumColors;

            // Reset watchdog before delay
            domes::infra::Watchdog::reset();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Clear LED on exit
        led_strip_clear(strip_);
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
    led_strip_handle_t strip_;
    volatile bool running_;
};

// Static task runner instance
static LedDemoRunner* ledDemoRunner = nullptr;

/**
 * @brief Initialize the LED strip hardware
 */
static esp_err_t initLedStrip() {
    ESP_LOGI(kTag, "Initializing LED on GPIO%d (SPI backend)", static_cast<int>(pins::kLedData));

    led_strip_config_t stripConfig = {};
    memset(&stripConfig, 0, sizeof(stripConfig));
    stripConfig.strip_gpio_num = pins::kLedData;
    stripConfig.max_leds = pins::kLedCount;
    stripConfig.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    stripConfig.led_model = LED_MODEL_WS2812;

    led_strip_spi_config_t spiConfig = {};
    memset(&spiConfig, 0, sizeof(spiConfig));
    spiConfig.spi_bus = SPI2_HOST;
    spiConfig.flags.with_dma = true;

    esp_err_t err = led_strip_new_spi_device(&stripConfig, &spiConfig, &ledStrip);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create LED strip: %s", esp_err_to_name(err));
        return err;
    }

    led_strip_clear(ledStrip);
    ESP_LOGI(kTag, "LED strip initialized successfully");
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

    // Quick RGB flash to confirm hardware
    if (ledStrip) {
        ESP_LOGI(kTag, "Quick RGB test...");
        led_strip_set_pixel(ledStrip, 0, 255, 0, 0);
        led_strip_refresh(ledStrip);
        vTaskDelay(pdMS_TO_TICKS(300));
        led_strip_set_pixel(ledStrip, 0, 0, 255, 0);
        led_strip_refresh(ledStrip);
        vTaskDelay(pdMS_TO_TICKS(300));
        led_strip_set_pixel(ledStrip, 0, 0, 0, 255);
        led_strip_refresh(ledStrip);
        vTaskDelay(pdMS_TO_TICKS(300));
        led_strip_clear(ledStrip);
    }

    // Create LED demo task via TaskManager
    if (ledStrip) {
        // Allocate runner (during init, heap is allowed per GUIDELINES.md)
        static LedDemoRunner runner(ledStrip);
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
