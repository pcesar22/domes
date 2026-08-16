#pragma once

/**
 * @file crashDumpHandler.hpp
 * @brief Shutdown handler that saves diagnostic info to NVS on clean restart
 *
 * Uses esp_register_shutdown_handler() to capture backtrace PCs, task name,
 * uptime, and free heap to NVS namespace "crashdump" on clean shutdown
 * (e.g., esp_restart()). On next boot, the dump can be retrieved via
 * the config protocol (MSG_TYPE_GET_CRASH_DUMP_REQ).
 *
 * NOTE: This handler only fires on clean restarts, NOT on hard faults,
 * stack overflows, watchdog resets, or abort(). For capturing real panic
 * data, enable ESP-IDF's built-in coredump facility:
 *   menuconfig → Component config → Core dump → Data destination → Flash
 * Coredumps can then be retrieved with `espcoredump.py`.
 */

#include "esp_err.h"
#include "restartSnapshotRecord.hpp"

namespace domes::infra {

/// Legacy NVS namespace retained for restart-snapshot compatibility
constexpr const char* kCrashDumpNs = "crashdump";

/// NVS keys
namespace crash_key {
constexpr const char* kValid = "valid";                 ///< uint8_t: 1 if snapshot exists
constexpr const char* kReason = "reason";               ///< string: clean-restart reason
constexpr const char* kTaskName = "task_name";          ///< string: task active at restart
constexpr const char* kUptimeS = "uptime_s";            ///< uint32_t
constexpr const char* kFreeHeap = "free_heap";          ///< uint32_t
constexpr const char* kBacktrace = "backtrace";         ///< blob: PC values
constexpr const char* kBootCount = "boot_count";        ///< uint32_t: which boot
constexpr const char* kFirmwareVersion = "fw_version";  ///< string: pre-restart image version
constexpr const char* kElfSha256 = "elf_sha256";        ///< string: exact ELF SHA-256
constexpr const char* kFormatVersion = "format_ver";    ///< uint8_t: record schema version
constexpr const char* kRecordCrc = "record_crc";        ///< uint32_t: record integrity CRC
}  // namespace crash_key

/**
 * @brief Shutdown dump handler
 *
 * Registers a shutdown handler that captures diagnostic info to NVS
 * on clean restart (esp_restart()). Does NOT capture hard faults or
 * watchdog resets — use ESP-IDF coredump for those.
 */
class ShutdownDumpHandler {
public:
    /**
     * @brief Initialize shutdown dump handler
     *
     * Registers via esp_register_shutdown_handler() (clean restarts only).
     * Call once during startup.
     *
     * @param bootCount Current persisted boot count to capture on restart
     * @return ESP_OK on success
     */
    static esp_err_t init(uint32_t bootCount);

    /**
     * @brief Set the reason retained by the next clean restart
     *
     * The reason is copied into fixed storage and must fit in the existing
     * restart-snapshot field. Call from normal task context before a clean
     * restart; the default remains "shutdown/restart".
     *
     * @param reason Null-terminated reason string
     * @return ESP_OK on success or ESP_ERR_INVALID_ARG when it is empty or too long
     */
    static esp_err_t setRestartReason(const char* reason);

    /**
     * @brief Check if a restart snapshot exists in NVS
     */
    static bool hasDump();

    /**
     * @brief Load a restart snapshot from NVS
     *
     * @param dump Output struct to fill
     * @return ESP_OK if dump exists, ESP_ERR_NOT_FOUND otherwise
     */
    static esp_err_t loadDump(CrashDumpData& dump);

    /**
     * @brief Clear the restart snapshot from NVS
     *
     * @return ESP_OK on success
     */
    static esp_err_t clearDump();

private:
    /**
     * @brief Shutdown handler called on clean restart
     *
     * Captures backtrace, task name, uptime, free heap.
     * Writes to NVS. Must be safe to call in shutdown context.
     */
    static void shutdownHandler();

    static bool initialized_;
    static uint32_t bootCount_;
    static char restartReason_[kRestartReasonCapacity];
};

}  // namespace domes::infra
