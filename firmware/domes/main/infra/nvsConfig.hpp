#pragma once

/**
 * @file nvsConfig.hpp
 * @brief NVS-backed configuration storage implementation
 *
 * Provides type-safe access to ESP-IDF NVS with proper error handling.
 */

#include "infra/configKeys.hpp"
#include "interfaces/iConfigStorage.hpp"
#include "nvs.h"

namespace domes::infra {

/**
 * @brief NVS namespace names for DOMES configuration
 */
/**
 * @brief NVS-backed configuration storage implementation
 *
 * Provides type-safe access to NVS with proper error handling.
 * One instance per namespace to avoid handle conflicts.
 *
 * @note Must call NvsConfig::initFlash() once at startup before
 *       creating instances.
 *
 * @code
 * // At startup:
 * NvsConfig::initFlash();
 *
 * // Usage:
 * NvsConfig config;
 * config.open(nvs_ns::kConfig);
 * uint8_t brightness = config.getOrDefault<uint8_t>(config_key::kBrightness, 128);
 * config.setU8(config_key::kBrightness, 200);
 * config.commit();
 * config.close();
 * @endcode
 */
class NvsConfig : public IConfigStorage {
public:
    NvsConfig();
    ~NvsConfig() override;

    // Non-copyable
    NvsConfig(const NvsConfig&) = delete;
    NvsConfig& operator=(const NvsConfig&) = delete;

    /**
     * @brief Initialize NVS flash partition
     *
     * Must be called once at startup before using NvsConfig instances.
     * Handles first-boot formatting automatically.
     *
     * @return ESP_OK on success
     */
    static esp_err_t initFlash();

    // IConfigStorage implementation
    esp_err_t open(const char* ns) override;
    void close() override;
    bool isOpen() const override;

    esp_err_t getU8(const char* key, uint8_t& out) const override;
    esp_err_t setU8(const char* key, uint8_t value) override;

    esp_err_t getU16(const char* key, uint16_t& out) const override;
    esp_err_t setU16(const char* key, uint16_t value) override;

    esp_err_t getU32(const char* key, uint32_t& out) const override;
    esp_err_t setU32(const char* key, uint32_t value) override;

    esp_err_t getI32(const char* key, int32_t& out) const override;
    esp_err_t setI32(const char* key, int32_t value) override;

    esp_err_t getBlob(const char* key, void* out, size_t& len) const override;
    esp_err_t setBlob(const char* key, const void* data, size_t len) override;

    esp_err_t commit() override;
    esp_err_t eraseAll() override;
    esp_err_t eraseKey(const char* key) override;

    /**
     * @brief Get value with default fallback
     *
     * Returns the stored value if found, otherwise returns the default.
     * Does not write the default to storage.
     *
     * @tparam T Value type (uint8_t, uint16_t, uint32_t, int32_t)
     * @param key NVS key
     * @param defaultValue Value to return if key not found
     * @return Stored value or default
     */
    template <typename T>
    T getOrDefault(const char* key, T defaultValue) const;

private:
    nvs_handle_t handle_;
    bool isOpen_;
};

// Template specializations
template <>
uint8_t NvsConfig::getOrDefault<uint8_t>(const char* key, uint8_t defaultValue) const;

template <>
uint16_t NvsConfig::getOrDefault<uint16_t>(const char* key, uint16_t defaultValue) const;

template <>
uint32_t NvsConfig::getOrDefault<uint32_t>(const char* key, uint32_t defaultValue) const;

template <>
int32_t NvsConfig::getOrDefault<int32_t>(const char* key, int32_t defaultValue) const;

}  // namespace domes::infra
