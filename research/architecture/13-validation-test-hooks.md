# 13 - Validation Test Hooks

## AI Agent Instructions

Load this file when:
- Setting up milestone validation
- Implementing hardware-in-loop testing
- Creating test commands and diagnostics
- Building multi-pod test harnesses

Prerequisites: `06-testing.md`, `12-system-policies.md`

---

## Overview

This document defines test hooks for validating each development milestone. Test hooks are:
- **Serial commands** for triggering tests
- **BLE characteristics** for phone-triggered validation
- **Automated scripts** for CI/nightly testing
- **Built-in diagnostics** for field debugging

---

## Milestone Test Matrix

| Milestone | Automated | Manual | Hardware Required |
|-----------|-----------|--------|-------------------|
| M1: Compiles | ✅ CI | - | No |
| M2: Boots | ✅ CI + Script | Visual | 1 DevKit |
| M3: Debug | ✅ Script | GDB session | 1 DevKit |
| M4: RF Valid | ✅ Script | - | 1 DevKit |
| M5: Unit Tests | ✅ CI | - | No (host) |
| M6: Latency | ✅ Script | - | 2+ DevKits |
| M7: PCB Works | ⚠️ Semi-auto | Visual/Audio | Custom PCB |
| M8: 6-Pod Demo | ⚠️ Semi-auto | Visual | 6 Pods |
| M9: Demo Ready | Manual | Full | 6 Pods + Phone |

---

## Serial Command Interface

### Command Protocol

```cpp
// utils/serial_commands.hpp
#pragma once

#include "esp_err.h"
#include <functional>

namespace domes {

/**
 * @brief Serial command handler registry
 *
 * Commands are entered via UART console with format:
 *   command_name [arg1] [arg2] ...
 *
 * Example: "test_led 255 0 0" sets all LEDs to red
 */
class SerialCommands {
public:
    using Handler = std::function<void(int argc, char** argv)>;

    /**
     * @brief Register a command handler
     */
    static void registerCommand(const char* name, Handler handler,
                                 const char* help = nullptr);

    /**
     * @brief Initialize serial command processor
     */
    static esp_err_t init();

    /**
     * @brief Process incoming command (call from UART task)
     */
    static void processLine(const char* line);

private:
    static constexpr size_t kMaxCommands = 50;
    static constexpr size_t kMaxArgs = 10;
};

}  // namespace domes
```

### Core Test Commands

```cpp
// test/serial_test_commands.cpp
#include "serial_commands.hpp"

void registerTestCommands() {
    SerialCommands::registerCommand("help", cmdHelp,
        "List all commands");

    // === M2: Boot Validation ===
    SerialCommands::registerCommand("boot_info", cmdBootInfo,
        "Show boot status, heap, PSRAM");

    SerialCommands::registerCommand("health_check", cmdHealthCheck,
        "Run quick health check on all subsystems");

    // === M3: Debug Infrastructure ===
    SerialCommands::registerCommand("crash_test", cmdCrashTest,
        "Trigger intentional crash for core dump test");

    SerialCommands::registerCommand("assert_test", cmdAssertTest,
        "Trigger assertion failure");

    SerialCommands::registerCommand("stack_check", cmdStackCheck,
        "Check task stack high water marks");

    // === M4: RF Validation ===
    SerialCommands::registerCommand("wifi_scan", cmdWifiScan,
        "Scan for nearby WiFi networks");

    SerialCommands::registerCommand("ble_advertise", cmdBleAdvertise,
        "Start/stop BLE advertising");

    SerialCommands::registerCommand("coex_test", cmdCoexTest,
        "Run WiFi+BLE coexistence stress test");

    SerialCommands::registerCommand("espnow_ping", cmdEspNowPing,
        "Ping another pod: espnow_ping <mac>");

    // === M5: Unit Test Hooks ===
    SerialCommands::registerCommand("run_tests", cmdRunTests,
        "Run built-in unit tests");

    // === M6: Latency Validation ===
    SerialCommands::registerCommand("latency_test", cmdLatencyTest,
        "Run ESP-NOW latency test: latency_test <peer_mac> <count>");

    SerialCommands::registerCommand("sync_stats", cmdSyncStats,
        "Show clock synchronization statistics");

    // === M7: Smoke Tests ===
    SerialCommands::registerCommand("test_all", cmdTestAll,
        "Run full smoke test suite");

    SerialCommands::registerCommand("test_led", cmdTestLed,
        "Test LEDs: test_led [r] [g] [b] [w]");

    SerialCommands::registerCommand("test_audio", cmdTestAudio,
        "Test audio: test_audio [freq_hz] [duration_ms]");

    SerialCommands::registerCommand("test_haptic", cmdTestHaptic,
        "Test haptic: test_haptic [effect_id]");

    SerialCommands::registerCommand("test_touch", cmdTestTouch,
        "Test touch detection (5s timeout)");

    SerialCommands::registerCommand("test_imu", cmdTestImu,
        "Test IMU readings and tap detection");

    SerialCommands::registerCommand("test_battery", cmdTestBattery,
        "Show battery voltage and percentage");

    // === M8: Multi-Pod Tests ===
    SerialCommands::registerCommand("network_info", cmdNetworkInfo,
        "Show network topology and peers");

    SerialCommands::registerCommand("sync_flash", cmdSyncFlash,
        "Synchronized LED flash across all pods");

    SerialCommands::registerCommand("touch_relay", cmdTouchRelay,
        "Touch propagation test");

    // === Diagnostics ===
    SerialCommands::registerCommand("mem_stats", cmdMemStats,
        "Show memory statistics");

    SerialCommands::registerCommand("task_stats", cmdTaskStats,
        "Show FreeRTOS task statistics");

    SerialCommands::registerCommand("nvs_dump", cmdNvsDump,
        "Dump NVS contents");

    SerialCommands::registerCommand("gpio_test", cmdGpioTest,
        "Test GPIO: gpio_test <pin> <mode> [value]");
}
```

