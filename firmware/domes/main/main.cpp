/**
 * @file main.cpp
 * @brief DOMES Firmware - M4 RF Stacks + Infrastructure
 *
 * Milestones validated on ESP32-S3-DevKitC-1 v1.1:
 * - M1-M3: Build, boot, debug infrastructure
 * - M4: RF stacks init (WiFi for ESP-NOW + BLE advertising)
 *
 * Features:
 * - NVS configuration storage
 * - Task Watchdog Timer (TWDT)
 * - Managed task lifecycle
 * - LED demo using TaskManager with smooth transitions
 * - WiFi stack init for ESP-NOW (no AP connection)
 * - BLE OTA service with NimBLE advertising
 * - OTA self-test and rollback protection
 */

#include "sdkconfig.h"  // Must be first for CONFIG_* macros
#include "config.hpp"
#include "infra/logging.hpp"
#include "infra/watchdog.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/taskManager.hpp"
#include "interfaces/iTaskRunner.hpp"
#include "drivers/ledStrip.hpp"
#include "utils/ledAnimator.hpp"
#include "services/githubClient.hpp"
#include "services/otaManager.hpp"
#include "transport/usbCdcTransport.hpp"
#include "transport/serialOtaReceiver.hpp"
#include "transport/bleOtaService.hpp"
#include "trace/traceRecorder.hpp"
#include "trace/traceApi.hpp"

// WiFi manager and secrets are only needed when WiFi auto-connect is enabled
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
#include "services/wifiManager.hpp"
#include "secrets.hpp"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include <cstdint>
#include <cstring>

// Version string from CMake
#ifndef DOMES_VERSION_STRING
#define DOMES_VERSION_STRING "v0.0.0-unknown"
#endif

static constexpr const char* kTag = domes::infra::tag::kMain;

using namespace domes::config;

// GitHub configuration for OTA updates
static constexpr const char* kGithubOwner = "pcesar22";
static constexpr const char* kGithubRepo = "domes";

// Global instances (static allocation per GUIDELINES.md)
static domes::LedStripDriver<pins::kLedCount>* ledDriver = nullptr;
static domes::LedAnimator* ledAnimator = nullptr;
static domes::infra::TaskManager taskManager;
static domes::infra::NvsConfig configStorage;
static domes::infra::NvsConfig statsStorage;
static domes::GithubClient* githubClient = nullptr;
static domes::OtaManager* otaManager = nullptr;
static domes::UsbCdcTransport* usbCdcTransport = nullptr;
static domes::SerialOtaReceiver* serialOtaReceiver = nullptr;
static domes::BleOtaService* bleOtaService = nullptr;
static domes::SerialOtaReceiver* bleOtaReceiver = nullptr;  // Reuses SerialOtaReceiver with BLE transport

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
static domes::WifiManager* wifiManager = nullptr;
static domes::infra::NvsConfig wifiStorage;
#endif

/**
 * @brief LED demo task runner with smooth transitions
 *
 * Demonstrates smooth color transitions and breathing effects.
 * Cycles through colors with 500ms transitions, switches to
 * breathing mode periodically.
 */
class LedDemoRunner : public domes::ITaskRunner {
public:
    explicit LedDemoRunner(domes::LedAnimator& animator)
        : animator_(animator), running_(true) {}

    void run() override {
        ESP_LOGI(kTag, "LED demo task started (smooth transitions)");
        domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "led_demo");

        static constexpr domes::Color kColors[] = {
            domes::Color::red(),
            domes::Color::green(),
            domes::Color::blue(),
            domes::Color::white(),
            domes::Color::yellow(),
            domes::Color::cyan(),
            domes::Color::magenta(),
            domes::Color::orange(),
        };
        static constexpr size_t kNumColors = sizeof(kColors) / sizeof(kColors[0]);
        static constexpr const char* kColorNames[] = {
            "RED", "GREEN", "BLUE", "WHITE", "YELLOW", "CYAN", "MAGENTA", "ORANGE"
        };

        static constexpr uint32_t kTransitionMs = 500;
        static constexpr uint32_t kHoldMs = 500;
        static constexpr uint32_t kBreathingDurationMs = 10000;  // 10s breathing
        static constexpr uint32_t kCycleDurationMs = 20000;      // 20s color cycle

        size_t colorIdx = 0;
        uint32_t modeStartTime = 0;
        bool breathingMode = false;

