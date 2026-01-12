/**
 * @file nvsConfig.cpp
 * @brief NVS configuration storage implementation
 */

#include "infra/nvsConfig.hpp"

#include "infra/logging.hpp"
#include "nvs_flash.h"

static constexpr const char* kTag = domes::infra::tag::kNvs;

namespace domes::infra {

NvsConfig::NvsConfig() : handle_(0), isOpen_(false) {}

NvsConfig::~NvsConfig() {
    close();
}

esp_err_t NvsConfig::initFlash() {
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "NVS partition needs formatting, erasing...");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to erase NVS: %s", esp_err_to_name(err));
            return err;
        }
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init NVS flash: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "NVS flash initialized");
    return ESP_OK;
}

esp_err_t NvsConfig::open(const char* ns) {
    if (isOpen_) {
        ESP_LOGW(kTag, "Namespace already open, closing first");
        close();
    }

    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return err;
    }

    isOpen_ = true;
    ESP_LOGD(kTag, "Opened namespace '%s'", ns);
    return ESP_OK;
}

void NvsConfig::close() {
    if (isOpen_) {
        nvs_close(handle_);
        handle_ = 0;
        isOpen_ = false;
        ESP_LOGD(kTag, "Namespace closed");
    }
}

bool NvsConfig::isOpen() const {
    return isOpen_;
}

esp_err_t NvsConfig::getU8(const char* key, uint8_t& out) const {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_get_u8(handle_, key, &out);
}

esp_err_t NvsConfig::setU8(const char* key, uint8_t value) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_set_u8(handle_, key, value);
}

esp_err_t NvsConfig::getU16(const char* key, uint16_t& out) const {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_get_u16(handle_, key, &out);
}

esp_err_t NvsConfig::setU16(const char* key, uint16_t value) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_set_u16(handle_, key, value);
}

esp_err_t NvsConfig::getU32(const char* key, uint32_t& out) const {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_get_u32(handle_, key, &out);
}

esp_err_t NvsConfig::setU32(const char* key, uint32_t value) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_set_u32(handle_, key, value);
}

esp_err_t NvsConfig::getI32(const char* key, int32_t& out) const {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_get_i32(handle_, key, &out);
}

esp_err_t NvsConfig::setI32(const char* key, int32_t value) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_set_i32(handle_, key, value);
}

esp_err_t NvsConfig::getBlob(const char* key, void* out, size_t& len) const {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_get_blob(handle_, key, out, &len);
}

esp_err_t NvsConfig::setBlob(const char* key, const void* data, size_t len) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }
    return nvs_set_blob(handle_, key, data, len);
}

esp_err_t NvsConfig::commit() {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }

    esp_err_t err = nvs_commit(handle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to commit: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(kTag, "Changes committed to flash");
    return ESP_OK;
}

esp_err_t NvsConfig::eraseAll() {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }

    esp_err_t err = nvs_erase_all(handle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to erase all: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "All keys erased from namespace");
    return ESP_OK;
}

esp_err_t NvsConfig::eraseKey(const char* key) {
    if (!isOpen_) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }

    esp_err_t err = nvs_erase_key(handle_, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Key doesn't exist, that's fine
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to erase key '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(kTag, "Key '%s' erased", key);
    return ESP_OK;
}

// Template specializations for getOrDefault

template <>
uint8_t NvsConfig::getOrDefault<uint8_t>(const char* key, uint8_t defaultValue) const {
    uint8_t value = defaultValue;
    esp_err_t err = getU8(key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    return value;
}

template <>
uint16_t NvsConfig::getOrDefault<uint16_t>(const char* key, uint16_t defaultValue) const {
    uint16_t value = defaultValue;
    esp_err_t err = getU16(key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    return value;
}

template <>
uint32_t NvsConfig::getOrDefault<uint32_t>(const char* key, uint32_t defaultValue) const {
    uint32_t value = defaultValue;
    esp_err_t err = getU32(key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    return value;
}

template <>
int32_t NvsConfig::getOrDefault<int32_t>(const char* key, int32_t defaultValue) const {
    int32_t value = defaultValue;
    esp_err_t err = getI32(key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    return value;
}

}  // namespace domes::infra
