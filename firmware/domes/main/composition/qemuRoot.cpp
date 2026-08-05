#include "config/modeManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game/gameEngine.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/taskStartEvidence.hpp"
#include "infra/taskTopology.hpp"
#include "mbedtls/sha256.h"
#include "platform/qemu/deterministicPlatformInputs.hpp"
#include "platform/qemu/qemuPeripheralAdapters.hpp"
#include "runtime/initOrderTracker.hpp"
#include "runtime/runtimeAssembly.hpp"
#include "services/imuService.hpp"
#include "trace/traceRecorder.hpp"

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kTag = "qemu_root";
constexpr TickType_t kReadinessTicks = pdMS_TO_TICKS(domes::runtime_profile::kReadinessDwellMs);

enum Failure : uint32_t {
    kTraceFailure = 1U << 0,
    kNvsFailure = 1U << 1,
    kInputFailure = 1U << 2,
    kAdapterInitFailure = 1U << 3,
    kAssemblyFailure = 1U << 4,
    kProgressFailure = 1U << 5,
    kTaskContractFailure = 1U << 6,
    kReadyStateFailure = 1U << 7,
    kTraceOverflowFailure = 1U << 8,
    kTargetTimeFailure = 1U << 9,
    kForbiddenTaskFailure = 1U << 10,
    kInitOrderFailure = 1U << 11,
};

std::atomic<uint32_t> gameHitCount{0};
std::atomic<uint32_t> gameMissCount{0};
std::atomic<uint32_t> gamePadMask{0};

void recordReadinessGameEvent(const domes::game::GameEvent& event) {
    if (event.type == domes::game::GameEvent::Type::kHit) {
        gameHitCount.fetch_add(1, std::memory_order_acq_rel);
        gamePadMask.fetch_or(1U << event.padIndex, std::memory_order_acq_rel);
    } else {
        gameMissCount.fetch_add(1, std::memory_order_acq_rel);
    }
}

bool advanceInitStage(domes::runtime::InitOrderTracker& initOrder, const char* stage) {
    if (initOrder.advance(stage)) {
        return true;
    }
    const char* expected = initOrder.expected();
    ESP_LOGE(kTag, "Init-order violation: expected=%s actual=%s",
             expected ? expected : "<complete>", stage ? stage : "<null>");
    return false;
}

bool hashText(const char* text, size_t size, char output[65]) {
    std::array<unsigned char, 32> digest{};
    if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(text), size, digest.data(), 0) != 0) {
        return false;
    }
    for (size_t index = 0; index < digest.size(); ++index) {
        std::snprintf(output + (index * 2), 3, "%02x", digest[index]);
    }
    output[64] = '\0';
    return true;
}

bool nvsRoundTrip() {
    if (domes::infra::NvsConfig::initFlash() != ESP_OK) {
        return false;
    }
    domes::infra::NvsConfig config;
    uint32_t actual = 0;
    constexpr uint32_t kExpected = 0x44564d31U;
    const bool passed = config.open("qemu_probe") == ESP_OK &&
                        config.setU32("sentinel", kExpected) == ESP_OK &&
                        config.commit() == ESP_OK && config.getU32("sentinel", actual) == ESP_OK &&
                        actual == kExpected;
    config.close();
    return passed;
}

size_t validateTasks(char snapshotHash[65]) {
    std::array<char, 1024> snapshot{};
    size_t offset = 0;
    size_t present = 0;
    for (const auto& expected : domes::runtime_profile::kRequiredTasks) {
        TaskHandle_t handle = xTaskGetHandle(expected.config->name);
        if (!handle) {
            continue;
        }
        const UBaseType_t priority = uxTaskPriorityGet(handle);
        const BaseType_t affinity = xTaskGetCoreID(handle);
        if (priority != expected.config->priority || affinity != expected.config->coreAffinity) {
            continue;
        }
        const int written =
            std::snprintf(snapshot.data() + offset, snapshot.size() - offset, "%s:%s:%lu:%ld;",
                          expected.id, expected.config->name, static_cast<unsigned long>(priority),
                          static_cast<long>(affinity));
        if (written <= 0 || static_cast<size_t>(written) >= snapshot.size() - offset) {
            return 0;
        }
        offset += static_cast<size_t>(written);
        ++present;
    }
    if (!hashText(snapshot.data(), offset, snapshotHash)) {
        return 0;
    }
    return present;
}

