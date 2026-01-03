/**
 * @file main.cpp
 * @brief DOMES Firmware Entry Point
 *
 * Initializes all hardware drivers and starts FreeRTOS tasks.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config/constants.hpp"
#include "platform/pins.hpp"

static constexpr const char* kTag = "main";

/**
 * @brief Initialize NVS flash storage
 * @return ESP_OK on success
 */
static esp_err_t initNvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "NVS partition truncated, erasing...");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), kTag, "NVS erase failed");
        err = nvs_flash_init();
    }
    return err;
}

/**
 * @brief Print system information at startup
 */
static void printSystemInfo() {
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "DOMES Firmware v%d.%d.%d",
             config::kVersionMajor,
             config::kVersionMinor,
             config::kVersionPatch);
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "Platform: %s", config::kPlatformName);
    ESP_LOGI(kTag, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(kTag, "IDF version: %s", esp_get_idf_version());
}

extern "C" void app_main(void) {
    // Initialize NVS (required for WiFi/BLE)
    esp_err_t err = initNvs();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }

    // Print startup banner
    printSystemInfo();

    // TODO: Initialize drivers
    // - LED driver (RMT/SK6812)
    // - Audio driver (I2S/MAX98357A)
    // - Haptic driver (I2C/DRV2605L)
    // - Touch driver (capacitive)
    // - IMU driver (I2C/LIS2DW12)

    // TODO: Initialize services
    // - Feedback service (coordinates LED/audio/haptic)
    // - Communication service (ESP-NOW + BLE)
    // - Game service (reaction drills)

    // TODO: Start FreeRTOS tasks
    // - Audio task (Core 1, priority 5)
    // - Game task (Core 1, priority 4)
    // - Communication task (Core 0, priority 3)
    // - LED update task (either core, priority 2)

    ESP_LOGI(kTag, "Initialization complete");

    // Main loop - can be used for system monitoring
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
