#pragma once

/**
 * @file iOtaManager.hpp
 * @brief Abstract interface for OTA update management
 *
 * Defines the contract for OTA update implementations.
 * Allows mocking for unit tests without hardware.
 */

#include "esp_err.h"
#include "services/githubClient.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace domes {

/**
 * @brief OTA update state
 */
enum class OtaState : uint8_t {
    kIdle,             ///< No update in progress
    kCheckingVersion,  ///< Checking for updates
    kDownloading,      ///< Downloading firmware
    kVerifying,        ///< Verifying downloaded firmware
    kInstalling,       ///< Writing to flash
    kRebooting,        ///< About to reboot
    kError,            ///< Error occurred
};

/**
 * @brief OTA update check result
 */
struct OtaCheckResult {
    bool updateAvailable;              ///< True if newer version found
    FirmwareVersion currentVersion;    ///< Currently running version
    FirmwareVersion availableVersion;  ///< Available version
    size_t firmwareSize;               ///< Size of new firmware
    char downloadUrl[256];             ///< URL to download firmware
    char sha256[65];                   ///< Expected SHA-256 hash
};

/**
 * @brief OTA progress callback
 *
 * @param bytesReceived Bytes downloaded so far
 * @param totalBytes Total firmware size
 */
using OtaProgressCallback = std::function<void(size_t bytesReceived, size_t totalBytes)>;

/**
 * @brief OTA completion callback
 *
 * @param success True if update succeeded (device will reboot)
 * @param errorMsg Error message if failed, nullptr on success
 */
using OtaCompleteCallback = std::function<void(bool success, const char* errorMsg)>;

/**
 * @brief Abstract interface for OTA management
 *
 * Provides OTA update functionality with version checking,
 * secure download, and rollback protection.
 */
class IOtaManager {
public:
    virtual ~IOtaManager() = default;

    /**
     * @brief Initialize OTA subsystem
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Get currently running firmware version
     *
     * @return Current firmware version
     */
    virtual FirmwareVersion getCurrentVersion() const = 0;

    /**
     * @brief Check for available updates
     *
     * Queries the update server for newer versions.
     *
     * @param result Output struct with update info
     * @return ESP_OK if check succeeded (even if no update available)
     */
    virtual esp_err_t checkForUpdate(OtaCheckResult& result) = 0;

    /**
     * @brief Start OTA download and installation
     *
     * Downloads firmware from URL and installs to next OTA partition.
     * Device will reboot on success.
     *
     * @param downloadUrl URL to firmware binary
     * @param expectedSha256 Expected SHA-256 hash (optional, can be nullptr)
     * @return ESP_OK if download started
     */
    virtual esp_err_t startUpdate(const char* downloadUrl,
                                  const char* expectedSha256 = nullptr) = 0;

    /**
     * @brief Abort current OTA operation
     */
    virtual void abort() = 0;

    /**
     * @brief Get current OTA state
     *
     * @return Current state
     */
    virtual OtaState getState() const = 0;

    /**
     * @brief Get download progress
     *
     * @return Bytes received so far
     */
    virtual size_t getBytesReceived() const = 0;

    /**
     * @brief Get total firmware size
     *
     * @return Total size in bytes, or 0 if not downloading
     */
    virtual size_t getTotalBytes() const = 0;

    /**
     * @brief Register progress callback
     *
     * @param callback Function to call on progress updates
     */
    virtual void onProgress(OtaProgressCallback callback) = 0;

    /**
     * @brief Register completion callback
     *
     * @param callback Function to call when OTA completes or fails
     */
    virtual void onComplete(OtaCompleteCallback callback) = 0;

    /**
     * @brief Confirm current firmware is good
     *
     * Must be called after OTA boot and successful self-test
     * to prevent rollback on next boot.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t confirmFirmware() = 0;

    /**
     * @brief Force rollback to previous firmware
     *
     * Marks current firmware as invalid and reboots.
     * This function does not return.
     *
     * @return Never returns (reboots)
     */
    virtual esp_err_t rollback() = 0;

    /**
     * @brief Check if running from new OTA partition
     *
     * Returns true if the current boot is from a newly updated
     * OTA partition that hasn't been confirmed yet.
     *
     * @return true if pending verification
     */
    virtual bool isPendingVerification() const = 0;

    /**
     * @brief Get the current OTA partition label
     *
     * @return Partition label (e.g., "ota_0" or "ota_1")
     */
    virtual const char* getCurrentPartition() const = 0;
};

}  // namespace domes