void emitResult(uint32_t failureMask, const domes::PlatformIdentity& identity,
                size_t randomConsumed, TickType_t tickStart, TickType_t tickEnd,
                const domes::platform::QemuAdapterEvidence& adapters, size_t presentTasks,
                const char* taskSnapshotSha, bool nvsPassed, uint32_t enabledMask,
                uint32_t startedTaskMask, uint32_t duplicateTaskMask, uint32_t core0TaskMask,
                uint32_t core1TaskMask, const char* gameState) {
    const char* status = failureMask == 0 ? "PASS" : "FAIL";
    const uint32_t traceCount =
        domes::trace::Recorder::isInitialized()
            ? static_cast<uint32_t>(domes::trace::Recorder::buffer().count())
            : 0;
    const uint32_t traceDrops = domes::trace::Recorder::isInitialized()
                                    ? domes::trace::Recorder::buffer().droppedCount()
                                    : 0;
    ESP_LOGI(
        kTag,
        "DOMES_QEMU_READY schema=1 status=%s profile=%s scenario=%s "
        "manifest_sha256=%s spec_sha256=%s "
        "sdkconfig_sha256=%s identity=%02x%02x%02x%02x%02x%02x random_consumed=%zu "
        "mode=idle supported_mask=0x%08" PRIx32 " enabled_mask=0x%08" PRIx32
        " expected_tasks=%zu present_tasks=%zu expected_task_mask=0x%08" PRIx32
        " started_task_mask=0x%08" PRIx32 " duplicate_task_mask=0x%08" PRIx32
        " core0_task_mask=0x%08" PRIx32 " core1_task_mask=0x%08" PRIx32
        " task_config_sha256=%s "
        "task_snapshot_sha256=%s tick_start=%" PRIu32 " tick_end=%" PRIu32 " tick_delta=%" PRIu32
        " cpu0_progress=%u cpu1_progress=%u "
        "adapter_init_mask=0x%08" PRIx32 " adapter_progress_mask=0x%08" PRIx32
        " game_state=%s game_hits=%" PRIu32 " game_misses=%" PRIu32 " game_pad_mask=0x%08" PRIx32
        " nvs_roundtrip=%u trace_count=%" PRIu32 " trace_drops=%" PRIu32
        " failure_mask=0x%08" PRIx32,
        status, domes::runtime_profile::kProfileName, domes::runtime_profile::kReadinessScenario,
        domes::runtime_profile::kManifestSha256, domes::runtime_profile::kSpecSha256,
        domes::runtime_profile::kSdkconfigSha256, identity[0], identity[1], identity[2],
        identity[3], identity[4], identity[5], randomConsumed,
        domes::runtime_profile::kSupportedFeatureMask, enabledMask,
        domes::runtime_profile::kRequiredTasks.size(), presentTasks,
        domes::runtime_profile::kRequiredTaskEvidenceMask, startedTaskMask, duplicateTaskMask,
        core0TaskMask, core1TaskMask, domes::runtime_profile::kTaskConfigSha256, taskSnapshotSha,
        static_cast<uint32_t>(tickStart), static_cast<uint32_t>(tickEnd),
        static_cast<uint32_t>(tickEnd - tickStart), core0TaskMask != 0 ? 1U : 0U,
        core1TaskMask != 0 ? 1U : 0U, adapters.initMask(), adapters.progressMask(), gameState,
        gameHitCount.load(std::memory_order_acquire), gameMissCount.load(std::memory_order_acquire),
        gamePadMask.load(std::memory_order_acquire), nvsPassed ? 1U : 0U, traceCount, traceDrops,
        failureMask);
}

}  // namespace

