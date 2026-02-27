/**
 * @file crashDumpHandler.cpp
 * @brief Shutdown dump handler implementation
 */

#include "crashDumpHandler.hpp"

#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>

namespace {
constexpr const char* kTag = "crash_dump";
}

namespace domes::infra {

bool ShutdownDumpHandler::initialized_ = false;

esp_err_t ShutdownDumpHandler::init() {
    if (initialized_) {
        return ESP_OK;
    }

    esp_err_t err = esp_register_shutdown_handler(shutdownHandler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register shutdown handler: %s", esp_err_to_name(err));
        return err;
    }

    // Log if we have a crash dump from a previous run
    if (hasDump()) {
        CrashDumpData dump;
        if (loadDump(dump) == ESP_OK) {
            ESP_LOGW(kTag, "*** Previous crash dump found ***");
            ESP_LOGW(kTag, "  Reason: %s", dump.reason);
            ESP_LOGW(kTag, "  Task: %s", dump.taskName);
            ESP_LOGW(kTag, "  Uptime: %lu s", static_cast<unsigned long>(dump.uptimeS));
            ESP_LOGW(kTag, "  Free heap: %lu", static_cast<unsigned long>(dump.freeHeap));
            ESP_LOGW(kTag, "  Backtrace depth: %u", dump.backtraceDepth);
            for (uint8_t i = 0; i < dump.backtraceDepth; ++i) {
                ESP_LOGW(kTag, "    PC[%u]: 0x%08lX", i,
                         static_cast<unsigned long>(dump.backtrace[i]));
            }
        }
    }

    initialized_ = true;
    ESP_LOGI(kTag, "Crash dump handler initialized");
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
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t valid = 0;
    err = nvs_get_u8(handle, crash_key::kValid, &valid);
    if (err != ESP_OK || valid != 1) {
        nvs_close(handle);
        return ESP_ERR_NOT_FOUND;
    }

    dump.valid = true;

    // Load reason string
    size_t len = sizeof(dump.reason);
    if (nvs_get_str(handle, crash_key::kReason, dump.reason, &len) != ESP_OK) {
        std::strncpy(dump.reason, "unknown", sizeof(dump.reason));
    }

    // Load task name
    len = sizeof(dump.taskName);
    if (nvs_get_str(handle, crash_key::kTaskName, dump.taskName, &len) != ESP_OK) {
        std::strncpy(dump.taskName, "unknown", sizeof(dump.taskName));
    }

    // Load scalars
    nvs_get_u32(handle, crash_key::kUptimeS, &dump.uptimeS);
    nvs_get_u32(handle, crash_key::kFreeHeap, &dump.freeHeap);
    nvs_get_u32(handle, crash_key::kBootCount, &dump.bootCount);

    // Load backtrace blob
    len = sizeof(dump.backtrace);
    err = nvs_get_blob(handle, crash_key::kBacktrace, dump.backtrace, &len);
    if (err == ESP_OK) {
        dump.backtraceDepth = static_cast<uint8_t>(len / sizeof(uint32_t));
    } else {
        dump.backtraceDepth = 0;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ShutdownDumpHandler::clearDump() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kCrashDumpNs, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_erase_all(handle);
    err = nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(kTag, "Crash dump cleared");
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

    // Mark dump as valid
    nvs_set_u8(handle, crash_key::kValid, 1);

    // Save reason — generic since this only fires on clean restart
    nvs_set_str(handle, crash_key::kReason, "shutdown/restart");

    // Save task name
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask != nullptr) {
        const char* name = pcTaskGetName(currentTask);
        if (name != nullptr) {
            nvs_set_str(handle, crash_key::kTaskName, name);
        } else {
            nvs_set_str(handle, crash_key::kTaskName, "unknown");
        }
    } else {
        nvs_set_str(handle, crash_key::kTaskName, "none");
    }

    // Save uptime
    uint32_t uptimeS = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);
    nvs_set_u32(handle, crash_key::kUptimeS, uptimeS);

    // Save free heap
    uint32_t freeHeap = static_cast<uint32_t>(esp_get_free_heap_size());
    nvs_set_u32(handle, crash_key::kFreeHeap, freeHeap);

    // Capture backtrace
    esp_backtrace_frame_t frame;
    esp_backtrace_get_start(&frame.pc, &frame.sp, &frame.next_pc);

    uint32_t backtrace[kMaxBacktraceDepth];
    uint8_t depth = 0;

    // First PC is the current one
    backtrace[depth++] = frame.pc;
    if (depth < kMaxBacktraceDepth) {
        backtrace[depth++] = frame.next_pc;
    }

    // Walk the stack
    while (depth < kMaxBacktraceDepth && esp_backtrace_get_next_frame(&frame)) {
        backtrace[depth++] = frame.pc;
    }

    nvs_set_blob(handle, crash_key::kBacktrace, backtrace,
                 depth * sizeof(uint32_t));

    nvs_set_u32(handle, crash_key::kBootCount, 0);  // Will be updated on next boot

    nvs_commit(handle);
    nvs_close(handle);
}

}  // namespace domes::infra