---

## M2: Boot Validation

### Automated Boot Check

```cpp
// test/boot_validation.cpp

struct BootValidation {
    bool heapOk;
    bool psramOk;
    bool nvsOk;
    bool taskStarted;
    uint32_t bootTimeMs;
    size_t freeHeap;
    size_t freePsram;
};

BootValidation validateBoot() {
    BootValidation result = {};

    uint32_t startMs = esp_log_timestamp();

    // Check heap
    result.freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    result.heapOk = result.freeHeap > 100000;  // > 100KB free

    // Check PSRAM
    result.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    result.psramOk = result.freePsram > 4000000;  // > 4MB free

    // Check NVS
    nvs_handle_t handle;
    result.nvsOk = nvs_open("test", NVS_READONLY, &handle) == ESP_OK ||
                   nvs_open("test", NVS_READONLY, &handle) == ESP_ERR_NVS_NOT_FOUND;

    result.bootTimeMs = esp_log_timestamp();
    result.taskStarted = true;

    return result;
}

void cmdBootInfo(int argc, char** argv) {
    auto v = validateBoot();

    ESP_LOGI(kTag, "=== Boot Validation ===");
    ESP_LOGI(kTag, "Boot time: %lu ms", v.bootTimeMs);
    ESP_LOGI(kTag, "Heap: %d bytes free %s", v.freeHeap, v.heapOk ? "OK" : "LOW");
    ESP_LOGI(kTag, "PSRAM: %d bytes free %s", v.freePsram, v.psramOk ? "OK" : "LOW");
    ESP_LOGI(kTag, "NVS: %s", v.nvsOk ? "OK" : "FAIL");
    ESP_LOGI(kTag, "All checks: %s",
             (v.heapOk && v.psramOk && v.nvsOk) ? "PASS" : "FAIL");

    // Output machine-readable line for scripts
    printf("BOOT_RESULT:%d:%d:%d:%lu\n",
           v.heapOk, v.psramOk, v.nvsOk, v.bootTimeMs);
}
```

### CI Boot Validation Script

```bash
#!/bin/bash
# scripts/validate_boot.sh

PORT=${1:-/dev/ttyUSB0}
TIMEOUT=30

echo "Flashing firmware..."
idf.py -p $PORT flash || exit 1

echo "Waiting for boot..."
timeout $TIMEOUT cat $PORT | while read line; do
    echo "$line"
    if [[ "$line" == *"BOOT_RESULT:"* ]]; then
        # Parse: BOOT_RESULT:heap:psram:nvs:time
        IFS=':' read -ra PARTS <<< "$line"
        HEAP=${PARTS[1]}
        PSRAM=${PARTS[2]}
        NVS=${PARTS[3]}
        TIME=${PARTS[4]}

        if [[ "$HEAP" == "1" && "$PSRAM" == "1" && "$NVS" == "1" ]]; then
            echo "✓ Boot validation PASSED (${TIME}ms)"
            exit 0
        else
            echo "✗ Boot validation FAILED"
            exit 1
        fi
    fi
done

echo "✗ Boot timeout"
exit 1
```

