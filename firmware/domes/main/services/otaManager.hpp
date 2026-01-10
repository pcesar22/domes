#pragma once

/**
 * @file otaManager.hpp
 * @brief OTA update manager implementation
 *
 * Provides HTTPS OTA updates with:
 * - GitHub Releases integration
 * - SHA-256 verification
 * - Automatic rollback protection
 * - Progress reporting
 */

#include "interfaces/iOtaManager.hpp"
#include "githubClient.hpp"

#include "esp_ota_ops.h"
#include "esp_app_format.h"

#include <atomic>

namespace domes {

/**
 * @brief OTA update manager implementation
 *
 * Manages OTA updates using ESP-IDF's esp_https_ota.
 *
 * @code
 * GithubClient github("pcesar22", "domes");
 * OtaManager ota(github);
 *
 * ota.init();
 *
 * OtaCheckResult result;
 * if (ota.checkForUpdate(result) == ESP_OK && result.updateAvailable) {
 *     ota.startUpdate(result.downloadUrl, result.sha256);
 *     // Device will reboot on success
 * }
 * @endcode
 */
class OtaManager : public IOtaManager {
public:
    /**
     * @brief Construct OTA manager
     *
     * @param github GitHub client for version checking
     */
    explicit OtaManager(GithubClient& github);

    ~OtaManager() override;

    // Non-copyable
    OtaManager(const OtaManager&) = delete;
    OtaManager& operator=(const OtaManager&) = delete;

    // IOtaManager implementation
    esp_err_t init() override;
    FirmwareVersion getCurrentVersion() const override;
    esp_err_t checkForUpdate(OtaCheckResult& result) override;
    esp_err_t startUpdate(const char* downloadUrl,
                         const char* expectedSha256 = nullptr) override;
    void abort() override;
    OtaState getState() const override;
    size_t getBytesReceived() const override;
    size_t getTotalBytes() const override;
    void onProgress(OtaProgressCallback callback) override;
    void onComplete(OtaCompleteCallback callback) override;
    esp_err_t confirmFirmware() override;
    esp_err_t rollback() override;
    bool isPendingVerification() const override;
    const char* getCurrentPartition() const override;

private:
    /**
     * @brief Verify firmware hash after download
     *
     * @param partition Partition containing new firmware
     * @param expectedSha256 Expected hash (64 hex chars)
     * @return ESP_OK if hash matches
     */
    esp_err_t verifyFirmwareHash(const esp_partition_t* partition,
                                  const char* expectedSha256);

    GithubClient& github_;

    std::atomic<OtaState> state_;
    std::atomic<size_t> bytesReceived_;
    std::atomic<size_t> totalBytes_;
    std::atomic<bool> abortRequested_;

    OtaProgressCallback progressCallback_;
    OtaCompleteCallback completeCallback_;

    const esp_partition_t* runningPartition_;
    FirmwareVersion currentVersion_;

    char lastError_[128];

    static constexpr uint32_t kTimeoutMs = 120000;  // 2 minutes
    static constexpr size_t kBufferSize = 4096;
    static constexpr uint8_t kMaxRetries = 3;
    static constexpr uint32_t kRetryDelayMs = 5000;
};

}  // namespace domes