        // Start with first color
        animator_.transitionTo(kColors[0], kTransitionMs);
        ESP_LOGI(kTag, "LED: %s (transitioning)", kColorNames[0]);

        while (shouldRun()) {
            TRACE_SCOPE(TRACE_ID("LED.Update"), domes::trace::TraceCategory::kLed);
            // Update animator (this refreshes LEDs)
            animator_.update();

            // Check mode switching
            uint32_t elapsed = xTaskGetTickCount() * portTICK_PERIOD_MS - modeStartTime;

            if (breathingMode) {
                // In breathing mode
                if (elapsed >= kBreathingDurationMs) {
                    // Switch to color cycle
                    TRACE_INSTANT(TRACE_ID("LED.ModeColorCycle"), domes::trace::TraceCategory::kLed);
                    breathingMode = false;
                    modeStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    animator_.stopBreathing();
                    colorIdx = 0;
                    animator_.transitionTo(kColors[0], kTransitionMs);
                    ESP_LOGI(kTag, "Mode: Color cycle");
                }
            } else {
                // In color cycle mode
                if (elapsed >= kCycleDurationMs) {
                    // Switch to breathing
                    TRACE_INSTANT(TRACE_ID("LED.ModeBreathing"), domes::trace::TraceCategory::kLed);
                    breathingMode = true;
                    modeStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    animator_.startBreathing(domes::Color::cyan(), 2000);
                    ESP_LOGI(kTag, "Mode: Breathing (cyan)");
                } else if (!animator_.isAnimating()) {
                    // Transition complete, wait then move to next color
                    vTaskDelay(pdMS_TO_TICKS(kHoldMs));

                    colorIdx = (colorIdx + 1) % kNumColors;
                    animator_.transitionTo(kColors[colorIdx], kTransitionMs);
                    ESP_LOGI(kTag, "LED: %s", kColorNames[colorIdx]);
                }
            }

            // Reset watchdog and yield
            domes::infra::Watchdog::reset();
            vTaskDelay(pdMS_TO_TICKS(16));  // ~60fps update rate
        }

        // Clear LED on exit
        animator_.stopBreathing();
        animator_.transitionTo(domes::Color::off(), 200);
        for (int i = 0; i < 15; ++i) {
            animator_.update();
            vTaskDelay(pdMS_TO_TICKS(16));
        }
        ESP_LOGI(kTag, "LED demo task stopped");
    }

    esp_err_t requestStop() override {
        running_ = false;
        return ESP_OK;
    }

    bool shouldRun() const override {
        return running_;
    }

private:
    domes::LedAnimator& animator_;
    volatile bool running_;
};

// Static task runner instance
static LedDemoRunner* ledDemoRunner = nullptr;

/**
 * @brief Perform post-OTA self-test
 *
 * Validates critical systems after an OTA update.
 * If this fails, firmware will roll back to previous version.
 *
 * @return ESP_OK if all tests pass
 */
static esp_err_t performSelfTest() {
    ESP_LOGI(kTag, "Running post-OTA self-test...");

    // Test 1: Watchdog initialized
    if (!domes::infra::Watchdog::isInitialized()) {
        ESP_LOGE(kTag, "Self-test FAIL: Watchdog not initialized");
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "  [PASS] Watchdog initialized");

    // Test 2: NVS accessible
    domes::infra::NvsConfig testNvs;
    esp_err_t err = testNvs.open(domes::infra::nvs_ns::kConfig);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(kTag, "Self-test FAIL: NVS inaccessible");
        return ESP_FAIL;
    }
    testNvs.close();
    ESP_LOGI(kTag, "  [PASS] NVS accessible");

    // Test 3: Heap is reasonable (>50KB free)
    size_t freeHeap = esp_get_free_heap_size();
    if (freeHeap < 50000) {
        ESP_LOGE(kTag, "Self-test FAIL: Heap too low (%zu bytes)", freeHeap);
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "  [PASS] Heap OK (%zu bytes free)", freeHeap);

    // Test 4: LED driver (if initialized)
    if (ledDriver) {
        err = ledDriver->setPixel(0, domes::Color::green());
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Self-test FAIL: LED driver error");
            return ESP_FAIL;
        }
        ledDriver->refresh();
        ESP_LOGI(kTag, "  [PASS] LED driver OK");
    }

    ESP_LOGI(kTag, "Self-test PASSED");
    return ESP_OK;
}