---

## M4: RF Validation

### Coexistence Test

```cpp
// test/rf_validation.cpp

struct RfTestResult {
    bool wifiInitOk;
    bool bleInitOk;
    bool coexOk;
    int wifiScanCount;
    int bleAdvCount;
    uint32_t testDurationMs;
    int wifiErrors;
    int bleErrors;
};

RfTestResult runCoexTest(uint32_t durationMs = 30000) {
    RfTestResult result = {};
    uint32_t start = esp_log_timestamp();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    result.wifiInitOk = esp_wifi_init(&cfg) == ESP_OK &&
                        esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK &&
                        esp_wifi_start() == ESP_OK;

    // Initialize BLE
    result.bleInitOk = nimble_port_init() == ESP_OK;
    if (result.bleInitOk) {
        ble_hs_cfg.sync_cb = bleSyncCallback;
        nimble_port_freertos_init(bleHostTask);
    }

    // Coexistence check
    result.coexOk = result.wifiInitOk && result.bleInitOk;

    // Stress test: alternate operations
    while (esp_log_timestamp() - start < durationMs) {
        // WiFi scan
        if (esp_wifi_scan_start(nullptr, true) == ESP_OK) {
            result.wifiScanCount++;
        } else {
            result.wifiErrors++;
        }

        // BLE advertise toggle
        if (toggleBleAdvertising()) {
            result.bleAdvCount++;
        } else {
            result.bleErrors++;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    result.testDurationMs = esp_log_timestamp() - start;
    return result;
}

void cmdCoexTest(int argc, char** argv) {
    uint32_t duration = (argc > 1) ? atoi(argv[1]) : 30000;

    ESP_LOGI(kTag, "Running coex test for %lu ms...", duration);
    auto result = runCoexTest(duration);

    ESP_LOGI(kTag, "=== RF Coexistence Test ===");
    ESP_LOGI(kTag, "WiFi init: %s", result.wifiInitOk ? "OK" : "FAIL");
    ESP_LOGI(kTag, "BLE init: %s", result.bleInitOk ? "OK" : "FAIL");
    ESP_LOGI(kTag, "Coexistence: %s", result.coexOk ? "OK" : "FAIL");
    ESP_LOGI(kTag, "WiFi scans: %d (errors: %d)", result.wifiScanCount, result.wifiErrors);
    ESP_LOGI(kTag, "BLE toggles: %d (errors: %d)", result.bleAdvCount, result.bleErrors);

    bool pass = result.coexOk && result.wifiErrors == 0 && result.bleErrors == 0;
    ESP_LOGI(kTag, "Result: %s", pass ? "PASS" : "FAIL");

    printf("RF_RESULT:%d:%d:%d:%d\n",
           result.coexOk, result.wifiScanCount, result.wifiErrors, result.bleErrors);
}
```

---

## M6: ESP-NOW Latency Validation

### Latency Test Implementation

