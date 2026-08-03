/**
 * @file crashDumpHandler.cpp
 * @brief Shutdown dump handler implementation
 */

#include "crashDumpHandler.hpp"

#include "appMetadata.hpp"
#include "esp_cpu_utils.h"
#include "esp_debug_helpers.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <cstring>

namespace domes::infra {

namespace {
constexpr const char* kTag = "crash_dump";

void encodeElfSha256(char (&output)[65]) {
    constexpr char kHex[] = "0123456789abcdef";
    const auto& digest = appMetadata().app_elf_sha256;
    for (size_t i = 0; i < sizeof(appMetadata().app_elf_sha256); ++i) {
        output[i * 2] = kHex[digest[i] >> 4];
        output[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    output[64] = '\0';
}
}  // namespace

bool ShutdownDumpHandler::initialized_ = false;
uint32_t ShutdownDumpHandler::bootCount_ = 0;

esp_err_t ShutdownDumpHandler::init(uint32_t bootCount) {
    bootCount_ = bootCount;
    if (initialized_) {
        return ESP_OK;
    }

    esp_err_t err = esp_register_shutdown_handler(shutdownHandler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register shutdown handler: %s", esp_err_to_name(err));
        return err;
    }

    // Log the clean-restart snapshot from the previous run, if present.
    if (hasDump()) {
        CrashDumpData dump;
        const esp_err_t loadErr = loadDump(dump);
        if (loadErr == ESP_OK) {
            ESP_LOGW(kTag, "*** Previous restart snapshot found ***");
            ESP_LOGW(kTag, "  Reason: %s", dump.reason);
            ESP_LOGW(kTag, "  Task: %s", dump.taskName);
            ESP_LOGW(kTag, "  Uptime: %lu s", static_cast<unsigned long>(dump.uptimeS));
            if (dump.formatVersion == 0) {
                ESP_LOGW(kTag, "  Legacy free-heap value: %lu",
                         static_cast<unsigned long>(dump.freeHeap));
            } else {
                ESP_LOGW(kTag, "  Internal free heap: %lu",
                         static_cast<unsigned long>(dump.freeHeap));
                ESP_LOGW(kTag, "  Firmware: %s", dump.firmwareVersion);
                ESP_LOGW(kTag, "  ELF SHA-256: %s", dump.elfSha256);
            }
            ESP_LOGW(kTag, "  Backtrace depth: %u", dump.backtraceDepth);
            for (uint8_t i = 0; i < dump.backtraceDepth; ++i) {
                ESP_LOGW(kTag, "    PC[%u]: 0x%08lX", i,
                         static_cast<unsigned long>(dump.backtrace[i]));
            }
        } else {
            ESP_LOGE(kTag, "Stored restart snapshot is invalid: %s", esp_err_to_name(loadErr));
        }
    }

    initialized_ = true;
    ESP_LOGI(kTag, "Restart snapshot handler initialized");
    return ESP_OK;
}

bool ShutdownDumpHandler::hasDump() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    uint8_t valid = 0;
    err = nvs_get_u8(handle, crash_key::kValid, &valid);
    nvs_close(handle);
    return (err == ESP_OK && valid == 1);
}

esp_err_t ShutdownDumpHandler::loadDump(CrashDumpData& dump) {
    dump = {};

    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint8_t valid = 0;
    err = nvs_get_u8(handle, crash_key::kValid, &valid);
    if (err == ESP_ERR_NVS_NOT_FOUND || (err == ESP_OK && valid != 1)) {
        nvs_close(handle);
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    size_t len = sizeof(dump.reason);
    err = nvs_get_str(handle, crash_key::kReason, dump.reason, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    len = sizeof(dump.taskName);
    err = nvs_get_str(handle, crash_key::kTaskName, dump.taskName, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_get_u32(handle, crash_key::kUptimeS, &dump.uptimeS);
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, crash_key::kFreeHeap, &dump.freeHeap);
    }
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, crash_key::kBootCount, &dump.bootCount);
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    len = sizeof(dump.backtrace);
    err = nvs_get_blob(handle, crash_key::kBacktrace, dump.backtrace, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    if (len == 0 || len > sizeof(dump.backtrace) || len % sizeof(uint32_t) != 0) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    dump.backtraceDepth = static_cast<uint8_t>(len / sizeof(uint32_t));

    err = nvs_get_u8(handle, crash_key::kFormatVersion, &dump.formatVersion);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Pre-format records used total heap and raw return addresses. Do not
        // associate independently retained metadata with those legacy fields.
        sanitizeLegacyRestartSnapshot(dump);
        nvs_close(handle);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    if (!isSupportedRestartSnapshotFormat(dump.formatVersion)) {
        nvs_close(handle);
        return ESP_ERR_INVALID_VERSION;
    }

    len = sizeof(dump.firmwareVersion);
    err = nvs_get_str(handle, crash_key::kFirmwareVersion, dump.firmwareVersion, &len);
    if (err == ESP_OK) {
        len = sizeof(dump.elfSha256);
        err = nvs_get_str(handle, crash_key::kElfSha256, dump.elfSha256, &len);
    }
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, crash_key::kRecordCrc, &dump.recordCrc);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }
    if (!restartSnapshotCrcMatches(dump)) {
        return ESP_ERR_INVALID_CRC;
    }