/**
 * @brief Handle OTA verification after boot
 *
 * If running from a new OTA partition, performs self-test
 * and either confirms the firmware or rolls back.
 */
static void handleOtaVerification() {
    if (!otaManager) {
        return;
    }

    if (!otaManager->isPendingVerification()) {
        ESP_LOGI(kTag, "Firmware already verified");
        return;
    }

    ESP_LOGW(kTag, "New OTA firmware - running verification");

    esp_err_t selfTestResult = performSelfTest();

    if (selfTestResult == ESP_OK) {
        // Confirm new firmware is good
        esp_err_t err = otaManager->confirmFirmware();
        if (err == ESP_OK) {
            ESP_LOGI(kTag, "OTA firmware confirmed successfully");

            // Visual indication - green LED
            if (ledDriver) {
                ledDriver->setPixel(0, domes::Color::green());
                ledDriver->refresh();
                vTaskDelay(pdMS_TO_TICKS(2000));
                ledDriver->clear();
                ledDriver->refresh();
            }
        } else {
            ESP_LOGE(kTag, "Failed to confirm firmware: %s", esp_err_to_name(err));
        }
    } else {
        // Self-test failed - rollback
        ESP_LOGE(kTag, "Self-test FAILED - rolling back to previous firmware");

        // Visual indication - red LED
        if (ledDriver) {
            ledDriver->setPixel(0, domes::Color::red());
            ledDriver->refresh();
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        otaManager->rollback();  // Never returns
    }
}

/**
 * @brief Initialize OTA subsystem
 */
static esp_err_t initOta() {
    ESP_LOGI(kTag, "Initializing OTA subsystem");

    // Create GitHub client (static allocation during init)
    static domes::GithubClient github(kGithubOwner, kGithubRepo);
    githubClient = &github;

    // Create OTA manager
    static domes::OtaManager ota(github);
    otaManager = &ota;

    esp_err_t err = otaManager->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA init failed: %s", esp_err_to_name(err));
        return err;
    }

    domes::FirmwareVersion ver = otaManager->getCurrentVersion();
    ESP_LOGI(kTag, "Firmware version: %d.%d.%d (partition: %s)",
             ver.major, ver.minor, ver.patch,
             otaManager->getCurrentPartition());

    return ESP_OK;
}

/**
 * @brief Initialize serial OTA receiver
 *
 * Sets up USB-CDC transport and starts the serial OTA receiver task.
 * This allows OTA updates via USB serial from the host tool.
 */
static esp_err_t initSerialOta() {
    ESP_LOGI(kTag, "Initializing serial OTA receiver...");

    // Create USB-CDC transport
    static domes::UsbCdcTransport transport;
    usbCdcTransport = &transport;

    domes::TransportError err = usbCdcTransport->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "USB-CDC transport init failed: %s",
                 domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "USB-CDC transport initialized");

    // Create serial OTA receiver
    static domes::SerialOtaReceiver receiver(*usbCdcTransport);
    serialOtaReceiver = &receiver;

    // Create receiver task
    domes::infra::TaskConfig config = {
        .name = "serial_ota",
        .stackSize = 8192,  // Increased for trace dump operations
        .priority = domes::infra::priority::kMedium,
        .coreAffinity = domes::infra::core::kAny,
        .subscribeToWatchdog = false  // OTA can take a long time
    };

    esp_err_t espErr = taskManager.createTask(config, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create serial OTA task: %s",
                 esp_err_to_name(espErr));
        return espErr;
    }

    ESP_LOGI(kTag, "Serial OTA receiver task started");
    return ESP_OK;
}

/**
 * @brief Initialize BLE OTA service
 *
 * Sets up BLE GATT server and starts the BLE OTA receiver task.
 * This allows OTA updates via Bluetooth from a phone or host tool.
 */