```cpp
// test/latency_test.cpp

struct LatencyStats {
    uint32_t count;
    uint32_t minUs;
    uint32_t maxUs;
    uint64_t sumUs;
    uint32_t p50Us;
    uint32_t p95Us;
    uint32_t p99Us;
    uint32_t lostPackets;
};

static uint32_t latencies[1000];
static volatile bool pongReceived;
static volatile uint32_t pongTimestamp;

void espnowPongCallback(const uint8_t* mac, const uint8_t* data, int len) {
    pongTimestamp = esp_timer_get_time();
    pongReceived = true;
}

LatencyStats runLatencyTest(const uint8_t* peerMac, uint32_t count) {
    LatencyStats stats = {.minUs = UINT32_MAX};

    // Register peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, 6);
    esp_now_add_peer(&peer);

    // Ping-pong test
    uint8_t pingMsg[8] = {'P', 'I', 'N', 'G', 0, 0, 0, 0};

    for (uint32_t i = 0; i < count && i < 1000; i++) {
        pongReceived = false;

        // Send ping with sequence number
        pingMsg[4] = (i >> 0) & 0xFF;
        pingMsg[5] = (i >> 8) & 0xFF;

        uint32_t sendTime = esp_timer_get_time();
        if (esp_now_send(peerMac, pingMsg, sizeof(pingMsg)) != ESP_OK) {
            stats.lostPackets++;
            continue;
        }

        // Wait for pong (max 50ms)
        int waitMs = 0;
        while (!pongReceived && waitMs < 50) {
            vTaskDelay(1);
            waitMs++;
        }

        if (!pongReceived) {
            stats.lostPackets++;
            continue;
        }

        uint32_t rtt = pongTimestamp - sendTime;
        latencies[stats.count++] = rtt;

        stats.sumUs += rtt;
        if (rtt < stats.minUs) stats.minUs = rtt;
        if (rtt > stats.maxUs) stats.maxUs = rtt;
    }

    // Calculate percentiles
    if (stats.count > 0) {
        // Sort latencies
        std::sort(latencies, latencies + stats.count);

        stats.p50Us = latencies[stats.count * 50 / 100];
        stats.p95Us = latencies[stats.count * 95 / 100];
        stats.p99Us = latencies[stats.count * 99 / 100];
    }

    return stats;
}

void cmdLatencyTest(int argc, char** argv) {
    if (argc < 2) {
        ESP_LOGE(kTag, "Usage: latency_test <peer_mac> [count]");
        return;
    }

    uint8_t peerMac[6];
    if (!parseMac(argv[1], peerMac)) {
        ESP_LOGE(kTag, "Invalid MAC address");
        return;
    }

    uint32_t count = (argc > 2) ? atoi(argv[2]) : 100;

    ESP_LOGI(kTag, "Running latency test with %lu iterations...", count);
    auto stats = runLatencyTest(peerMac, count);

    ESP_LOGI(kTag, "=== ESP-NOW Latency Test ===");
    ESP_LOGI(kTag, "Packets: %lu sent, %lu received, %lu lost",
             count, stats.count, stats.lostPackets);
    ESP_LOGI(kTag, "RTT min/avg/max: %lu/%llu/%lu µs",
             stats.minUs, stats.sumUs / stats.count, stats.maxUs);
    ESP_LOGI(kTag, "RTT P50/P95/P99: %lu/%lu/%lu µs",
             stats.p50Us, stats.p95Us, stats.p99Us);

    // One-way is RTT/2
    uint32_t p95OneWay = stats.p95Us / 2;
    bool pass = p95OneWay < 2000;  // < 2ms one-way P95

    ESP_LOGI(kTag, "One-way P95: %lu µs (target < 2000 µs)", p95OneWay);
    ESP_LOGI(kTag, "Result: %s", pass ? "PASS" : "FAIL");

    printf("LATENCY_RESULT:%lu:%lu:%lu:%lu:%d\n",
           stats.p50Us / 2, stats.p95Us / 2, stats.p99Us / 2,
           stats.lostPackets, pass);
}
```

### Latency Validation Script

```bash
#!/bin/bash
# scripts/validate_latency.sh
# Requires 2 DevKits connected

PORT1=${1:-/dev/ttyUSB0}
PORT2=${2:-/dev/ttyUSB1}
COUNT=${3:-1000}

# Get MAC of second device
echo "Getting peer MAC..."
MAC2=$(timeout 5 cat $PORT2 | grep -m1 "MAC:" | cut -d: -f2-)

# Run latency test on first device
echo "Running latency test ($COUNT iterations)..."
echo "latency_test $MAC2 $COUNT" > $PORT1

# Parse result
timeout 120 cat $PORT1 | while read line; do
    if [[ "$line" == *"LATENCY_RESULT:"* ]]; then
        IFS=':' read -ra PARTS <<< "$line"
        P95=${PARTS[2]}
        LOST=${PARTS[4]}
        PASS=${PARTS[5]}

        echo "P95 one-way latency: ${P95}µs"
        echo "Lost packets: $LOST"

        if [[ "$PASS" == "1" ]]; then
            echo "✓ Latency validation PASSED"
            exit 0
        else
            echo "✗ Latency validation FAILED (P95 > 2ms)"
            exit 1
        fi
    fi
done

echo "✗ Test timeout"
exit 1
```

---

## M7: Complete Smoke Test Suite

### Hardware Smoke Test