extern "C" void app_main() {
    domes::infra::TaskStartEvidence::markStarted(domes::infra::task::kMain);
    domes::runtime::InitOrderTracker initOrder;
    uint32_t failureMask = 0;
    domes::PlatformIdentity identity{};
    char taskSnapshotSha[65] = {};

    if (!advanceInitStage(initOrder, "trace")) {
        failureMask |= kInitOrderFailure;
    }
    if (domes::trace::Recorder::init() != ESP_OK) {
        failureMask |= kTraceFailure;
    } else {
        domes::trace::Recorder::setEnabled(true);
        domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main");
    }

    if (!advanceInitStage(initOrder, "nvs")) {
        failureMask |= kInitOrderFailure;
    }
    const bool nvsPassed = nvsRoundTrip();
    if (!nvsPassed) {
        failureMask |= kNvsFailure;
    }

    if (!advanceInitStage(initOrder, "platform_identity")) {
        failureMask |= kInitOrderFailure;
    }
    static domes::platform::FixedPlatformIdentity platformIdentity(
        domes::runtime_profile::kDeterministicIdentity);
    const bool identityPassed = platformIdentity.read(identity) == ESP_OK;

    if (!advanceInitStage(initOrder, "platform_random")) {
        failureMask |= kInitOrderFailure;
    }
    static domes::platform::RecordedRandomSource randomSource(
        domes::runtime_profile::kDeterministicRandom);
    uint32_t randomValue = 0;
    if (!identityPassed || randomSource.nextU32(randomValue) != ESP_OK ||
        randomValue != domes::runtime_profile::kDeterministicRandom.front() ||
        randomSource.remaining() != 0) {
        failureMask |= kInputFailure;
    }

    static domes::platform::QemuAdapterEvidence adapterEvidence;
    static domes::platform::QemuLedDriver led(adapterEvidence);
    static domes::platform::QemuImuDriver imu(adapterEvidence);
    static domes::platform::QemuHapticDriver haptic(adapterEvidence);
    static domes::platform::QemuAudioDriver audio(adapterEvidence);
    static domes::platform::QemuTouchDriver touch(adapterEvidence);
    bool adaptersReady = true;
    if (!advanceInitStage(initOrder, "led_adapter")) {
        failureMask |= kInitOrderFailure;
    }
    if (led.init() != ESP_OK) {
        adaptersReady = false;
    }
    if (!advanceInitStage(initOrder, "imu_adapter")) {
        failureMask |= kInitOrderFailure;
    }
    if (imu.init() != ESP_OK) {
        adaptersReady = false;
    }
    if (!advanceInitStage(initOrder, "haptic_adapter")) {
        failureMask |= kInitOrderFailure;
    }
    if (haptic.init() != ESP_OK) {
        adaptersReady = false;
    }
    if (!advanceInitStage(initOrder, "audio_adapter")) {
        failureMask |= kInitOrderFailure;
    }
    if (audio.init() != ESP_OK) {
        adaptersReady = false;
    }
    if (!advanceInitStage(initOrder, "touch_adapter")) {
        failureMask |= kInitOrderFailure;
    }
    if (touch.init() != ESP_OK) {
        adaptersReady = false;
    }
    if (!adaptersReady || adapterEvidence.initMask() != domes::platform::kAllQemuAdapterBits) {
        failureMask |= kAdapterInitFailure;
    }

    static domes::runtime::RuntimeAssembly runtime;
    bool assemblyReady = true;
    if (!advanceInitStage(initOrder, "feature_manager")) {
        failureMask |= kInitOrderFailure;
    }
    if (runtime.initFeatureManager(domes::runtime_profile::kSupportedFeatureMask, {}) != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "mode_manager")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initModeManager() != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "diagnostics")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initDiagnostics() != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "memory_profiler")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initMemoryProfiler() != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "led_service")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initLedService(led) != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "imu_service")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initImuService(imu) != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "audio_service")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && (runtime.initAudioService(audio) != ESP_OK ||
                          runtime.connectImuFeedback(&haptic, runtime.handles().audio) != ESP_OK)) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "touch")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initTouchService(touch) != ESP_OK) {
        assemblyReady = false;
    }
    if (!advanceInitStage(initOrder, "game_engine")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && runtime.initGameEngine(touch) != ESP_OK) {
        assemblyReady = false;
    }
    if (assemblyReady) {
        runtime.handles().game->setEventCallback(recordReadinessGameEvent);
    }
    if (!advanceInitStage(initOrder, "mode_idle")) {
        failureMask |= kInitOrderFailure;
    }
    if (assemblyReady && !runtime.transitionToIdle()) {
        assemblyReady = false;
    }
    if (!assemblyReady) {
        failureMask |= kAssemblyFailure;
    }

    if (!advanceInitStage(initOrder, "readiness_probe") || !initOrder.complete()) {
        failureMask |= kInitOrderFailure;
    }
    const TickType_t tickStart = xTaskGetTickCount();
    if ((failureMask & (kAssemblyFailure | kInitOrderFailure)) == 0) {
        domes::game::ArmConfig arm = {
            .timeoutMs = domes::runtime_profile::kReadinessGameTimeoutMs,
            .feedbackMode = domes::game::kFeedbackLed | domes::game::kFeedbackAudio};
        if (!runtime.handles().game->arm(arm) ||
            !runtime.handles().modes->transitionTo(domes::config::SystemMode::kGame)) {
            failureMask |= kProgressFailure;
        } else {
            imu.armSingleTap();
            touch.setTouched(domes::runtime_profile::kReadinessTouchPad, true);
            while ((xTaskGetTickCount() - tickStart) < kReadinessTicks) {
                if ((xTaskGetTickCount() - tickStart) >=
                    pdMS_TO_TICKS(domes::runtime_profile::kReadinessTouchReleaseMs)) {
                    touch.setTouched(domes::runtime_profile::kReadinessTouchPad, false);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            const uint32_t expectedGamePadMask = 1U << domes::runtime_profile::kReadinessTouchPad;
            if (runtime.handles().game->currentState() != domes::game::GameState::kReady ||
                gameHitCount.load(std::memory_order_acquire) != 1 ||
                gameMissCount.load(std::memory_order_acquire) != 0 ||
                gamePadMask.load(std::memory_order_acquire) != expectedGamePadMask) {
                failureMask |= kProgressFailure;
            }
            if (!runtime.transitionToIdle()) {
                failureMask |= kReadyStateFailure;
            }
        }
    }
    const TickType_t tickEnd = xTaskGetTickCount();

    if (tickEnd - tickStart < kReadinessTicks) {
        failureMask |= kTargetTimeFailure;
    }
    if (adapterEvidence.progressMask() != domes::platform::kAllQemuAdapterBits ||
        adapterEvidence.coreProgress(1) == 0) {
        failureMask |= kProgressFailure;
    }

    const size_t presentTasks = validateTasks(taskSnapshotSha);
    if (presentTasks != domes::runtime_profile::kRequiredTasks.size()) {
        failureMask |= kTaskContractFailure;
    }
    for (const char* taskName : domes::runtime_profile::kAbsentTaskNames) {
        if (xTaskGetHandle(taskName) != nullptr) {
            failureMask |= kForbiddenTaskFailure;
        }
    }

    const uint32_t startedTaskMask = domes::infra::TaskStartEvidence::startedMask();
    const uint32_t duplicateTaskMask = domes::infra::TaskStartEvidence::duplicateMask();
    const uint32_t core0TaskMask = domes::infra::TaskStartEvidence::coreMask(0);
    const uint32_t core1TaskMask = domes::infra::TaskStartEvidence::coreMask(1);
    if (startedTaskMask != domes::runtime_profile::kRequiredTaskEvidenceMask ||
        duplicateTaskMask != 0 || core0TaskMask == 0 || core1TaskMask == 0 ||
        (core0TaskMask & core1TaskMask) != 0 ||
        (core0TaskMask | core1TaskMask) != startedTaskMask ||
        (core0TaskMask & domes::runtime_profile::kCore1RequiredTaskEvidenceMask) != 0 ||
        (core1TaskMask & domes::runtime_profile::kCore0RequiredTaskEvidenceMask) != 0 ||
        (core0TaskMask & domes::runtime_profile::kCore0RequiredTaskEvidenceMask) !=
            domes::runtime_profile::kCore0RequiredTaskEvidenceMask ||
        (core1TaskMask & domes::runtime_profile::kCore1RequiredTaskEvidenceMask) !=
            domes::runtime_profile::kCore1RequiredTaskEvidenceMask ||
        ((core0TaskMask | core1TaskMask) &
         domes::runtime_profile::kUnpinnedRequiredTaskEvidenceMask) !=
            domes::runtime_profile::kUnpinnedRequiredTaskEvidenceMask) {
        failureMask |= kTaskContractFailure;
    }

    if (!runtime.handles().modes ||
        runtime.handles().modes->currentMode() != domes::config::SystemMode::kIdle ||
        !runtime.handles().features ||
        runtime.handles().features->getMask() != domes::runtime_profile::kReadyEnabledFeatureMask) {
        failureMask |= kReadyStateFailure;
    }
    if (domes::trace::Recorder::isInitialized() &&
        domes::trace::Recorder::buffer().droppedCount() != 0) {
        failureMask |= kTraceOverflowFailure;
    }

    const uint32_t enabledMask =
        runtime.handles().features ? runtime.handles().features->getMask() : 0;
    const char* gameState =
        runtime.handles().game
            ? domes::game::gameStateToString(runtime.handles().game->currentState())
            : "UNAVAILABLE";
    emitResult(failureMask, identity, randomSource.consumed(), tickStart, tickEnd, adapterEvidence,
               presentTasks, taskSnapshotSha, nvsPassed, enabledMask, startedTaskMask,
               duplicateTaskMask, core0TaskMask, core1TaskMask, gameState);
}