static esp_err_t initBleOta() {
    ESP_LOGI(kTag, "Initializing BLE OTA service...");

    // Create BLE OTA service (GATT server)
    static domes::BleOtaService service;
    bleOtaService = &service;

    domes::TransportError err = bleOtaService->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "BLE OTA service init failed: %s",
                 domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "BLE OTA service initialized, advertising started");

    // Create BLE OTA receiver (reuses SerialOtaReceiver with BLE transport)
    static domes::SerialOtaReceiver receiver(*bleOtaService);
    bleOtaReceiver = &receiver;

    // Create receiver task
    domes::infra::TaskConfig config = {
        .name = "ble_ota",
        .stackSize = 4096,
        .priority = domes::infra::priority::kMedium,
        .coreAffinity = domes::infra::core::kProtocol,  // Core 0 for BLE
        .subscribeToWatchdog = false  // OTA can take a long time
    };

    esp_err_t espErr = taskManager.createTask(config, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create BLE OTA task: %s",
                 esp_err_to_name(espErr));
        return espErr;
    }

    ESP_LOGI(kTag, "BLE OTA receiver task started");
    return ESP_OK;
}

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
/**
 * @brief Initialize WiFi and connect
 */
static esp_err_t initWifi() {
    ESP_LOGI(kTag, "Initializing WiFi...");

    // Open WiFi NVS namespace
    esp_err_t err = wifiStorage.open(domes::wifi_nvs::kNamespace);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(kTag, "WiFi NVS open warning: %s", esp_err_to_name(err));
    }

    // Create WiFi manager
    static domes::WifiManager wifi(wifiStorage);
    wifiManager = &wifi;

    err = wifiManager->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Connect with credentials from secrets.hpp
    ESP_LOGI(kTag, "Connecting to WiFi: %s", domes::secrets::kWifiSsid);
    err = wifiManager->connect(domes::secrets::kWifiSsid,
                                domes::secrets::kWifiPassword,
                                true);  // Save to NVS
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi connect failed: %s", esp_err_to_name(err));
        return err;
    }

    // Wait for connection (max 30 seconds)
    for (int i = 0; i < 30 && !wifiManager->isConnected(); i++) {
        ESP_LOGI(kTag, "Waiting for WiFi... %d/30", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (wifiManager->isConnected()) {
        char ip[16];
        wifiManager->getIpAddress(ip, sizeof(ip));
        ESP_LOGI(kTag, "WiFi connected! IP: %s, RSSI: %d dBm",
                 ip, wifiManager->getRssi());
        return ESP_OK;
    } else {
        ESP_LOGE(kTag, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}
#endif  // CONFIG_DOMES_WIFI_AUTO_CONNECT

/**
 * @brief Initialize WiFi in station mode for ESP-NOW
 *
 * ESP-NOW requires the WiFi stack to be initialized in station mode.
 * We don't connect to any AP - this just enables the radio for direct
 * peer-to-peer communication.
 *
 * @note This must be called before BLE init for proper coexistence.
 */
static esp_err_t initWifiForEspNow() {
    ESP_LOGI(kTag, "Initializing WiFi stack for ESP-NOW...");

    // Track heap before WiFi init
    size_t heapBefore = esp_get_free_heap_size();

    // Initialize TCP/IP stack (required even though we don't use networking)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create default event loop (may already exist from NVS init)
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create WiFi station interface (required for ESP-NOW)
    esp_netif_t* staNetif = esp_netif_create_default_wifi_sta();
    if (staNetif == nullptr) {
        ESP_LOGE(kTag, "Failed to create WiFi STA netif");
        return ESP_FAIL;
    }

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Set station mode (required for ESP-NOW)
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    // Start WiFi (brings up the radio)
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    // Log success and heap usage
    size_t heapAfter = esp_get_free_heap_size();
    ESP_LOGI(kTag, "WiFi stack initialized (STA mode, not connected)");
    ESP_LOGI(kTag, "WiFi heap usage: %zu bytes", heapBefore - heapAfter);

    return ESP_OK;
}

/**
 * @brief Initialize the LED strip hardware and animator
 */
static esp_err_t initLedStrip() {
    ESP_LOGI(kTag, "Initializing LED on GPIO%d", static_cast<int>(pins::kLedData));

    // Create LED driver (static allocation)
    static domes::LedStripDriver<pins::kLedCount> driver(pins::kLedData, false);
    ledDriver = &driver;

    esp_err_t err = ledDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init LED driver: %s", esp_err_to_name(err));
        return err;
    }

    // Set default brightness
    ledDriver->setBrightness(led::kDefaultBrightness);

    // Create animator
    static domes::LedAnimator animator(*ledDriver);
    ledAnimator = &animator;

    ESP_LOGI(kTag, "LED strip initialized with animator");
    return ESP_OK;
}

/**
 * @brief Initialize infrastructure services
 *
 * Must be called before hardware initialization.
 */
static esp_err_t initInfrastructure() {
    esp_err_t err;

    // 1. Initialize NVS flash (must be first for config access)
    err = domes::infra::NvsConfig::initFlash();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NVS init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Open config namespace
    err = configStorage.open(domes::infra::nvs_ns::kConfig);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Config namespace not found, will use defaults");
    }

    // 3. Initialize watchdog
    err = domes::infra::Watchdog::init(timing::kWatchdogTimeoutS, true);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Watchdog init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(kTag, "Watchdog initialized (%lu second timeout)",
             static_cast<unsigned long>(timing::kWatchdogTimeoutS));

    // 4. Increment and log boot counter
    err = statsStorage.open(domes::infra::nvs_ns::kStats);
    if (err == ESP_OK) {
        uint32_t bootCount = statsStorage.getOrDefault<uint32_t>(
            domes::infra::stats_key::kBootCount, 0);
        bootCount++;
        statsStorage.setU32(domes::infra::stats_key::kBootCount, bootCount);
        statsStorage.commit();
        ESP_LOGI(kTag, "Boot count: %lu", static_cast<unsigned long>(bootCount));
    }

    return ESP_OK;
}

extern "C" void app_main() {
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "  DOMES Firmware - %s", DOMES_VERSION_STRING);
    ESP_LOGI(kTag, "  Infrastructure + OTA Layer");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "");

    // Initialize trace system early
    esp_err_t traceErr = domes::trace::Recorder::init();
    if (traceErr == ESP_OK) {
        domes::trace::Recorder::setEnabled(true);
        domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main");
        ESP_LOGI(kTag, "Trace system initialized and enabled");
    } else {
        ESP_LOGW(kTag, "Trace init failed: %s", esp_err_to_name(traceErr));
    }

    // Initialize infrastructure first
    if (initInfrastructure() != ESP_OK) {
        ESP_LOGE(kTag, "Infrastructure init failed, halting");
        return;
    }
    ESP_LOGI(kTag, "Infrastructure initialized");

    // Initialize hardware drivers
    if (initLedStrip() != ESP_OK) {
        ESP_LOGW(kTag, "LED init failed, continuing without LED");
    }

    // Initialize WiFi stack (required for ESP-NOW and BLE coexistence)
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    // WiFi auto-connect enabled - WifiManager will initialize WiFi and connect to AP
    if (initWifi() != ESP_OK) {
        ESP_LOGW(kTag, "WiFi connect failed, GitHub OTA unavailable");
    }