```cpp
// test/smoke_test_suite.cpp

struct SmokeTestResult {
    const char* name;
    bool passed;
    const char* message;
};

static etl::vector<SmokeTestResult, 16> smokeResults;

bool testLedRing() {
    // Test each color
    const Color colors[] = {Color::red(), Color::green(), Color::blue(), Color::white()};

    for (auto& c : colors) {
        if (led_.setAll(c) != ESP_OK) return false;
        if (led_.refresh() != ESP_OK) return false;
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    // Test individual LED addressing
    led_.setAll(Color::off());
    for (int i = 0; i < ILedDriver::kLedCount; i++) {
        led_.setLed(i, Color::white());
        led_.refresh();
        vTaskDelay(pdMS_TO_TICKS(50));
        led_.setLed(i, Color::off());
    }

    return true;
}

bool testAudioOutput() {
    // Test multiple tones
    const uint16_t freqs[] = {440, 880, 1320};

    for (auto f : freqs) {
        if (audio_.playTone(f, 100) != ESP_OK) return false;
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    return true;
}

bool testHapticMotor() {
    const uint8_t effects[] = {
        IHapticDriver::kEffectStrongClick,
        IHapticDriver::kEffectDoubleClick,
        IHapticDriver::kEffectBuzz
    };

    for (auto e : effects) {
        if (haptic_.playEffect(e) != ESP_OK) return false;
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    return true;
}

bool testCapacitiveTouch() {
    ESP_LOGI(kTag, "Touch the pod within 5 seconds...");
    led_.setAll(Color::yellow());
    led_.refresh();

    TouchEvent event;
    esp_err_t err = touch_.waitForTouch(&event, 5000);

    if (err == ESP_OK) {
        ESP_LOGI(kTag, "Touch detected: strength=%d, source=%d",
                 event.strength, static_cast<int>(event.source));
        led_.setAll(Color::green());
        led_.refresh();
        return true;
    }

    led_.setAll(Color::red());
    led_.refresh();
    return false;
}

bool testImuSensor() {
    // Verify device ID
    if (imu_.getDeviceId() != IImuDriver::kExpectedDeviceId) {
        return false;
    }

    // Read acceleration
    Acceleration accel;
    if (imu_.readAcceleration(accel) != ESP_OK) {
        return false;
    }

    // Should read ~1g on one axis when stationary
    int16_t mag = accel.magnitude();
    ESP_LOGI(kTag, "Accel: x=%d, y=%d, z=%d, mag=%d mg",
             accel.x, accel.y, accel.z, mag);

    // Expect 800-1200 mg when stationary
    return mag > 800 && mag < 1200;
}

bool testTapDetection() {
    ESP_LOGI(kTag, "Tap the pod within 5 seconds...");
    led_.setAll(Color::cyan());
    led_.refresh();

    imu_.enableTapDetection();
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t start = esp_log_timestamp();
    while (esp_log_timestamp() - start < 5000) {
        if (imu_.wasTapDetected()) {
            ESP_LOGI(kTag, "Tap detected by IMU");
            led_.setAll(Color::green());
            led_.refresh();
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    led_.setAll(Color::red());
    led_.refresh();
    return false;
}

bool testBattery() {
    uint16_t mv = power_.getBatteryVoltageMv();
    uint8_t pct = power_.getBatteryPercent();

    ESP_LOGI(kTag, "Battery: %d mV (%d%%)", mv, pct);

    // Voltage should be in valid LiPo range
    return mv >= 3000 && mv <= 4250;
}

bool testI2cBus() {
    auto& i2c = I2cManager::instance();

    // Probe expected devices
    bool imuFound = i2c.probe(IImuDriver::kI2cAddress);
    bool hapticFound = i2c.probe(0x5A);  // DRV2605L

    ESP_LOGI(kTag, "I2C devices: IMU=%s, Haptic=%s",
             imuFound ? "found" : "missing",
             hapticFound ? "found" : "missing");

    return imuFound && hapticFound;
}

void runSmokeTestSuite() {
    smokeResults.clear();

    struct TestDef {
        const char* name;
        bool (*func)();
    };

    TestDef tests[] = {
        {"I2C Bus", testI2cBus},
        {"LED Ring", testLedRing},
        {"Audio Output", testAudioOutput},
        {"Haptic Motor", testHapticMotor},
        {"IMU Sensor", testImuSensor},
        {"Battery", testBattery},
        {"Capacitive Touch", testCapacitiveTouch},
        {"Tap Detection", testTapDetection},
    };

    int passed = 0, failed = 0;

    for (const auto& test : tests) {
        ESP_LOGI(kTag, "\n=== Testing: %s ===", test.name);

        bool result = false;
        try {
            result = test.func();
        } catch (...) {
            result = false;
        }

        smokeResults.push_back({test.name, result, nullptr});

        if (result) {
            ESP_LOGI(kTag, "✓ %s: PASS", test.name);
            passed++;
        } else {
            ESP_LOGE(kTag, "✗ %s: FAIL", test.name);
            failed++;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(kTag, "\n=== Smoke Test Summary ===");
    ESP_LOGI(kTag, "Passed: %d, Failed: %d", passed, failed);

    // Visual indication
    if (failed == 0) {
        led_.setAll(Color::green());
    } else {
        led_.setAll(Color::red());
    }
    led_.refresh();

    // Machine-readable output
    printf("SMOKE_RESULT:%d:%d\n", passed, failed);
}
```