    dump.valid = true;
    return ESP_OK;
}

esp_err_t ShutdownDumpHandler::clearDump() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(kTag, "Restart snapshot cleared");
    }
    return err;
}

void ShutdownDumpHandler::shutdownHandler() {
    // This runs in shutdown context (clean esp_restart() only).
    // Minimal operations, no logging, no allocs.

    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;  // Can't write, bail out
    }

    // Resolve values before starting the NVS transaction.
    const char* taskName = "none";
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask != nullptr) {
        const char* name = pcTaskGetName(currentTask);
        if (name != nullptr) {
            taskName = name;
        } else {
            taskName = "unknown";
        }
    }

    CrashDumpData record;
    record.formatVersion = kRestartSnapshotFormatVersion;
    std::strncpy(record.reason, "shutdown/restart", sizeof(record.reason) - 1);
    std::strncpy(record.taskName, taskName, sizeof(record.taskName) - 1);
    std::strncpy(record.firmwareVersion, firmwareVersion(), sizeof(record.firmwareVersion) - 1);
    encodeElfSha256(record.elfSha256);
    record.bootCount = bootCount_;
    record.uptimeS = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);
    constexpr uint32_t kInternalHeapCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    record.freeHeap = static_cast<uint32_t>(heap_caps_get_free_size(kInternalHeapCaps));

    esp_backtrace_frame_t frame = {};
    esp_backtrace_get_start(&frame.pc, &frame.sp, &frame.next_pc);

    record.backtrace[record.backtraceDepth++] = esp_cpu_process_stack_pc(frame.pc);
    while (record.backtraceDepth < kMaxBacktraceDepth && frame.next_pc != 0 &&
           esp_backtrace_get_next_frame(&frame)) {
        record.backtrace[record.backtraceDepth++] = esp_cpu_process_stack_pc(frame.pc);
    }
    record.recordCrc = calculateRestartSnapshotCrc(record);

    // Commit an invalid marker first so a failed replacement cannot expose stale data.
    err = nvs_set_u8(handle, crash_key::kValid, 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return;
    }

    // Remove every old-format key before publishing the replacement. The CRC
    // also makes a later write by downgraded firmware fail closed on read.
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, crash_key::kReason, record.reason);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, crash_key::kTaskName, record.taskName);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, crash_key::kUptimeS, record.uptimeS);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, crash_key::kFreeHeap, record.freeHeap);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, crash_key::kBootCount, record.bootCount);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, crash_key::kFirmwareVersion, record.firmwareVersion);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, crash_key::kElfSha256, record.elfSha256);
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, crash_key::kBacktrace, record.backtrace,
                           record.backtraceDepth * sizeof(uint32_t));
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, crash_key::kFormatVersion, record.formatVersion);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, crash_key::kRecordCrc, record.recordCrc);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, crash_key::kValid, 1);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err != ESP_OK) {
        nvs_set_u8(handle, crash_key::kValid, 0);
        nvs_commit(handle);
    }
    nvs_close(handle);
}

}  // namespace domes::infra
