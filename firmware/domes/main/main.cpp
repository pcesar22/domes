/**
 * @file main.cpp
 * @brief DOMES Firmware - Phase 2 Complete
 *
 * LED Driver validated on ESP32-S3-DevKitC-1 v1.1
 * - GPIO38 for RGB LED (WS2812)
 * - SPI backend (RMT has conflicts on this board)
 */

#include "config.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include <cstdint>
#include <cstring>

static constexpr const char* kTag = "domes";

using namespace domes::config;

static led_strip_handle_t ledStrip = nullptr;

/**
 * @brief Initialize the LED strip hardware
 *
 * Configures the WS2812 LED using SPI backend (required for DevKitC-1 v1.1
 * with Octal PSRAM - RMT backend has timing conflicts).
 *
 * @return ESP_OK on success, error code on failure
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
 * @brief LED demo task - cycles through colors
 *
 * Demonstrates LED functionality by cycling through 8 colors
 * at 1 second intervals.
 *
 * @param pvParameters Task parameters (unused)
 */
void ledDemoTask(void* pvParameters) {
    ESP_LOGI(kTag, "LED demo task started");

    // Color sequence
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
    while (true) {
        const auto& c = kColors[idx];
        led_strip_set_pixel(ledStrip, 0, c.r, c.g, c.b);
        led_strip_refresh(ledStrip);
        ESP_LOGI(kTag, "LED: %s", c.name);

        idx = (idx + 1) % kNumColors;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main() {
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "  DOMES Firmware - Phase 2 Complete");
    ESP_LOGI(kTag, "  LED Driver Validated!");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "");

    if (initLedStrip() != ESP_OK) {
        ESP_LOGE(kTag, "LED init failed, stopping");
        return;
    }

    // Quick RGB flash to confirm
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

    ESP_LOGI(kTag, "Starting color cycle...");
    xTaskCreate(ledDemoTask, "led_demo", 4096, nullptr, 5, nullptr);
}