---

## M8: Multi-Pod Test Harness

### Network Coordination Test

```cpp
// test/multi_pod_test.cpp

struct MultiPodTestResult {
    uint8_t podCount;
    uint8_t respondingPods;
    bool masterElected;
    uint32_t electionTimeMs;
    uint32_t syncAccuracyUs;
    uint32_t flashSyncErrorUs;
};

MultiPodTestResult runMultiPodTest() {
    MultiPodTestResult result = {};

    // Wait for network formation
    uint32_t start = esp_log_timestamp();

    // Count pods
    result.podCount = election_.peerCount() + 1;  // Include self

    // Wait for master election (max 10s)
    while (!election_.isMaster() &&
           election_.currentState() != MasterElectionState::kFollower &&
           esp_log_timestamp() - start < 10000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    result.masterElected = election_.isMaster() ||
                           election_.currentState() == MasterElectionState::kFollower;
    result.electionTimeMs = esp_log_timestamp() - start;

    // Count responding pods (master only)
    if (election_.isMaster()) {
        result.respondingPods = countRespondingPods();
    }

    // Measure sync accuracy
    auto stats = timing_.getStats();
    result.syncAccuracyUs = stats.jitterUs;

    return result;
}

void cmdSyncFlash(int argc, char** argv) {
    if (!election_.isMaster()) {
        ESP_LOGE(kTag, "Must be master to run sync flash test");
        return;
    }

    ESP_LOGI(kTag, "Starting synchronized flash test...");

    // Schedule flash 2 seconds from now
    uint64_t flashTime = timing_.getSyncedTimeUs() + 2000000;

    // Broadcast flash command to all pods
    SyncFlashMsg msg;
    msg.flashTimeUs = flashTime;
    msg.color = Color::white();
    comm_.broadcast(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    // Execute locally too
    while (timing_.getSyncedTimeUs() < flashTime) {
        // Spin wait for precision
    }

    led_.setAll(Color::white());
    led_.refresh();

    vTaskDelay(pdMS_TO_TICKS(200));
    led_.setAll(Color::off());
    led_.refresh();

    ESP_LOGI(kTag, "Flash executed - verify all pods flashed simultaneously");
    ESP_LOGI(kTag, "Use oscilloscope on LED data lines to measure sync error");
}

void cmdTouchRelay(int argc, char** argv) {
    if (!election_.isMaster()) {
        ESP_LOGE(kTag, "Must be master to run touch relay test");
        return;
    }

    ESP_LOGI(kTag, "Touch relay test: touch any pod to trigger chain reaction");

    // Arm all pods
    ArmAllMsg arm;
    arm.timeoutMs = 10000;
    comm_.broadcast(reinterpret_cast<uint8_t*>(&arm), sizeof(arm));

    // Wait for any touch
    uint64_t startUs = timing_.getSyncedTimeUs();

    while (timing_.getSyncedTimeUs() - startUs < 10000000) {
        // Check for touch events from any pod
        TouchEventMsg* event = comm_.getNextTouchEvent();
        if (event) {
            ESP_LOGI(kTag, "Pod %d touched at %llu µs (reaction: %lu µs)",
                     event->podId,
                     event->timestampUs,
                     event->timestampUs - startUs);

            // Light up next pod in sequence
            uint8_t nextPod = (event->podId % election_.peerCount()) + 1;
            LightPodMsg light;
            light.podId = nextPod;
            light.color = Color::green();
            light.durationMs = 500;
            comm_.sendToPod(nextPod, reinterpret_cast<uint8_t*>(&light), sizeof(light));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Multi-Pod Test Script

```bash
#!/bin/bash
# scripts/test_multi_pod.sh
# Requires 6 pods connected to USB hub