#else
    // No AP connection needed - just initialize WiFi stack for ESP-NOW
    if (initWifiForEspNow() != ESP_OK) {
        ESP_LOGE(kTag, "WiFi stack init failed - ESP-NOW will not work!");
    }
#endif

    // Initialize OTA subsystem
    if (initOta() != ESP_OK) {
        ESP_LOGW(kTag, "OTA init failed, continuing without OTA support");
    } else {
        // Handle OTA verification (self-test + confirm/rollback)
        handleOtaVerification();
    }

    // Initialize BLE OTA service (before serial takes over console)
    ESP_LOGI(kTag, "Initializing BLE stack...");
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay to flush logs
    size_t heapBeforeBle = esp_get_free_heap_size();
    if (initBleOta() != ESP_OK) {
        ESP_LOGW(kTag, "BLE OTA init failed, continuing without BLE OTA");
    } else {
        size_t heapAfterBle = esp_get_free_heap_size();
        ESP_LOGI(kTag, "BLE stack initialized (NimBLE + advertising)");
        ESP_LOGI(kTag, "BLE heap usage: %zu bytes", heapBeforeBle - heapAfterBle);
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay to flush logs

    // Log RF stack summary
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "=== M4: RF Stacks Init Complete ===");
    ESP_LOGI(kTag, "  WiFi: STA mode (for ESP-NOW)");
    ESP_LOGI(kTag, "  BLE:  Advertising as 'DOMES-Pod'");
    ESP_LOGI(kTag, "  Heap: %lu bytes free", static_cast<unsigned long>(esp_get_free_heap_size()));
    ESP_LOGI(kTag, "===================================");
    ESP_LOGI(kTag, "");

    // Delay before serial OTA to allow boot logs to be visible
    ESP_LOGI(kTag, "Waiting 3s before serial OTA (for log visibility)...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Initialize serial OTA receiver (USB-CDC based) - this takes over console
    if (initSerialOta() != ESP_OK) {
        ESP_LOGW(kTag, "Serial OTA init failed, continuing without serial OTA");
    }

    // Quick RGB flash to confirm hardware (using smooth transitions)
    if (ledAnimator) {
        ESP_LOGI(kTag, "Quick RGB test (smooth)...");
        ledAnimator->transitionTo(domes::Color::red(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::green(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::blue(), 150);
        for (int i = 0; i < 20; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
        ledAnimator->transitionTo(domes::Color::off(), 150);
        for (int i = 0; i < 10; ++i) { ledAnimator->update(); vTaskDelay(pdMS_TO_TICKS(16)); }
    }

    // Create LED demo task via TaskManager
    if (ledAnimator) {
        // Allocate runner (during init, heap is allowed per GUIDELINES.md)
        static LedDemoRunner runner(*ledAnimator);
        ledDemoRunner = &runner;

        domes::infra::TaskConfig ledConfig = {
            .name = "led_demo",
            .stackSize = domes::infra::stack::kStandard,  // 4096 - needs more for animations
            .priority = domes::infra::priority::kLow,
            .coreAffinity = domes::infra::core::kAny,
            .subscribeToWatchdog = true
        };

        esp_err_t err = taskManager.createTask(ledConfig, runner);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to create LED demo task");
        }
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "Initialization complete");
    ESP_LOGI(kTag, "Active tasks: %zu", taskManager.getActiveTaskCount());
    ESP_LOGI(kTag, "Free heap: %lu bytes", static_cast<unsigned long>(esp_get_free_heap_size()));
    ESP_LOGI(kTag, "");

    // Check for OTA updates in a separate task (needs large stack for TLS)
    // Disabled by default - enable with: idf.py menuconfig -> DOMES -> Enable OTA auto-check
#if defined(CONFIG_DOMES_OTA_AUTO_CHECK) && defined(CONFIG_DOMES_WIFI_AUTO_CONNECT)
    if (wifiManager && wifiManager->isConnected() && otaManager) {
        ESP_LOGI(kTag, "Creating OTA check task...");

        xTaskCreate([](void* param) {
            ESP_LOGI(kTag, "OTA check task started");
            ESP_LOGI(kTag, "Checking for firmware updates...");

            domes::OtaCheckResult updateResult;
            esp_err_t err = otaManager->checkForUpdate(updateResult);

            if (err == ESP_OK) {
                if (updateResult.updateAvailable) {
                    ESP_LOGI(kTag, "Update available: v%d.%d.%d -> v%d.%d.%d",
                             updateResult.currentVersion.major,
                             updateResult.currentVersion.minor,
                             updateResult.currentVersion.patch,
                             updateResult.availableVersion.major,
                             updateResult.availableVersion.minor,
                             updateResult.availableVersion.patch);
                    ESP_LOGI(kTag, "Download URL: %s", updateResult.downloadUrl);
                    ESP_LOGI(kTag, "Firmware size: %zu bytes", updateResult.firmwareSize);

                    // Start OTA update
                    ESP_LOGI(kTag, "Starting OTA update...");
                    err = otaManager->startUpdate(updateResult.downloadUrl,
                                                  updateResult.sha256[0] ? updateResult.sha256 : nullptr);
                    if (err != ESP_OK) {
                        ESP_LOGE(kTag, "OTA update failed to start: %s", esp_err_to_name(err));
                    }
                    // If successful, device will reboot
                } else {
                    ESP_LOGI(kTag, "Firmware is up to date (v%d.%d.%d)",
                             updateResult.currentVersion.major,
                             updateResult.currentVersion.minor,
                             updateResult.currentVersion.patch);
                }
            } else {
                ESP_LOGW(kTag, "Update check failed: %s", esp_err_to_name(err));
            }

            ESP_LOGI(kTag, "OTA check task done, deleting self");
            vTaskDelete(nullptr);
        }, "ota_check", 16384, nullptr, domes::infra::priority::kLow, nullptr);
    }
#endif  // CONFIG_DOMES_OTA_AUTO_CHECK
}
