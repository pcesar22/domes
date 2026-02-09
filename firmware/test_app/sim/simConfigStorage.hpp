#pragma once

#include "interfaces/iConfigStorage.hpp"

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace sim {

class SimConfigStorage : public domes::IConfigStorage {
public:
    esp_err_t open(const char* /*ns*/) override {
        open_ = true;
        return ESP_OK;
    }

    void close() override { open_ = false; }
    bool isOpen() const override { return open_; }

    esp_err_t getU8(const char* key, uint8_t& out) const override {
        return getVal(key, &out, sizeof(out));
    }
    esp_err_t setU8(const char* key, uint8_t value) override {
        return setVal(key, &value, sizeof(value));
    }

    esp_err_t getU16(const char* key, uint16_t& out) const override {
        return getVal(key, &out, sizeof(out));
    }
    esp_err_t setU16(const char* key, uint16_t value) override {
        return setVal(key, &value, sizeof(value));
    }

    esp_err_t getU32(const char* key, uint32_t& out) const override {
        return getVal(key, &out, sizeof(out));
    }
    esp_err_t setU32(const char* key, uint32_t value) override {
        return setVal(key, &value, sizeof(value));
    }

    esp_err_t getI32(const char* key, int32_t& out) const override {
        return getVal(key, &out, sizeof(out));
    }
    esp_err_t setI32(const char* key, int32_t value) override {
        return setVal(key, &value, sizeof(value));
    }

    esp_err_t getBlob(const char* key, void* out, size_t& len) const override {
        auto it = store_.find(key);
        if (it == store_.end()) return ESP_ERR_NVS_NOT_FOUND;
        if (len < it->second.size()) return ESP_ERR_NVS_INVALID_LENGTH;
        len = it->second.size();
        std::memcpy(out, it->second.data(), len);
        return ESP_OK;
    }

    esp_err_t setBlob(const char* key, const void* data, size_t len) override {
        auto* bytes = static_cast<const uint8_t*>(data);
        store_[key] = std::vector<uint8_t>(bytes, bytes + len);
        return ESP_OK;
    }

    esp_err_t commit() override { return ESP_OK; }

    esp_err_t eraseAll() override {
        store_.clear();
        return ESP_OK;
    }

    esp_err_t eraseKey(const char* key) override {
        auto it = store_.find(key);
        if (it == store_.end()) return ESP_ERR_NVS_NOT_FOUND;
        store_.erase(it);
        return ESP_OK;
    }

private:
    esp_err_t getVal(const char* key, void* out, size_t size) const {
        auto it = store_.find(key);
        if (it == store_.end()) return ESP_ERR_NVS_NOT_FOUND;
        if (it->second.size() != size) return ESP_ERR_NVS_INVALID_LENGTH;
        std::memcpy(out, it->second.data(), size);
        return ESP_OK;
    }

    esp_err_t setVal(const char* key, const void* val, size_t size) {
        auto* bytes = static_cast<const uint8_t*>(val);
        store_[key] = std::vector<uint8_t>(bytes, bytes + size);
        return ESP_OK;
    }

    bool open_ = false;
    std::map<std::string, std::vector<uint8_t>> store_;
};

}  // namespace sim