PORTS=(/dev/ttyUSB*)
POD_COUNT=${#PORTS[@]}

echo "Found $POD_COUNT pods"

if [ $POD_COUNT -lt 6 ]; then
    echo "Need at least 6 pods for full test"
    exit 1
fi

# Flash all pods
for port in "${PORTS[@]}"; do
    echo "Flashing $port..."
    idf.py -p $port flash &
done
wait

echo "Waiting for network formation..."
sleep 15

# Run tests on master (first pod)
MASTER=${PORTS[0]}

echo "Running multi-pod tests..."
echo "network_info" > $MASTER
sleep 2

echo "sync_flash" > $MASTER
sleep 3

# Collect results
echo "=== Results ==="
cat $MASTER | grep -E "(POD_COUNT|SYNC_ERROR|PASS|FAIL)"
```

---

## BLE Test Characteristics

### Test Service UUID

```cpp
// BLE Test Service: 0x1900
// Characteristics:
//   0x1901 - Trigger smoke test (write)
//   0x1902 - Smoke test result (notify)
//   0x1903 - Run specific test (write)
//   0x1904 - Get diagnostics (read)
//   0x1905 - Force state (write)

void registerBleTestCharacteristics() {
    ble_uuid16_t testSvcUuid = BLE_UUID16_INIT(0x1900);

    static const struct ble_gatt_chr_def testChrs[] = {
        {
            .uuid = BLE_UUID16_DECLARE(0x1901),
            .access_cb = onTriggerSmokeTest,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = BLE_UUID16_DECLARE(0x1902),
            .access_cb = onSmokeTestResult,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        },
        {
            .uuid = BLE_UUID16_DECLARE(0x1903),
            .access_cb = onRunSpecificTest,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
            .uuid = BLE_UUID16_DECLARE(0x1904),
            .access_cb = onGetDiagnostics,
            .flags = BLE_GATT_CHR_F_READ,
        },
        {
            .uuid = BLE_UUID16_DECLARE(0x1905),
            .access_cb = onForceState,
            .flags = BLE_GATT_CHR_F_WRITE,
        },
        {0},
    };

    // Register service...
}
```

---

## Milestone Validation Summary

### CI Pipeline Integration

```yaml
# .github/workflows/validation.yml

jobs:
  m1-compiles:
    runs-on: ubuntu-latest
    steps:
      - uses: espressif/esp-idf-ci-action@v1
      - run: cd firmware && idf.py build

  m5-unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: espressif/esp-idf-ci-action@v1
      - run: |
          cd firmware
          idf.py --preview set-target linux
          idf.py build
          ./build/domes.elf

  nightly-hardware:
    runs-on: [self-hosted, devkit]
    schedule:
      - cron: '0 3 * * *'
    steps:
      - run: ./scripts/validate_boot.sh
      - run: ./scripts/validate_rf.sh
      - run: ./scripts/validate_latency.sh
```

### Test Hook Checklist by Milestone

| Milestone | Serial Cmd | BLE Char | Script | CI Job |
|-----------|------------|----------|--------|--------|
| M1 | - | - | - | ✅ m1-compiles |
| M2 | boot_info | - | validate_boot.sh | ✅ nightly |
| M3 | crash_test, stack_check | - | - | Manual |
| M4 | wifi_scan, coex_test | - | validate_rf.sh | ✅ nightly |
| M5 | run_tests | - | - | ✅ m5-unit-tests |
| M6 | latency_test, sync_stats | - | validate_latency.sh | ✅ nightly |
| M7 | test_all, test_* | 0x1901 | smoke_test.sh | ⚠️ Manual |
| M8 | sync_flash, touch_relay | - | test_multi_pod.sh | ⚠️ Manual |
| M9 | - | Full app | - | Manual |

---

*Prerequisites: 06-testing.md, 12-system-policies.md*
*Related: All architecture docs*
