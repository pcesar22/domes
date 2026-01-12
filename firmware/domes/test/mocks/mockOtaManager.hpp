#pragma once

/**
 * @file mockOtaManager.hpp
 * @brief Mock OTA manager for unit testing
 *
 * Provides controllable OTA behavior for testing services
 * that depend on firmware updates.
 */

#include "interfaces/iOtaManager.hpp"

#include <cstring>

namespace domes {

/**
 * @brief Mock OTA manager for unit testing
 *
 * Allows tests to simulate OTA update scenarios.
 *
 * @code
 * MockOtaManager mockOta;
 * mockOta.updateAvailable = true;
 * mockOta.availableVersion = {2, 0, 0};
 *
 * SomeService service(mockOta);
 * service.checkForUpdates();
 *
 * TEST_ASSERT_TRUE(mockOta.checkForUpdateCalled);
 * @endcode
 */
class MockOtaManager : public IOtaManager {
public:
    MockOtaManager() {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        initCalled = false;
        checkForUpdateCalled = false;
        startUpdateCalled = false;
        abortCalled = false;
        confirmFirmwareCalled = false;
        rollbackCalled = false;

        initReturnValue = ESP_OK;
        checkForUpdateReturnValue = ESP_OK;
        startUpdateReturnValue = ESP_OK;
        confirmFirmwareReturnValue = ESP_OK;
        rollbackReturnValue = ESP_OK;

        currentVersion = {1, 0, 0};
        availableVersion = {1, 0, 0};
        updateAvailable = false;
        firmwareSize = 0;
        state_ = OtaState::kIdle;
        bytesReceived_ = 0;
        totalBytes_ = 0;
        pendingVerification = false;

        std::strcpy(downloadUrl, "");
        std::strcpy(sha256, "");
        std::strcpy(currentPartition_, "ota_0");

        lastDownloadUrl[0] = '\0';
        lastExpectedSha256[0] = '\0';
    }

    // IOtaManager implementation
    esp_err_t init() override {
        initCalled = true;
        return initReturnValue;
    }

    FirmwareVersion getCurrentVersion() const override {
        return currentVersion;
    }

    esp_err_t checkForUpdate(OtaCheckResult& result) override {
        checkForUpdateCalled = true;

        if (checkForUpdateReturnValue != ESP_OK) {
            return checkForUpdateReturnValue;
        }

        result.updateAvailable = updateAvailable;
        result.currentVersion = currentVersion;
        result.availableVersion = availableVersion;
        result.firmwareSize = firmwareSize;
        std::strncpy(result.downloadUrl, downloadUrl, sizeof(result.downloadUrl) - 1);
        std::strncpy(result.sha256, sha256, sizeof(result.sha256) - 1);

        return ESP_OK;
    }

    esp_err_t startUpdate(const char* url, const char* expectedSha256) override {
        startUpdateCalled = true;

        if (url) {
            std::strncpy(lastDownloadUrl, url, sizeof(lastDownloadUrl) - 1);
        }
        if (expectedSha256) {
            std::strncpy(lastExpectedSha256, expectedSha256, sizeof(lastExpectedSha256) - 1);
        }

        if (startUpdateReturnValue == ESP_OK) {
            state_ = OtaState::kDownloading;
            totalBytes_ = firmwareSize;
        }

        return startUpdateReturnValue;
    }

    void abort() override {
        abortCalled = true;
        state_ = OtaState::kIdle;
        bytesReceived_ = 0;
    }

    OtaState getState() const override {
        return state_;
    }

    size_t getBytesReceived() const override {
        return bytesReceived_;
    }

    size_t getTotalBytes() const override {
        return totalBytes_;
    }

    void onProgress(OtaProgressCallback callback) override {
        progressCallback_ = std::move(callback);
    }

    void onComplete(OtaCompleteCallback callback) override {
        completeCallback_ = std::move(callback);
    }

    esp_err_t confirmFirmware() override {
        confirmFirmwareCalled = true;
        pendingVerification = false;
        return confirmFirmwareReturnValue;
    }

    esp_err_t rollback() override {
        rollbackCalled = true;
        return rollbackReturnValue;
    }

    bool isPendingVerification() const override {
        return pendingVerification;
    }

    const char* getCurrentPartition() const override {
        return currentPartition_;
    }

    // Test control methods

    /**
     * @brief Simulate download progress
     */
    void simulateProgress(size_t bytes, size_t total) {
        bytesReceived_ = bytes;
        totalBytes_ = total;
        if (progressCallback_) {
            progressCallback_(bytes, total);
        }
    }

    /**
     * @brief Simulate update completion
     */
    void simulateComplete(bool success, const char* errorMsg = nullptr) {
        state_ = success ? OtaState::kRebooting : OtaState::kError;
        if (completeCallback_) {
            completeCallback_(success, errorMsg);
        }
    }

    /**
     * @brief Set OTA state directly
     */
    void setState(OtaState state) {
        state_ = state;
    }

    /**
     * @brief Set current partition label
     */
    void setCurrentPartition(const char* partition) {
        std::strncpy(currentPartition_, partition, sizeof(currentPartition_) - 1);
    }

    // Test inspection - method calls
    bool initCalled;
    bool checkForUpdateCalled;
    bool startUpdateCalled;
    bool abortCalled;
    bool confirmFirmwareCalled;
    bool rollbackCalled;

    // Test control - return values
    esp_err_t initReturnValue;
    esp_err_t checkForUpdateReturnValue;
    esp_err_t startUpdateReturnValue;
    esp_err_t confirmFirmwareReturnValue;
    esp_err_t rollbackReturnValue;

    // Test control - mock data
    FirmwareVersion currentVersion;
    FirmwareVersion availableVersion;
    bool updateAvailable;
    size_t firmwareSize;
    char downloadUrl[256];
    char sha256[65];
    bool pendingVerification;

    // Test inspection - captured arguments
    char lastDownloadUrl[256];
    char lastExpectedSha256[65];

private:
    OtaState state_ = OtaState::kIdle;
    size_t bytesReceived_ = 0;
    size_t totalBytes_ = 0;
    char currentPartition_[16] = "ota_0";
    OtaProgressCallback progressCallback_;
    OtaCompleteCallback completeCallback_;
};

}  // namespace domes
