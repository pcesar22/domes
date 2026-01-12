#pragma once

/**
 * @file iConfigStorage.hpp
 * @brief Abstract interface for persistent configuration storage
 *
 * Defines the contract for configuration storage implementations.
 * Allows mocking NVS for unit tests.
 */

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for persistent configuration storage
 *
 * Provides type-safe access to key-value configuration storage.
 * Real implementation uses NVS; test mocks can use in-memory storage.
 */
class IConfigStorage {
public:
    virtual ~IConfigStorage() = default;

    /**
     * @brief Open storage namespace
     * @param ns Namespace name (max 15 chars for NVS)
     * @return ESP_OK on success, error code on failure
     */
    virtual esp_err_t open(const char* ns) = 0;

    /**
     * @brief Close current namespace
     */
    virtual void close() = 0;

    /**
     * @brief Check if namespace is currently open
     * @return true if open, false otherwise
     */
    virtual bool isOpen() const = 0;

    // Integer accessors
    virtual esp_err_t getU8(const char* key, uint8_t& out) const = 0;
    virtual esp_err_t setU8(const char* key, uint8_t value) = 0;

    virtual esp_err_t getU16(const char* key, uint16_t& out) const = 0;
    virtual esp_err_t setU16(const char* key, uint16_t value) = 0;

    virtual esp_err_t getU32(const char* key, uint32_t& out) const = 0;
    virtual esp_err_t setU32(const char* key, uint32_t value) = 0;

    virtual esp_err_t getI32(const char* key, int32_t& out) const = 0;
    virtual esp_err_t setI32(const char* key, int32_t value) = 0;

    /**
     * @brief Get blob data
     * @param key Key name
     * @param out Output buffer
     * @param len On input: buffer size. On output: actual data size
     * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
     */
    virtual esp_err_t getBlob(const char* key, void* out, size_t& len) const = 0;

    /**
     * @brief Set blob data
     * @param key Key name
     * @param data Data buffer
     * @param len Data length
     * @return ESP_OK on success
     */
    virtual esp_err_t setBlob(const char* key, const void* data, size_t len) = 0;

    /**
     * @brief Commit pending writes to flash
     *
     * Must be called after set operations to persist data.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t commit() = 0;

    /**
     * @brief Erase all keys in current namespace
     * @return ESP_OK on success
     */
    virtual esp_err_t eraseAll() = 0;

    /**
     * @brief Erase a specific key
     * @param key Key to erase
     * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
     */
    virtual esp_err_t eraseKey(const char* key) = 0;
};

}  // namespace domes
