/**
 * @file githubClient.cpp
 * @brief GitHub Releases API client implementation
 */

#include "githubClient.hpp"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "infra/logging.hpp"
#include "services/releaseMetadata.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace domes {

namespace {
constexpr const char* kTag = "github";
constexpr const char* kApiUrl = "https://api.github.com/repos/%s/%s/releases/latest";
constexpr const char* kUserAgent = "ESP32-OTA-Client/1.0";
}  // namespace

GithubClient::GithubClient(const char* owner, const char* repo) : useCustomEndpoint_(false) {
    std::strncpy(owner_, owner, sizeof(owner_) - 1);
    owner_[sizeof(owner_) - 1] = '\0';

    std::strncpy(repo_, repo, sizeof(repo_) - 1);
    repo_[sizeof(repo_) - 1] = '\0';

    customEndpoint_[0] = '\0';
}

void GithubClient::setEndpoint(const char* endpoint) {
    if (endpoint && strlen(endpoint) > 0) {
        std::strncpy(customEndpoint_, endpoint, sizeof(customEndpoint_) - 1);
        customEndpoint_[sizeof(customEndpoint_) - 1] = '\0';
        useCustomEndpoint_ = true;
    } else {
        useCustomEndpoint_ = false;
    }
}

esp_err_t GithubClient::getLatestRelease(GithubRelease& release) {
    release = {};
    release.found = false;

    // Build URL
    char url[256];
    if (useCustomEndpoint_) {
        std::strncpy(url, customEndpoint_, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
    } else {
        snprintf(url, sizeof(url), kApiUrl, owner_, repo_);
    }

    ESP_LOGI(kTag, "Fetching release from: %s", url);

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = kTimeoutMs;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_agent = kUserAgent;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(kTag, "Failed to create HTTP client");
        return ESP_FAIL;
    }

    // Set headers for GitHub API
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");
    esp_http_client_set_header(client, "X-GitHub-Api-Version", "2022-11-28");

    // Allocate response buffer
    char* responseBuffer = static_cast<char*>(malloc(kMaxResponseSize));
    if (!responseBuffer) {
        ESP_LOGE(kTag, "Failed to allocate response buffer");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    // Open connection
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open connection: %s", esp_err_to_name(err));
        free(responseBuffer);
        esp_http_client_cleanup(client);
        return err;
    }

    // Read headers
    int contentLength = esp_http_client_fetch_headers(client);
    int statusCode = esp_http_client_get_status_code(client);

    ESP_LOGI(kTag, "HTTP status: %d, content-length: %d", statusCode, contentLength);

    if (statusCode != 200) {
        ESP_LOGE(kTag, "Unexpected status code: %d", statusCode);
        free(responseBuffer);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Read response body
    size_t totalRead = 0;
    int readLen;

    while ((readLen = esp_http_client_read(client, responseBuffer + totalRead,
                                           kMaxResponseSize - totalRead - 1)) > 0) {
        totalRead += readLen;
        if (totalRead >= kMaxResponseSize - 1) {
            break;
        }
    }

    if (readLen < 0) {
        ESP_LOGE(kTag, "Failed while reading release response");
        err = ESP_FAIL;
    } else {
        responseBuffer[totalRead] = '\0';
        ESP_LOGD(kTag, "Read %zu bytes", totalRead);
        err = parseRelease(responseBuffer, totalRead, release);
    }

    free(responseBuffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return err;
}

esp_err_t GithubClient::parseRelease(const char* json, size_t len, GithubRelease& release) {
    cJSON* root = cJSON_ParseWithLength(json, len);
    if (!root) {
        ESP_LOGE(kTag, "Failed to parse JSON");
        return ESP_FAIL;
    }

    // Extract tag_name
    cJSON* tagName = cJSON_GetObjectItem(root, "tag_name");
    if (cJSON_IsString(tagName) && tagName->valuestring) {
        const size_t tagLength = std::strlen(tagName->valuestring);
        if (tagLength == 0 || tagLength >= sizeof(release.tagName)) {
            ESP_LOGE(kTag, "Release tag exceeds the firmware version limit");
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }
        std::memcpy(release.tagName, tagName->valuestring, tagLength + 1);
        ESP_LOGI(kTag, "Release tag: %s", release.tagName);
    } else {
        ESP_LOGE(kTag, "No tag_name in response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Extract the application image digest from the release body.
    cJSON* body = cJSON_GetObjectItem(root, "body");
    if (!cJSON_IsString(body) || !body->valuestring ||
        !extractSha256(body->valuestring, release.sha256)) {
        ESP_LOGE(kTag, "Release is missing the application SHA-256");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(kTag, "Found SHA-256: %.16s...", release.sha256);

    // Find firmware asset in assets array
    cJSON* assets = cJSON_GetObjectItem(root, "assets");
    if (!cJSON_IsArray(assets)) {
        ESP_LOGW(kTag, "No assets array in release");
        release.found = true;  // Release exists but no assets
        cJSON_Delete(root);
        return ESP_OK;
    }

    int assetCount = cJSON_GetArraySize(assets);
    ESP_LOGI(kTag, "Found %d assets", assetCount);

    char expectedAssetName[sizeof(release.firmware.name)];
    if (!formatOtaAssetName(release.tagName, expectedAssetName, sizeof(expectedAssetName))) {
        ESP_LOGE(kTag, "Release tag cannot identify an OTA asset");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    for (int i = 0; i < assetCount; i++) {
        cJSON* asset = cJSON_GetArrayItem(assets, i);
        cJSON* name = cJSON_GetObjectItem(asset, "name");

        if (!cJSON_IsString(name) || !name->valuestring) {
            continue;
        }

        const char* assetName = name->valuestring;
        if (std::strcmp(assetName, expectedAssetName) == 0) {
            std::strncpy(release.firmware.name, assetName, sizeof(release.firmware.name) - 1);
            release.firmware.name[sizeof(release.firmware.name) - 1] = '\0';

            cJSON* downloadUrl = cJSON_GetObjectItem(asset, "browser_download_url");
            if (cJSON_IsString(downloadUrl) && downloadUrl->valuestring) {
                const size_t downloadUrlLength = std::strlen(downloadUrl->valuestring);
                if (downloadUrlLength >= sizeof(release.firmware.downloadUrl)) {
                    ESP_LOGE(kTag, "Firmware download URL exceeds the supported length");
                    cJSON_Delete(root);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                std::strncpy(release.firmware.downloadUrl, downloadUrl->valuestring,
                             sizeof(release.firmware.downloadUrl) - 1);
                release.firmware.downloadUrl[sizeof(release.firmware.downloadUrl) - 1] = '\0';
            }

            cJSON* size = cJSON_GetObjectItem(asset, "size");
            if (!cJSON_IsNumber(size) ||
                !parseReleaseAssetSize(size->valuedouble, release.firmware.size)) {
                ESP_LOGE(kTag, "Firmware asset size is not a positive representable integer");
                cJSON_Delete(root);
                return ESP_ERR_INVALID_RESPONSE;
            }

            ESP_LOGI(kTag, "Found firmware: %s (%zu bytes)", release.firmware.name,
                     release.firmware.size);
            break;
        }
    }

    if (release.firmware.name[0] == '\0' || release.firmware.downloadUrl[0] == '\0' ||
        release.firmware.size == 0) {
        ESP_LOGE(kTag, "Release is missing expected OTA asset: %s", expectedAssetName);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    release.found = true;
    cJSON_Delete(root);
    return ESP_OK;
}

bool GithubClient::extractSha256(const char* body, char* sha256Out) {
    if (!body || !sha256Out) {
        return false;
    }

    // Look for "SHA-256: " or "sha256: " pattern
    const char* patterns[] = {"SHA-256:", "sha256:", "SHA256:"};

    for (const char* pattern : patterns) {
        const char* found = std::strstr(body, pattern);
        if (!found) {
            continue;
        }

        // Skip pattern and whitespace
        found += std::strlen(pattern);
        while (*found == ' ' || *found == '\t') {
            found++;
        }

        // Read 64 hex characters
        size_t hashLen = 0;
        while (hashLen < 64 && std::isxdigit(static_cast<unsigned char>(found[hashLen]))) {
            hashLen++;
        }

        if (hashLen == kSha256HexLength &&
            !std::isxdigit(static_cast<unsigned char>(found[hashLen]))) {
            std::strncpy(sha256Out, found, 64);
            sha256Out[64] = '\0';

            // Convert to lowercase
            for (int i = 0; i < 64; i++) {
                sha256Out[i] = std::tolower(static_cast<unsigned char>(sha256Out[i]));
            }

            return isSha256Hex(sha256Out);
        }
    }

    sha256Out[0] = '\0';
    return false;
}

}  // namespace domes
