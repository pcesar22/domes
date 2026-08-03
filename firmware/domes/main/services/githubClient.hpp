#pragma once

/**
 * @file githubClient.hpp
 * @brief GitHub Releases API client for OTA version checking
 *
 * Queries GitHub Releases API to check for firmware updates
 * and provides download URLs for OTA.
 */

#include "esp_err.h"
#include "services/firmwareVersion.hpp"

#include <cstddef>
#include <cstdint>

namespace domes {

/**
 * @brief GitHub Release asset information
 */
struct GithubAsset {
    char name[64];          ///< Asset filename
    char downloadUrl[256];  ///< Direct download URL
    size_t size;            ///< Asset size in bytes
};

/**
 * @brief GitHub Release information
 */
struct GithubRelease {
    char tagName[32];      ///< Release tag (e.g., "v1.2.3")
    char sha256[65];       ///< SHA-256 hash from release body (if present)
    GithubAsset firmware;  ///< Firmware binary asset
    bool found;            ///< True if release was found
};

/**
 * @brief GitHub Releases API client
 *
 * Fetches release information from GitHub to check for updates.
 *
 * @code
 * GithubClient github("pcesar22", "domes");
 * GithubRelease release;
 *
 * if (github.getLatestRelease(release) == ESP_OK && release.found) {
 *     FirmwareVersion available = parseVersion(release.tagName);
 *     if (currentVersion.isUpdateAvailable(available)) {
 *         // Start OTA with release.firmware.downloadUrl
 *     }
 * }
 * @endcode
 */
class GithubClient {
public:
    /**
     * @brief Construct GitHub client
     *
     * @param owner Repository owner (e.g., "pcesar22")
     * @param repo Repository name (e.g., "domes")
     */
    GithubClient(const char* owner, const char* repo);

    /**
     * @brief Fetch latest release information
     *
     * Queries the GitHub Releases API for the latest release.
     * Requires the versioned OTA asset `domes-<tag>.bin` and its SHA-256.
     *
     * @param release Output struct with release info
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t getLatestRelease(GithubRelease& release);

    /**
     * @brief Set custom API endpoint (for testing)
     *
     * @param endpoint Full URL to releases/latest endpoint
     */
    void setEndpoint(const char* endpoint);

private:
    /**
     * @brief Parse JSON response to extract release info
     *
     * @param json JSON string
     * @param len JSON length
     * @param release Output struct
     * @return ESP_OK on success
     */
    esp_err_t parseRelease(const char* json, size_t len, GithubRelease& release);

    /**
     * @brief Extract SHA-256 from release body
     *
     * Looks for pattern: "SHA-256: <hash>" or "sha256: <hash>"
     *
     * @param body Release body text
     * @param sha256Out Output buffer (65 bytes)
     * @return true if hash found
     */
    bool extractSha256(const char* body, char* sha256Out);

    char owner_[32];
    char repo_[32];
    char customEndpoint_[256];
    bool useCustomEndpoint_;

    static constexpr size_t kMaxResponseSize = 16384;
    static constexpr uint32_t kTimeoutMs = 30000;
};

}  // namespace domes
