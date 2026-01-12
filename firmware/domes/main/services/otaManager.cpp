/**
 * @file otaManager.cpp
 * @brief OTA update manager implementation
 */

#include "otaManager.hpp"

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "infra/logging.hpp"
#include "mbedtls/sha256.h"

#include <cstdio>
#include <cstring>

// Version macros from CMakeLists.txt
#ifndef DOMES_VERSION_STRING
#define DOMES_VERSION_STRING "v0.0.0-unknown"
#endif

namespace domes {

namespace {
constexpr const char* kTag = "ota";
}

OtaManager::OtaManager(GithubClient& github)
    : github_(github),
      state_(OtaState::kIdle),
      bytesReceived_(0),
      totalBytes_(0),
      abortRequested_(false),
      progressCallback_(nullptr),
      completeCallback_(nullptr),
      runningPartition_(nullptr),
      currentVersion_{} {
    lastError_[0] = '\0';
}

OtaManager::~OtaManager() {
    // Nothing to clean up
}

esp_err_t OtaManager::init() {
    ESP_LOGI(kTag, "Initializing OTA manager");

    // Get running partition
    runningPartition_ = esp_ota_get_running_partition();
    if (!runningPartition_) {
        ESP_LOGE(kTag, "Failed to get running partition");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Running from partition: %s at 0x%lx", runningPartition_->label,
             runningPartition_->address);

    // Parse current version
    currentVersion_ = parseVersion(DOMES_VERSION_STRING);
    ESP_LOGI(kTag, "Current version: %d.%d.%d", currentVersion_.major, currentVersion_.minor,
             currentVersion_.patch);

    // Check OTA state
    if (isPendingVerification()) {
        ESP_LOGW(kTag, "Firmware pending verification - call confirmFirmware() after self-test");
    }

    state_ = OtaState::kIdle;
    return ESP_OK;
}

FirmwareVersion OtaManager::getCurrentVersion() const {
    return currentVersion_;
}

esp_err_t OtaManager::checkForUpdate(OtaCheckResult& result) {
    result = {};
    result.currentVersion = currentVersion_;

    if (state_ != OtaState::kIdle) {
        ESP_LOGW(kTag, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    state_ = OtaState::kCheckingVersion;
    ESP_LOGI(kTag, "Checking for updates...");

    GithubRelease release;
    esp_err_t err = github_.getLatestRelease(release);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to fetch release: %s", esp_err_to_name(err));
        state_ = OtaState::kIdle;
        return err;
    }

    if (!release.found) {
        ESP_LOGI(kTag, "No release found");
        state_ = OtaState::kIdle;
        return ESP_OK;
    }

    // Parse available version
    result.availableVersion = parseVersion(release.tagName);

    ESP_LOGI(kTag, "Available version: %d.%d.%d", result.availableVersion.major,
             result.availableVersion.minor, result.availableVersion.patch);

    // Check if update is available
    result.updateAvailable = currentVersion_.isUpdateAvailable(result.availableVersion);

    if (result.updateAvailable) {
        ESP_LOGI(kTag, "Update available!");
        result.firmwareSize = release.firmware.size;
        std::strncpy(result.downloadUrl, release.firmware.downloadUrl,
                     sizeof(result.downloadUrl) - 1);
        std::strncpy(result.sha256, release.sha256, sizeof(result.sha256) - 1);
    } else {
        ESP_LOGI(kTag, "Already running latest version");
    }

    state_ = OtaState::kIdle;
    return ESP_OK;
}

esp_err_t OtaManager::startUpdate(const char* downloadUrl, const char* expectedSha256) {
    if (!downloadUrl || strlen(downloadUrl) == 0) {
        ESP_LOGE(kTag, "Invalid download URL");
        return ESP_ERR_INVALID_ARG;
    }

    if (state_ != OtaState::kIdle) {
        ESP_LOGW(kTag, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(kTag, "Starting OTA from: %s", downloadUrl);

    state_ = OtaState::kDownloading;
    bytesReceived_ = 0;
    totalBytes_ = 0;
    abortRequested_ = false;
    lastError_[0] = '\0';

    // Configure HTTPS OTA
    esp_http_client_config_t httpConfig = {};
    httpConfig.url = downloadUrl;
    httpConfig.timeout_ms = kTimeoutMs;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
    httpConfig.buffer_size = kBufferSize;
    httpConfig.buffer_size_tx = 1024;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;
    otaConfig.bulk_flash_erase = false;
    otaConfig.partial_http_download = false;

    esp_https_ota_handle_t otaHandle = nullptr;

    for (uint8_t attempt = 0; attempt < kMaxRetries; attempt++) {
        if (abortRequested_) {
            ESP_LOGW(kTag, "OTA aborted by user");
            state_ = OtaState::kIdle;
            if (completeCallback_) {
                completeCallback_(false, "Aborted by user");
            }
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = esp_https_ota_begin(&otaConfig, &otaHandle);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "OTA begin failed (attempt %d/%d): %s", attempt + 1, kMaxRetries,
                     esp_err_to_name(err));

            if (attempt < kMaxRetries - 1) {
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
                continue;
            }

            std::snprintf(lastError_, sizeof(lastError_), "OTA begin failed: %s",
                          esp_err_to_name(err));
            state_ = OtaState::kError;
            if (completeCallback_) {
                completeCallback_(false, lastError_);
            }
            return err;
        }

        break;
    }

    // Get image size
    int imageSize = esp_https_ota_get_image_size(otaHandle);
    if (imageSize > 0) {
        totalBytes_ = static_cast<size_t>(imageSize);
        ESP_LOGI(kTag, "Firmware size: %zu bytes", totalBytes_.load());
    }

    // Download in chunks
    esp_err_t err;
    while (true) {
        if (abortRequested_) {
            ESP_LOGW(kTag, "OTA aborted during download");
            esp_https_ota_abort(otaHandle);
            state_ = OtaState::kIdle;
            if (completeCallback_) {
                completeCallback_(false, "Aborted by user");
            }
            return ESP_ERR_INVALID_STATE;
        }

        err = esp_https_ota_perform(otaHandle);

        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            int progress = esp_https_ota_get_image_len_read(otaHandle);
            if (progress > 0) {
                bytesReceived_ = static_cast<size_t>(progress);
            }

            if (progressCallback_ && totalBytes_ > 0) {
                progressCallback_(bytesReceived_, totalBytes_);
            }

            // Feed watchdog
            vTaskDelay(1);
            continue;
        }

        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA perform failed: %s", esp_err_to_name(err));
        std::snprintf(lastError_, sizeof(lastError_), "Download failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(otaHandle);
        state_ = OtaState::kError;
        if (completeCallback_) {
            completeCallback_(false, lastError_);
        }
        return err;
    }

    ESP_LOGI(kTag, "Download complete, verifying...");
    state_ = OtaState::kVerifying;

    // Verify SHA-256 if provided
    if (expectedSha256 && strlen(expectedSha256) == 64) {
        const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
        err = verifyFirmwareHash(updatePartition, expectedSha256);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Hash verification failed");
            std::snprintf(lastError_, sizeof(lastError_), "Hash verification failed");
            esp_https_ota_abort(otaHandle);
            state_ = OtaState::kError;
            if (completeCallback_) {
                completeCallback_(false, lastError_);
            }
            return err;
        }
        ESP_LOGI(kTag, "Hash verified successfully");
    }

    // Check if update is valid
    if (!esp_https_ota_is_complete_data_received(otaHandle)) {
        ESP_LOGE(kTag, "Incomplete data received");
        std::snprintf(lastError_, sizeof(lastError_), "Incomplete download");
        esp_https_ota_abort(otaHandle);
        state_ = OtaState::kError;
        if (completeCallback_) {
            completeCallback_(false, lastError_);
        }
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Installing firmware...");
    state_ = OtaState::kInstalling;

    err = esp_https_ota_finish(otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA finish failed: %s", esp_err_to_name(err));
        std::snprintf(lastError_, sizeof(lastError_), "Install failed: %s", esp_err_to_name(err));
        state_ = OtaState::kError;
        if (completeCallback_) {
            completeCallback_(false, lastError_);
        }
        return err;
    }

    ESP_LOGI(kTag, "OTA successful! Rebooting...");
    state_ = OtaState::kRebooting;

    if (completeCallback_) {
        completeCallback_(true, nullptr);
    }

    // Allow callbacks and logs to complete
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_restart();

    // Never reached
    return ESP_OK;
}

void OtaManager::abort() {
    if (state_ == OtaState::kDownloading || state_ == OtaState::kVerifying) {
        ESP_LOGW(kTag, "Aborting OTA...");
        abortRequested_ = true;
    }
}

OtaState OtaManager::getState() const {
    return state_;
}

size_t OtaManager::getBytesReceived() const {
    return bytesReceived_;
}

size_t OtaManager::getTotalBytes() const {
    return totalBytes_;
}

void OtaManager::onProgress(OtaProgressCallback callback) {
    progressCallback_ = callback;
}

void OtaManager::onComplete(OtaCompleteCallback callback) {
    completeCallback_ = callback;
}

esp_err_t OtaManager::confirmFirmware() {
    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(runningPartition_, &state);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to get partition state: %s", esp_err_to_name(err));
        return err;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(kTag, "Confirming new firmware as valid");
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(kTag, "Firmware confirmed successfully");
        }
        return err;
    }

    ESP_LOGD(kTag, "Firmware already confirmed (state: %d)", static_cast<int>(state));
    return ESP_OK;
}

esp_err_t OtaManager::rollback() {
    ESP_LOGW(kTag, "Rolling back to previous firmware...");

    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();

    // This should not return
    ESP_LOGE(kTag, "Rollback failed: %s", esp_err_to_name(err));
    return err;
}

bool OtaManager::isPendingVerification() const {
    if (!runningPartition_) {
        return false;
    }

    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(runningPartition_, &state);

    if (err != ESP_OK) {
        return false;
    }

    return state == ESP_OTA_IMG_PENDING_VERIFY;
}

const char* OtaManager::getCurrentPartition() const {
    return runningPartition_ ? runningPartition_->label : "unknown";
}

esp_err_t OtaManager::verifyFirmwareHash(const esp_partition_t* partition,
                                         const char* expectedSha256) {
    if (!partition || !expectedSha256 || strlen(expectedSha256) != 64) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(kTag, "Verifying firmware hash...");

    // Get SHA-256 of partition
    uint8_t sha256[32];
    esp_err_t err = esp_partition_get_sha256(partition, sha256);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to get partition SHA-256: %s", esp_err_to_name(err));
        return err;
    }

    // Convert to hex string
    char actualSha256[65];
    for (int i = 0; i < 32; i++) {
        std::snprintf(actualSha256 + i * 2, 3, "%02x", sha256[i]);
    }
    actualSha256[64] = '\0';

    // Compare (case-insensitive)
    if (strcasecmp(actualSha256, expectedSha256) != 0) {
        ESP_LOGE(kTag, "SHA-256 mismatch!");
        ESP_LOGE(kTag, "Expected: %s", expectedSha256);
        ESP_LOGE(kTag, "Actual:   %s", actualSha256);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(kTag, "SHA-256 verified: %s", actualSha256);
    return ESP_OK;
}

}  // namespace domes
