#pragma once

/**
 * @file mockConfigStorage.hpp
 * @brief Mock configuration storage for unit testing
 *
 * Provides in-memory configuration storage for testing services
 * that depend on persistent configuration.
 */

#include "interfaces/iConfigStorage.hpp"

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace domes {

/**
 * @brief Mock configuration storage for unit testing
 *
 * Stores configuration in memory. Allows tests to pre-populate
 * values and verify storage operations.
 *
 * @code
 * MockConfigStorage mockStorage;
 * mockStorage.presetU8["brightness"] = 128;
 *
 * SomeService service(mockStorage);
 * service.loadConfig();
 *
 * TEST_ASSERT_TRUE(mockStorage.openCalled);
 * @endcode
 */
class MockConfigStorage : public IConfigStorage {
public:
    MockConfigStorage() {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        openCalled = false;
        closeCalled = false;
        commitCalled = false;
        eraseAllCalled = false;

        openReturnValue = ESP_OK;
        commitReturnValue = ESP_OK;
        eraseAllReturnValue = ESP_OK;

        isOpen_ = false;
        currentNamespace_.clear();

        u8Values.clear();
        u16Values.clear();
        u32Values.clear();
        i32Values.clear();
        blobValues.clear();

        accessedKeys.clear();
        modifiedKeys.clear();
    }

    // IConfigStorage implementation
    esp_err_t open(const char* ns) override {
        openCalled = true;
        lastNamespace = ns ? ns : "";

        if (openReturnValue == ESP_OK) {
            isOpen_ = true;
            currentNamespace_ = ns ? ns : "";
        }
        return openReturnValue;
    }

    void close() override {
        closeCalled = true;
        isOpen_ = false;
        currentNamespace_.clear();
    }

    bool isOpen() const override {
        return isOpen_;
    }

    esp_err_t getU8(const char* key, uint8_t& out) const override {
        accessedKeys.push_back(key);
        auto it = u8Values.find(key);
        if (it == u8Values.end()) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        out = it->second;
        return ESP_OK;
    }

    esp_err_t setU8(const char* key, uint8_t value) override {
        modifiedKeys.push_back(key);
        u8Values[key] = value;
        return ESP_OK;
    }

    esp_err_t getU16(const char* key, uint16_t& out) const override {
        accessedKeys.push_back(key);
        auto it = u16Values.find(key);
        if (it == u16Values.end()) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        out = it->second;
        return ESP_OK;
    }

    esp_err_t setU16(const char* key, uint16_t value) override {
        modifiedKeys.push_back(key);
        u16Values[key] = value;
        return ESP_OK;
    }

    esp_err_t getU32(const char* key, uint32_t& out) const override {
        accessedKeys.push_back(key);
        auto it = u32Values.find(key);
        if (it == u32Values.end()) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        out = it->second;
        return ESP_OK;
    }

    esp_err_t setU32(const char* key, uint32_t value) override {
        modifiedKeys.push_back(key);
        u32Values[key] = value;
        return ESP_OK;
    }

    esp_err_t getI32(const char* key, int32_t& out) const override {
        accessedKeys.push_back(key);
        auto it = i32Values.find(key);
        if (it == i32Values.end()) {
            return ESP_ERR_NVS_NOT_FOUND;
        }
        out = it->second;
        return ESP_OK;
    }

    esp_err_t setI32(const char* key, int32_t value) override {
        modifiedKeys.push_back(key);
        i32Values[key] = value;
        return ESP_OK;
    }

    esp_err_t getBlob(const char* key, void* out, size_t& len) const override {
        accessedKeys.push_back(key);
        auto it = blobValues.find(key);
        if (it == blobValues.end()) {
            return ESP_ERR_NVS_NOT_FOUND;
        }

        const auto& blob = it->second;
        if (len < blob.size()) {
            len = blob.size();
            return ESP_ERR_NVS_INVALID_LENGTH;
        }

        std::memcpy(out, blob.data(), blob.size());
        len = blob.size();
        return ESP_OK;
    }

    esp_err_t setBlob(const char* key, const void* data, size_t len) override {
        modifiedKeys.push_back(key);
        auto* bytes = static_cast<const uint8_t*>(data);
        blobValues[key] = std::vector<uint8_t>(bytes, bytes + len);
        return ESP_OK;
    }

    esp_err_t commit() override {
        commitCalled = true;
        commitCount++;
        return commitReturnValue;
    }

    esp_err_t eraseAll() override {
        eraseAllCalled = true;
        u8Values.clear();
        u16Values.clear();
        u32Values.clear();
        i32Values.clear();
        blobValues.clear();
        return eraseAllReturnValue;
    }

    esp_err_t eraseKey(const char* key) override {
        erasedKeys.push_back(key);

        // Erase from all maps
        u8Values.erase(key);
        u16Values.erase(key);
        u32Values.erase(key);
        i32Values.erase(key);
        blobValues.erase(key);

        return ESP_OK;
    }

    // Test inspection - method calls
    bool openCalled;
    bool closeCalled;
    bool commitCalled;
    bool eraseAllCalled;
    uint32_t commitCount = 0;

    // Test control - return values
    esp_err_t openReturnValue;
    esp_err_t commitReturnValue;
    esp_err_t eraseAllReturnValue;

    // Test inspection - captured arguments
    std::string lastNamespace;

    // Test control - preset values (test can populate before running)
    mutable std::map<std::string, uint8_t> u8Values;
    mutable std::map<std::string, uint16_t> u16Values;
    mutable std::map<std::string, uint32_t> u32Values;
    mutable std::map<std::string, int32_t> i32Values;
    mutable std::map<std::string, std::vector<uint8_t>> blobValues;

    // Test inspection - access tracking
    mutable std::vector<std::string> accessedKeys;
    mutable std::vector<std::string> modifiedKeys;
    mutable std::vector<std::string> erasedKeys;

private:
    bool isOpen_ = false;
    std::string currentNamespace_;
};

}  // namespace domes
