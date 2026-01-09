/**
 * DOMES Firmware - Phase 2 Complete
 *
 * LED Driver validated on ESP32-S3-DevKitC-1 v1.1
 * - GPIO38 for RGB LED (WS2812)
 * - SPI backend (RMT has conflicts on this board)
 */

#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include "config.hpp"

static const char* TAG = "domes";

using namespace domes::config;

static led_strip_handle_t led_strip = nullptr;

static esp_err_t init_led_strip(void)
{
    ESP_LOGI(TAG, "Initializing LED on GPIO%d (SPI backend)", static_cast<int>(pins::LED_DATA));

    led_strip_config_t strip_config = {};
    memset(&strip_config, 0, sizeof(strip_config));
    strip_config.strip_gpio_num = pins::LED_DATA;
    strip_config.max_leds = pins::LED_COUNT;
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_spi_config_t spi_config = {};
    memset(&spi_config, 0, sizeof(spi_config));
    spi_config.spi_bus = SPI2_HOST;
    spi_config.flags.with_dma = true;

    esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
        return err;
    }

    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LED strip initialized successfully");
    return ESP_OK;
}

void led_demo_task(void* pvParameters)
{
    ESP_LOGI(TAG, "LED demo task started");

    // Color sequence
    struct Color { uint8_t r, g, b; const char* name; };
    const Color colors[] = {
        {255,   0,   0, "RED"},
        {  0, 255,   0, "GREEN"},
        {  0,   0, 255, "BLUE"},
        {255, 255, 255, "WHITE"},
        {255, 255,   0, "YELLOW"},
        {  0, 255, 255, "CYAN"},
        {255,   0, 255, "MAGENTA"},
        {255, 128,   0, "ORANGE"},
    };
    constexpr size_t num_colors = sizeof(colors) / sizeof(colors[0]);

    size_t idx = 0;
    while (true) {
        const auto& c = colors[idx];
        led_strip_set_pixel(led_strip, 0, c.r, c.g, c.b);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "LED: %s", c.name);

        idx = (idx + 1) % num_colors;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  DOMES Firmware - Phase 2 Complete");
    ESP_LOGI(TAG, "  LED Driver Validated!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    if (init_led_strip() != ESP_OK) {
        ESP_LOGE(TAG, "LED init failed, stopping");
        return;
    }

    // Quick RGB flash to confirm
    ESP_LOGI(TAG, "Quick RGB test...");
    led_strip_set_pixel(led_strip, 0, 255, 0, 0);
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_strip_set_pixel(led_strip, 0, 0, 0, 255);
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_strip_clear(led_strip);

    ESP_LOGI(TAG, "Starting color cycle...");
    xTaskCreate(led_demo_task, "led_demo", 4096, nullptr, 5, nullptr);
}
