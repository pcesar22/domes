#pragma once

/**
 * @file otaBootVerification.hpp
 * @brief Bounded results and retained diagnostics for first-boot OTA verification
 */

#include "esp_err.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace domes {

/**
 * @brief Check that ended post-update boot verification
 */
enum class OtaSelfTestStage : uint8_t {
    kNone = 0,
    kInjectedFailure,
    kWatchdog,
    kNvs,
    kInternalHeap,
    kHardware,
    kLedOutput,
    kRuntimeServices,
    kDispatchUnavailable,
    kConfirmation,
};

/**
 * @brief Result of one complete post-update self-test
 *
 * detail is stage-specific: free bytes for kInternalHeap and the hardware
 * subsystem value for kHardware. Other stages leave it at zero.
 */
struct OtaSelfTestResult {
    esp_err_t status = ESP_OK;
    OtaSelfTestStage stage = OtaSelfTestStage::kNone;
    uint32_t detail = 0;

    [[nodiscard]] bool passed() const { return status == ESP_OK; }

    static OtaSelfTestResult failure(OtaSelfTestStage failedStage, esp_err_t error = ESP_FAIL,
                                     uint32_t failedDetail = 0) {
        return {.status = error, .stage = failedStage, .detail = failedDetail};
    }
};

/// Maximum complete self-test attempts during first-boot OTA verification.
constexpr uint8_t kOtaSelfTestMaxAttempts = 3;

/// Settling time between recoverable internal-heap checks.
constexpr uint32_t kOtaSelfTestRetryDelayMs = 2000;

/**
 * @brief Whether a failed check can recover without reinitializing the runtime
 */
inline bool isRetryableOtaSelfTestFailure(OtaSelfTestStage stage) {
    return stage == OtaSelfTestStage::kInternalHeap;
}

/**
 * @brief Run a bounded first-boot self-test with heap-settling retries
 *
 * Non-memory failures return immediately. The retry callback executes before
 * each repeated attempt and receives the completed-attempt count plus result.
 * Neither callback is retained or allocated.
 */
template <typename SelfTest, typename BeforeRetry>
OtaSelfTestResult runOtaSelfTestWithRetry(SelfTest&& selfTest, BeforeRetry&& beforeRetry) {
    for (uint8_t attempt = 1; attempt <= kOtaSelfTestMaxAttempts; ++attempt) {
        OtaSelfTestResult result = selfTest();
        if (result.passed() || !isRetryableOtaSelfTestFailure(result.stage) ||
            attempt == kOtaSelfTestMaxAttempts) {
            return result;
        }
        beforeRetry(attempt, result);
    }

    return OtaSelfTestResult::failure(OtaSelfTestStage::kInternalHeap);
}

/**
 * @brief Stable diagnostic name for a post-update self-test stage
 */
inline const char* otaSelfTestStageName(OtaSelfTestStage stage) {
    switch (stage) {
        case OtaSelfTestStage::kNone:
            return "none";
        case OtaSelfTestStage::kInjectedFailure:
            return "injected";
        case OtaSelfTestStage::kWatchdog:
            return "watchdog";
        case OtaSelfTestStage::kNvs:
            return "nvs";
        case OtaSelfTestStage::kInternalHeap:
            return "internal-heap";
        case OtaSelfTestStage::kHardware:
            return "hardware";
        case OtaSelfTestStage::kLedOutput:
            return "led-output";
        case OtaSelfTestStage::kRuntimeServices:
            return "runtime-services";
        case OtaSelfTestStage::kDispatchUnavailable:
            return "dispatch-unavailable";
        case OtaSelfTestStage::kConfirmation:
            return "confirmation";
    }
    return "unknown";
}

/**
 * @brief Format a bounded restart-snapshot reason for a failed OTA self-test
 *
 * @param result Failed self-test result
 * @param output Destination buffer
 * @param outputSize Destination capacity including the null terminator
 * @return true when the complete reason fit in the destination
 */
inline bool formatOtaSelfTestRestartReason(const OtaSelfTestResult& result, char* output,
                                           size_t outputSize) {
    if (result.passed() || output == nullptr || outputSize == 0) {
        return false;
    }

    int written = 0;
    if (result.stage == OtaSelfTestStage::kInternalHeap) {
        written = std::snprintf(output, outputSize, "ota verify failed: %s=%lu",
                                otaSelfTestStageName(result.stage),
                                static_cast<unsigned long>(result.detail));
    } else if (result.stage == OtaSelfTestStage::kHardware) {
        written = std::snprintf(output, outputSize, "ota verify failed: %s=%lu",
                                otaSelfTestStageName(result.stage),
                                static_cast<unsigned long>(result.detail));
    } else {
        written = std::snprintf(output, outputSize, "ota verify failed: %s",
                                otaSelfTestStageName(result.stage));
    }

    return written > 0 && static_cast<size_t>(written) < outputSize;
}

}  // namespace domes
