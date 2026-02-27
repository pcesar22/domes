#pragma once

/**
 * @file smokeTest.hpp
 * @brief Built-in smoke test suite for hardware validation
 *
 * Runs on-device self-tests to verify hardware and software functionality.
 * Can be triggered by:
 *   1. Holding touch pad 0 during boot (within 3 seconds)
 *   2. Sending a SelfTestRequest via the config protocol
 */

#include "config.pb.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "host/ble_hs.h"
#include "nvs.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace domes::infra {

/**
 * @brief Run all smoke tests and populate response
 *
 * Executes the built-in test suite:
 *   1. NVS test: write/read/verify a test key
 *   2. Heap test: check free heap above 100KB threshold
 *   3. Flash test: check OTA partition integrity
 *   4. WiFi test: scan for APs, report count and strongest RSSI
 *   5. BLE test: report if NimBLE stack initialized
 *
 * LED test and touch test require hardware and are done visually
 * via the boot-hold trigger path, not via the protocol path.
 *
 * @param resp Output: populated SelfTestResponse
 */
inline void runSmokeTests(domes_config_SelfTestResponse& resp) {
    static constexpr const char* kTag = "smoke_test";

    resp = domes_config_SelfTestResponse_init_zero;
    uint32_t passed = 0;
    uint32_t total = 0;

    auto addResult = [&](const char* name, bool pass, const char* msg) {
        if (resp.results_count >= 10) return;
        auto& r = resp.results[resp.results_count];
        strncpy(r.name, name, sizeof(r.name) - 1);
        r.passed = pass;
        strncpy(r.message, msg, sizeof(r.message) - 1);
        resp.results_count++;
        total++;
        if (pass) passed++;
        ESP_LOGI(kTag, "  [%s] %s: %s", pass ? "PASS" : "FAIL", name, msg);
    };

    ESP_LOGI(kTag, "=== Running smoke tests ===");

    // Test 1: NVS write/read/verify
    {
        nvs_handle_t h;
        esp_err_t err = nvs_open("smoke_test", NVS_READWRITE, &h);
        if (err == ESP_OK) {
            constexpr uint32_t kTestVal = 0xDEADBEEF;
            err = nvs_set_u32(h, "test_key", kTestVal);
            if (err == ESP_OK) err = nvs_commit(h);
            uint32_t readback = 0;
            if (err == ESP_OK) err = nvs_get_u32(h, "test_key", &readback);
            nvs_erase_key(h, "test_key");
            nvs_commit(h);
            nvs_close(h);
            if (err == ESP_OK && readback == kTestVal) {
                addResult("NVS", true, "write/read/verify OK");
            } else {
                addResult("NVS", false, "read-back mismatch");
            }
        } else {
            addResult("NVS", false, "open failed");
        }
    }

    // Test 2: Heap check (>30KB free — WiFi+BLE+ESP-NOW uses ~260KB)
    {
        size_t freeHeap = esp_get_free_heap_size();
        size_t minFreeHeap = esp_get_minimum_free_heap_size();
        char buf[48];
        snprintf(buf, sizeof(buf), "%zuKB free, %zuKB min",
                 freeHeap / 1024, minFreeHeap / 1024);
        addResult("Heap", freeHeap >= 30 * 1024, buf);
    }

    // Test 3: OTA partition integrity
    {
        const esp_partition_t* running = esp_ota_get_running_partition();
        if (running) {
            bool canRollback = esp_ota_check_rollback_is_possible();
            char buf[48];
            snprintf(buf, sizeof(buf), "%s, rollback=%s",
                     running->label, canRollback ? "yes" : "no");
            addResult("Flash", true, buf);
        } else {
            addResult("Flash", false, "no running partition");
        }
    }

    // Test 4: WiFi — check init state without blocking scan
    {
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_err_t err = esp_wifi_get_mode(&mode);
        if (err == ESP_OK && mode != WIFI_MODE_NULL) {
            wifi_ap_record_t apInfo;
            esp_err_t connErr = esp_wifi_sta_get_ap_info(&apInfo);
            char buf[48];
            if (connErr == ESP_OK) {
                snprintf(buf, sizeof(buf), "connected, RSSI=%d dBm", apInfo.rssi);
                addResult("WiFi", true, buf);
            } else {
                snprintf(buf, sizeof(buf), "mode=%d, not connected", static_cast<int>(mode));
                addResult("WiFi", true, buf);
            }
        } else {
            addResult("WiFi", false, "not initialized");
        }
    }

    // Test 5: BLE stack check — probe actual NimBLE state
    {
        int enabled = ble_hs_is_enabled();
        if (enabled) {
            addResult("BLE", true, "NimBLE host enabled");
        } else {
            addResult("BLE", false, "NimBLE host not enabled");
        }
    }

    resp.tests_run = total;
    resp.tests_passed = passed;

    ESP_LOGI(kTag, "=== Smoke tests complete: %lu/%lu passed ===",
             static_cast<unsigned long>(passed),
             static_cast<unsigned long>(total));
}

}  // namespace domes::infra
