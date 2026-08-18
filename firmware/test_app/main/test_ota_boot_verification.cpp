/**
 * @file test_ota_boot_verification.cpp
 * @brief Tests for retained first-boot OTA verification diagnostics
 */

#include "services/otaBootVerification.hpp"

#include <array>

#include <gtest/gtest.h>

TEST(OtaBootVerification, DefaultResultPasses) {
    const domes::OtaSelfTestResult result;

    EXPECT_TRUE(result.passed());
    EXPECT_EQ(result.stage, domes::OtaSelfTestStage::kNone);
    EXPECT_EQ(result.detail, 0u);
}

TEST(OtaBootVerification, FailureRetainsStageStatusAndDetail) {
    const auto result =
        domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kInternalHeap, ESP_FAIL, 28123);

    EXPECT_FALSE(result.passed());
    EXPECT_EQ(result.status, ESP_FAIL);
    EXPECT_EQ(result.stage, domes::OtaSelfTestStage::kInternalHeap);
    EXPECT_EQ(result.detail, 28123u);
}

TEST(OtaBootVerification, FormatsStableStageNames) {
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kInjectedFailure),
                 "injected");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kWatchdog), "watchdog");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kNvs), "nvs");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kInternalHeap),
                 "internal-heap");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kHardware), "hardware");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kLedOutput), "led-output");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kRuntimeServices),
                 "runtime-services");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kDispatchUnavailable),
                 "dispatch-unavailable");
    EXPECT_STREQ(domes::otaSelfTestStageName(domes::OtaSelfTestStage::kConfirmation),
                 "confirmation");
}

TEST(OtaBootVerification, FormatsHeapAndHardwareDetails) {
    std::array<char, 64> reason = {};
    auto result =
        domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kInternalHeap, ESP_FAIL, 28123);

    EXPECT_TRUE(domes::formatOtaSelfTestRestartReason(result, reason.data(), reason.size()));
    EXPECT_STREQ(reason.data(), "ota verify failed: internal-heap=28123");

    reason.fill('\0');
    result = domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kHardware, ESP_FAIL, 4);
    EXPECT_TRUE(domes::formatOtaSelfTestRestartReason(result, reason.data(), reason.size()));
    EXPECT_STREQ(reason.data(), "ota verify failed: hardware=4");
}

TEST(OtaBootVerification, RejectsPassingOrTruncatedReason) {
    std::array<char, 64> reason = {};
    const domes::OtaSelfTestResult passing;
    EXPECT_FALSE(domes::formatOtaSelfTestRestartReason(passing, reason.data(), reason.size()));

    const auto failure =
        domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kRuntimeServices);
    std::array<char, 8> smallReason = {};
    EXPECT_FALSE(
        domes::formatOtaSelfTestRestartReason(failure, smallReason.data(), smallReason.size()));
}

TEST(OtaBootVerification, DoesNotRetryNonHeapFailures) {
    uint32_t selfTestCalls = 0;
    uint32_t retryCalls = 0;

    const auto result = domes::runOtaSelfTestWithRetry(
        [&]() {
            ++selfTestCalls;
            return domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kHardware);
        },
        [&](uint8_t, const domes::OtaSelfTestResult&) { ++retryCalls; });

    EXPECT_FALSE(result.passed());
    EXPECT_EQ(result.stage, domes::OtaSelfTestStage::kHardware);
    EXPECT_EQ(selfTestCalls, 1u);
    EXPECT_EQ(retryCalls, 0u);
}

TEST(OtaBootVerification, FormatsDispatchFailureForRetention) {
    std::array<char, 64> reason = {};
    const auto result =
        domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kDispatchUnavailable);

    EXPECT_TRUE(domes::formatOtaSelfTestRestartReason(result, reason.data(), reason.size()));
    EXPECT_STREQ(reason.data(), "ota verify failed: dispatch-unavailable");
}

TEST(OtaBootVerification, DispatchesBootCompletionForPendingAndValidImages) {
    EXPECT_EQ(domes::otaStartupDispatchAction(true, ESP_OK),
              domes::OtaStartupDispatchAction::kScheduled);
    EXPECT_EQ(domes::otaStartupDispatchAction(false, ESP_OK),
              domes::OtaStartupDispatchAction::kScheduled);
}

TEST(OtaBootVerification, FailsClosedWithoutOffOwnerBootCompletion) {
    EXPECT_EQ(domes::otaStartupDispatchAction(true, ESP_ERR_INVALID_STATE),
              domes::OtaStartupDispatchAction::kRollbackPendingImage);
    EXPECT_EQ(domes::otaStartupDispatchAction(false, ESP_ERR_INVALID_STATE),
              domes::OtaStartupDispatchAction::kLeaveBootIncomplete);
}

TEST(OtaBootVerification, RecoversAfterBoundedHeapRetry) {
    uint32_t selfTestCalls = 0;
    uint32_t retryCalls = 0;
    uint8_t completedAttempts = 0;

    const auto result = domes::runOtaSelfTestWithRetry(
        [&]() {
            ++selfTestCalls;
            if (selfTestCalls == 1) {
                return domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kInternalHeap,
                                                         ESP_FAIL, 26499);
            }
            return domes::OtaSelfTestResult{};
        },
        [&](uint8_t completed, const domes::OtaSelfTestResult&) {
            ++retryCalls;
            completedAttempts = completed;
        });

    EXPECT_TRUE(result.passed());
    EXPECT_EQ(selfTestCalls, 2u);
    EXPECT_EQ(retryCalls, 1u);
    EXPECT_EQ(completedAttempts, 1u);
}

TEST(OtaBootVerification, StopsAfterMaximumHeapAttempts) {
    uint32_t selfTestCalls = 0;
    uint32_t retryCalls = 0;

    const auto result = domes::runOtaSelfTestWithRetry(
        [&]() {
            ++selfTestCalls;
            return domes::OtaSelfTestResult::failure(domes::OtaSelfTestStage::kInternalHeap,
                                                     ESP_FAIL, 27000 + selfTestCalls);
        },
        [&](uint8_t, const domes::OtaSelfTestResult&) { ++retryCalls; });

    EXPECT_FALSE(result.passed());
    EXPECT_EQ(result.stage, domes::OtaSelfTestStage::kInternalHeap);
    EXPECT_EQ(result.detail, 27000u + domes::kOtaSelfTestMaxAttempts);
    EXPECT_EQ(selfTestCalls, domes::kOtaSelfTestMaxAttempts);
    EXPECT_EQ(retryCalls, static_cast<uint32_t>(domes::kOtaSelfTestMaxAttempts - 1));
}
