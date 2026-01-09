/**
 * @file watchdog.cpp
 * @brief Task Watchdog Timer implementation
 */

#include "infra/watchdog.hpp"
#include "infra/logging.hpp"

#include "esp_task_wdt.h"

static constexpr const char* kTag = domes::infra::tag::kWatchdog;

namespace domes::infra {

bool Watchdog::initialized_ = false;

esp_err_t Watchdog::init(uint32_t timeoutSec, bool panicOnTimeout) {
    if (initialized_) {
        ESP_LOGW(kTag, "Watchdog already initialized by us");
        return ESP_OK;
    }

    esp_task_wdt_config_t config = {
        .timeout_ms = timeoutSec * 1000,
        .idle_core_mask = 0,  // Don't monitor idle tasks by default
        .trigger_panic = panicOnTimeout
    };

    esp_err_t err = esp_task_wdt_init(&config);

    // If TWDT was already initialized by ESP-IDF (CONFIG_ESP_TASK_WDT_INIT=y),
    // reconfigure it with our settings instead
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "TWDT already initialized by system, reconfiguring...");
        err = esp_task_wdt_reconfigure(&config);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to reconfigure TWDT: %s", esp_err_to_name(err));
            return err;
        }
    } else if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init TWDT: %s", esp_err_to_name(err));
        return err;
    }

    initialized_ = true;
    ESP_LOGI(kTag, "Watchdog initialized (timeout=%lus, panic=%s)",
             static_cast<unsigned long>(timeoutSec),
             panicOnTimeout ? "yes" : "no");
    return ESP_OK;
}

esp_err_t Watchdog::deinit() {
    if (!initialized_) {
        return ESP_OK;
    }

    esp_err_t err = esp_task_wdt_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to deinit TWDT: %s", esp_err_to_name(err));
        return err;
    }

    initialized_ = false;
    ESP_LOGI(kTag, "Watchdog deinitialized");
    return ESP_OK;
}

esp_err_t Watchdog::subscribe() {
    return subscribe(xTaskGetCurrentTaskHandle());
}

esp_err_t Watchdog::subscribe(TaskHandle_t taskHandle) {
    if (!initialized_) {
        ESP_LOGE(kTag, "Cannot subscribe: watchdog not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_task_wdt_add(taskHandle);
    if (err == ESP_ERR_INVALID_ARG) {
        // Task already subscribed
        ESP_LOGD(kTag, "Task already subscribed to watchdog");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to subscribe task: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(kTag, "Task subscribed to watchdog");
    return ESP_OK;
}

esp_err_t Watchdog::unsubscribe() {
    return unsubscribe(xTaskGetCurrentTaskHandle());
}

esp_err_t Watchdog::unsubscribe(TaskHandle_t taskHandle) {
    if (!initialized_) {
        return ESP_OK;  // Nothing to unsubscribe from
    }

    esp_err_t err = esp_task_wdt_delete(taskHandle);
    if (err == ESP_ERR_INVALID_ARG) {
        // Task not subscribed
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to unsubscribe task: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(kTag, "Task unsubscribed from watchdog");
    return ESP_OK;
}

esp_err_t Watchdog::reset() {
    if (!initialized_) {
        return ESP_OK;  // Silently succeed if not initialized
    }

    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to reset watchdog: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

bool Watchdog::isInitialized() {
    return initialized_;
}

// WatchdogGuard implementation

WatchdogGuard::WatchdogGuard() : subscribed_(false) {
    if (Watchdog::isInitialized()) {
        esp_err_t err = Watchdog::subscribe();
        subscribed_ = (err == ESP_OK);
    }
}

WatchdogGuard::~WatchdogGuard() {
    if (subscribed_) {
        Watchdog::unsubscribe();
    }
}

} // namespace domes::infra
