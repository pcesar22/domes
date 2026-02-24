#pragma once

/**
 * @file crashDumpHandler.hpp
 * @brief Panic handler that saves crash info to NVS for later retrieval
 *
 * On panic: captures backtrace PCs, reason, task name, uptime, and free heap
 * to NVS namespace "crashdump". On next boot, the dump can be retrieved via
 * the config protocol (MSG_TYPE_GET_CRASH_DUMP_REQ).
 */

#include "esp_err.h"

#include <array>
#include <cstdint>

namespace domes::infra {

/// Maximum backtrace depth
constexpr size_t kMaxBacktraceDepth = 16;

/// NVS namespace for crash dumps
constexpr const char* kCrashDumpNs = "crashdump";

/// NVS keys
namespace crash_key {
constexpr const char* kValid = "valid";          ///< uint8_t: 1 if dump exists
constexpr const char* kReason = "reason";        ///< string: panic reason
constexpr const char* kTaskName = "task_name";   ///< string: crashed task
constexpr const char* kUptimeS = "uptime_s";     ///< uint32_t
constexpr const char* kFreeHeap = "free_heap";   ///< uint32_t
constexpr const char* kBacktrace = "backtrace";  ///< blob: PC values
constexpr const char* kBootCount = "boot_count"; ///< uint32_t: which boot
}  // namespace crash_key

/**
 * @brief Stored crash dump data
 */
struct CrashDumpData {
    bool valid = false;
    char reason[64] = {};
    char taskName[16] = {};
    uint32_t uptimeS = 0;
    uint32_t freeHeap = 0;
    uint32_t backtrace[kMaxBacktraceDepth] = {};
    uint8_t backtraceDepth = 0;
    uint32_t bootCount = 0;
};

/**
 * @brief Crash dump handler
 *
 * Registers a shutdown handler that captures panic info to NVS.
 * On boot, check for existing crash dump and report it.
 */
class CrashDumpHandler {
public:
    /**
     * @brief Initialize crash dump handler
     *
     * Registers the panic handler via esp_register_shutdown_handler().
     * Call once during startup.
     *
     * @return ESP_OK on success
     */
    static esp_err_t init();

    /**
     * @brief Check if a crash dump exists in NVS
     */
    static bool hasDump();

    /**
     * @brief Load crash dump from NVS
     *
     * @param dump Output struct to fill
     * @return ESP_OK if dump exists, ESP_ERR_NOT_FOUND otherwise
     */
    static esp_err_t loadDump(CrashDumpData& dump);

    /**
     * @brief Clear crash dump from NVS
     *
     * @return ESP_OK on success
     */
    static esp_err_t clearDump();

private:
    /**
     * @brief Shutdown handler called on panic
     *
     * Captures backtrace, reason, task name, uptime, free heap.
     * Writes to NVS. Must be safe to call in panic context.
     */
    static void panicHandler();

    static bool initialized_;
};

}  // namespace domes::infra
