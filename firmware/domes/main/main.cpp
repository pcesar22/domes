/**
 * @file main.cpp
 * @brief DOMES Firmware entry point
 */

#include "config.hpp"
#include "sdkconfig.h"

#include "config/featureManager.hpp"
#include "drivers/ledStrip.hpp"
#include "infra/logging.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/taskManager.hpp"
#include "infra/watchdog.hpp"
#include "services/githubClient.hpp"
#include "services/ledService.hpp"
#include "services/otaManager.hpp"
#include "trace/traceApi.hpp"
#include "trace/traceRecorder.hpp"
#include "transport/bleOtaService.hpp"
#include "transport/serialOtaReceiver.hpp"
#include "transport/tcpConfigServer.hpp"
#include "transport/usbCdcTransport.hpp"

// WiFi manager and secrets are only needed when WiFi auto-connect is enabled
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
#include "secrets.hpp"
#include "services/wifiManager.hpp"
#endif

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// =============================================================================
// Global Instances - Infrastructure
// =============================================================================
static domes::infra::TaskManager taskManager;
static domes::infra::NvsConfig configStorage;
static domes::infra::NvsConfig statsStorage;

// =============================================================================
// Global Instances - Hardware Drivers
// =============================================================================
static domes::LedStripDriver<pins::kLedCount>* ledDriver = nullptr;

// =============================================================================
// Global Instances - Services
// =============================================================================
static domes::GithubClient* githubClient = nullptr;
static domes::OtaManager* otaManager = nullptr;
static domes::config::FeatureManager* featureManager = nullptr;
static domes::LedService* ledService = nullptr;

// =============================================================================
// Global Instances - Transport Layer
// =============================================================================
static domes::UsbCdcTransport* usbTransport = nullptr;
static domes::BleOtaService* bleTransport = nullptr;

// =============================================================================
// Global Instances - Protocol Handlers
// =============================================================================
static domes::SerialOtaReceiver* usbProtocolHandler = nullptr;
static domes::SerialOtaReceiver* bleProtocolHandler = nullptr;

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
static domes::TcpConfigServer* tcpConfigServer = nullptr;
static domes::WifiManager* wifiManager = nullptr;
static domes::infra::NvsConfig wifiStorage;
#endif

// =============================================================================
// LED Status Helper
// =============================================================================

/**
 * @brief Show a status color on the LED for a fixed duration
 *
 * @param color The color to display
 * @param clearAfter If true, clears the LED after the duration
 */
static void showLedStatus(const domes::Color& color, bool clearAfter = true) {
    if (!ledDriver) {
        return;
    }

    ledDriver->setPixel(0, color);
    ledDriver->refresh();
    vTaskDelay(pdMS_TO_TICKS(init_timing::kStatusIndicatorMs));

    if (clearAfter) {
        ledDriver->clear();
        ledDriver->refresh();
    }
}

// =============================================================================
// Self-Test and OTA Verification
// =============================================================================

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

    // Test 3: Heap is reasonable
    size_t freeHeap = esp_get_free_heap_size();
    if (freeHeap < init_timing::kMinHeapForSelfTest) {
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
        esp_err_t err = otaManager->confirmFirmware();
        if (err == ESP_OK) {
            ESP_LOGI(kTag, "OTA firmware confirmed successfully");
            showLedStatus(domes::Color::green());
        } else {
            ESP_LOGE(kTag, "Failed to confirm firmware: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(kTag, "Self-test FAILED - rolling back to previous firmware");
        showLedStatus(domes::Color::red(), false);  // Don't clear - we're rebooting
        otaManager->rollback();                     // Never returns
    }
}

// =============================================================================
// OTA Auto-Check Task
// =============================================================================

#if defined(CONFIG_DOMES_OTA_AUTO_CHECK) && defined(CONFIG_DOMES_WIFI_AUTO_CONNECT)
/**
 * @brief Task function for automatic OTA update checking
 *
 * Checks GitHub for firmware updates and initiates download if available.
 * Deletes itself after completion.
 */
static void otaAutoCheckTask(void* param) {
    (void)param;

    ESP_LOGI(kTag, "OTA check task started");
    ESP_LOGI(kTag, "Checking for firmware updates...");

    domes::OtaCheckResult updateResult;
    esp_err_t err = otaManager->checkForUpdate(updateResult);

    if (err == ESP_OK) {
        if (updateResult.updateAvailable) {
            ESP_LOGI(kTag, "Update available: v%d.%d.%d -> v%d.%d.%d",
                     updateResult.currentVersion.major, updateResult.currentVersion.minor,
                     updateResult.currentVersion.patch, updateResult.availableVersion.major,
                     updateResult.availableVersion.minor, updateResult.availableVersion.patch);
            ESP_LOGI(kTag, "Download URL: %s", updateResult.downloadUrl);
            ESP_LOGI(kTag, "Firmware size: %zu bytes", updateResult.firmwareSize);

            ESP_LOGI(kTag, "Starting OTA update...");
            err = otaManager->startUpdate(updateResult.downloadUrl,
                                          updateResult.sha256[0] ? updateResult.sha256 : nullptr);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "OTA update failed to start: %s", esp_err_to_name(err));
            }
            // If successful, device will reboot
        } else {
            ESP_LOGI(kTag, "Firmware is up to date (v%d.%d.%d)",
                     updateResult.currentVersion.major, updateResult.currentVersion.minor,
                     updateResult.currentVersion.patch);
        }
    } else {
        ESP_LOGW(kTag, "Update check failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(kTag, "OTA check task done, deleting self");
    vTaskDelete(nullptr);
}

/**
 * @brief Start OTA auto-check task if conditions are met
 */
static void startOtaAutoCheck() {
    if (!wifiManager || !wifiManager->isConnected() || !otaManager) {
        return;
    }

    ESP_LOGI(kTag, "Creating OTA check task...");
    xTaskCreate(otaAutoCheckTask, "ota_check", 16384, nullptr, domes::infra::priority::kLow,
                nullptr);
}
#endif  // CONFIG_DOMES_OTA_AUTO_CHECK && CONFIG_DOMES_WIFI_AUTO_CONNECT

// =============================================================================
// Initialization Functions - Infrastructure
// =============================================================================

static esp_err_t initInfrastructure() {
    esp_err_t err = domes::infra::NvsConfig::initFlash();
    if (err != ESP_OK)
        return err;

    configStorage.open(domes::infra::nvs_ns::kConfig);

    err = domes::infra::Watchdog::init(timing::kWatchdogTimeoutS, true);
    if (err != ESP_OK)
        return err;

    if (statsStorage.open(domes::infra::nvs_ns::kStats) == ESP_OK) {
        uint32_t bootCount =
            statsStorage.getOrDefault<uint32_t>(domes::infra::stats_key::kBootCount, 0) + 1;
        statsStorage.setU32(domes::infra::stats_key::kBootCount, bootCount);
        statsStorage.commit();
        ESP_LOGI(kTag, "Boot #%lu", static_cast<unsigned long>(bootCount));
    }

    return ESP_OK;
}

// =============================================================================
// Initialization Functions - Hardware Drivers
// =============================================================================

static esp_err_t initLedStrip() {
    static domes::LedStripDriver<pins::kLedCount> driver(pins::kLedData, false);
    ledDriver = &driver;

    esp_err_t err = ledDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "LED init failed: %s", esp_err_to_name(err));
        return err;
    }

    ledDriver->setBrightness(led::kDefaultBrightness);
    return ESP_OK;
}

// =============================================================================
// Initialization Functions - Network Stack
// =============================================================================

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
/**
 * @brief Initialize WiFi and connect to AP
 */
static esp_err_t initWifiWithApConnection() {
    ESP_LOGI(kTag, "Initializing WiFi...");

    esp_err_t err = wifiStorage.open(domes::wifi_nvs::kNamespace);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(kTag, "WiFi NVS open warning: %s", esp_err_to_name(err));
    }

    static domes::WifiManager wifi(wifiStorage);
    wifiManager = &wifi;

    err = wifiManager->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "Connecting to WiFi: %s", domes::secrets::kWifiSsid);
    err = wifiManager->connect(domes::secrets::kWifiSsid, domes::secrets::kWifiPassword, true);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi connect failed: %s", esp_err_to_name(err));
        return err;
    }

    // Wait for connection
    for (uint32_t i = 0; i < init_timing::kWifiConnectTimeoutS && !wifiManager->isConnected();
         i++) {
        ESP_LOGI(kTag, "Waiting for WiFi... %lu/%lu", static_cast<unsigned long>(i + 1),
                 static_cast<unsigned long>(init_timing::kWifiConnectTimeoutS));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (wifiManager->isConnected()) {
        char ip[16];
        wifiManager->getIpAddress(ip, sizeof(ip));
        ESP_LOGI(kTag, "WiFi connected! IP: %s, RSSI: %d dBm", ip, wifiManager->getRssi());
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

    size_t heapBefore = esp_get_free_heap_size();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_netif_t* staNetif = esp_netif_create_default_wifi_sta();
    if (staNetif == nullptr) {
        ESP_LOGE(kTag, "Failed to create WiFi STA netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t heapAfter = esp_get_free_heap_size();
    ESP_LOGI(kTag, "WiFi stack initialized (STA mode, not connected)");
    ESP_LOGI(kTag, "WiFi heap usage: %zu bytes", heapBefore - heapAfter);

    return ESP_OK;
}

/**
 * @brief Initialize WiFi subsystem based on config
 *
 * With CONFIG_DOMES_WIFI_AUTO_CONNECT: Connects to AP from secrets.hpp
 * Without: Just initializes WiFi stack for ESP-NOW
 */
static esp_err_t initWifiSubsystem() {
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    return initWifiWithApConnection();
#else
    return initWifiForEspNow();
#endif
}

// =============================================================================
// Initialization Functions - OTA Subsystem
// =============================================================================

static esp_err_t initOta() {
    ESP_LOGI(kTag, "Initializing OTA subsystem");

    static domes::GithubClient github(kGithubOwner, kGithubRepo);
    githubClient = &github;

    static domes::OtaManager ota(github);
    otaManager = &ota;

    esp_err_t err = otaManager->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OTA init failed: %s", esp_err_to_name(err));
        return err;
    }

    domes::FirmwareVersion ver = otaManager->getCurrentVersion();
    ESP_LOGI(kTag, "Firmware version: %d.%d.%d (partition: %s)", ver.major, ver.minor, ver.patch,
             otaManager->getCurrentPartition());

    return ESP_OK;
}

// =============================================================================
// Initialization Functions - Application Services
// =============================================================================

/**
 * @brief Initialize feature manager for runtime config
 *
 * Must be called before TCP config server and serial OTA receiver,
 * as both use the feature manager.
 */
static void initFeatureManager() {
    static domes::config::FeatureManager features;
    featureManager = &features;
    ESP_LOGI(kTag, "Feature manager initialized");
}

/**
 * @brief Initialize LED service for pattern control
 *
 * Requires ledDriver and featureManager to be initialized first.
 */
static esp_err_t initLedService() {
    if (!ledDriver || !featureManager) {
        ESP_LOGE(kTag, "Cannot init LED service: dependencies not ready");
        return ESP_FAIL;
    }

    static domes::LedService service(*ledDriver, *featureManager);
    ledService = &service;

    esp_err_t err = ledService->start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "LED service start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "LED service started");
    return ESP_OK;
}

// =============================================================================
// Initialization Functions - Protocol Handlers
// =============================================================================

/**
 * @brief Initialize BLE OTA service
 *
 * Sets up BLE GATT server and starts the BLE protocol handler task.
 * This allows OTA updates and config commands via Bluetooth.
 */
static esp_err_t initBleOta() {
    ESP_LOGI(kTag, "Initializing BLE OTA service...");

    // Create BLE transport (GATT server)
    static domes::BleOtaService service;
    bleTransport = &service;

    domes::TransportError err = bleTransport->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "BLE OTA service init failed: %s", domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "BLE OTA service initialized, advertising started");

    // Create protocol handler
    static domes::SerialOtaReceiver receiver(*bleTransport, featureManager);
    bleProtocolHandler = &receiver;

    // Wire up LED service BEFORE creating task (fixes race condition)
    if (ledService) {
        bleProtocolHandler->setLedService(ledService);
    }

    // Create handler task
    domes::infra::TaskConfig config = {
        .name = "ble_ota",
        .stackSize = 8192,
        .priority = domes::infra::priority::kMedium,
        .coreAffinity = domes::infra::core::kProtocol,
        .subscribeToWatchdog = false  // OTA can take a long time
    };

    esp_err_t espErr = taskManager.createTask(config, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create BLE OTA task: %s", esp_err_to_name(espErr));
        return espErr;
    }

    ESP_LOGI(kTag, "BLE OTA receiver task started");
    return ESP_OK;
}

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
/**
 * @brief Initialize TCP config server
 *
 * Starts the TCP config server for WiFi-based runtime configuration.
 * Requires featureManager to be initialized first.
 */
static esp_err_t initTcpConfigServer() {
    if (!featureManager) {
        ESP_LOGE(kTag, "Cannot init TCP config server: featureManager not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Initializing TCP config server on port %u...", domes::kConfigServerPort);

    static domes::TcpConfigServer server(*featureManager, domes::kConfigServerPort);
    tcpConfigServer = &server;

    if (ledService) {
        tcpConfigServer->setLedService(ledService);
    }

    domes::infra::TaskConfig config = {
        .name = "tcp_config",
        .stackSize = 4096,
        .priority = domes::infra::priority::kMedium,
        .coreAffinity = domes::infra::core::kProtocol,
        .subscribeToWatchdog = false  // Server may block on accept
    };

    esp_err_t err = taskManager.createTask(config, server);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create TCP config server task: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "TCP config server started on port %u", domes::kConfigServerPort);
    return ESP_OK;
}
#endif  // CONFIG_DOMES_WIFI_AUTO_CONNECT

/**
 * @brief Initialize serial OTA receiver
 *
 * Sets up USB-CDC transport and starts the serial protocol handler task.
 * This allows OTA updates and config commands via USB serial.
 *
 * @note This takes over the USB-CDC console - logs will stop appearing after this.
 */
static esp_err_t initSerialOta() {
    ESP_LOGI(kTag, "Initializing serial OTA receiver...");

    // Create USB-CDC transport
    static domes::UsbCdcTransport transport;
    usbTransport = &transport;

    domes::TransportError err = usbTransport->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "USB-CDC transport init failed: %s", domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "USB-CDC transport initialized");

    // Create protocol handler
    static domes::SerialOtaReceiver receiver(*usbTransport, featureManager);
    usbProtocolHandler = &receiver;

    if (ledService) {
        usbProtocolHandler->setLedService(ledService);
    }

    domes::infra::TaskConfig config = {
        .name = "serial_ota",
        .stackSize = 8192,
        .priority = domes::infra::priority::kMedium,
        .coreAffinity = domes::infra::core::kAny,
        .subscribeToWatchdog = false  // OTA can take a long time
    };

    esp_err_t espErr = taskManager.createTask(config, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create serial OTA task: %s", esp_err_to_name(espErr));
        return espErr;
    }

    ESP_LOGI(kTag, "Serial OTA receiver task started");
    return ESP_OK;
}

// =============================================================================
// Application Entry Point
// =============================================================================

extern "C" void app_main() {
    ESP_LOGI(kTag, "DOMES Firmware %s", DOMES_VERSION_STRING);

    // =========================================================================
    // Phase 1: Core Infrastructure (trace, NVS, watchdog)
    // =========================================================================
    esp_err_t traceErr = domes::trace::Recorder::init();
    if (traceErr == ESP_OK) {
        domes::trace::Recorder::setEnabled(true);
        domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main");
        ESP_LOGI(kTag, "Trace system initialized and enabled");
    } else {
        ESP_LOGW(kTag, "Trace init failed: %s", esp_err_to_name(traceErr));
    }

    if (initInfrastructure() != ESP_OK) {
        ESP_LOGE(kTag, "Infrastructure init failed, halting");
        return;
    }
    ESP_LOGI(kTag, "Infrastructure initialized");

    // =========================================================================
    // Phase 2: Hardware Drivers (LED)
    // =========================================================================
    if (initLedStrip() != ESP_OK) {
        ESP_LOGW(kTag, "LED init failed, continuing without LED");
    }

    // =========================================================================
    // Phase 3: Network Stack (WiFi subsystem)
    // =========================================================================
    esp_err_t wifiErr = initWifiSubsystem();
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (wifiErr != ESP_OK) {
        ESP_LOGW(kTag, "WiFi connect failed, GitHub OTA unavailable");
    }
#else
    if (wifiErr != ESP_OK) {
        ESP_LOGE(kTag, "WiFi stack init failed - ESP-NOW will not work!");
    }
#endif

    // =========================================================================
    // Phase 4: OTA Subsystem (manager, verification)
    // =========================================================================
    if (initOta() != ESP_OK) {
        ESP_LOGW(kTag, "OTA init failed, continuing without OTA support");
    } else {
        handleOtaVerification();
    }

    // =========================================================================
    // Phase 5: Application Services (feature manager, LED service)
    // =========================================================================
    initFeatureManager();

    if (initLedService() != ESP_OK) {
        ESP_LOGW(kTag, "LED service init failed, continuing without LED patterns");
    }

    // =========================================================================
    // Phase 6: Protocol Handlers (BLE -> TCP -> Serial, order matters!)
    // Note: Serial OTA is last because it takes over the console
    // =========================================================================
    ESP_LOGI(kTag, "Initializing BLE stack...");
    vTaskDelay(pdMS_TO_TICKS(init_timing::kLogFlushDelayMs));

    size_t heapBeforeBle = esp_get_free_heap_size();
    if (initBleOta() != ESP_OK) {
        ESP_LOGW(kTag, "BLE OTA init failed, continuing without BLE OTA");
    } else {
        size_t heapAfterBle = esp_get_free_heap_size();
        ESP_LOGI(kTag, "BLE stack initialized (NimBLE + advertising)");
        ESP_LOGI(kTag, "BLE heap usage: %zu bytes", heapBeforeBle - heapAfterBle);
    }

    vTaskDelay(pdMS_TO_TICKS(init_timing::kLogFlushDelayMs));
    vTaskDelay(pdMS_TO_TICKS(init_timing::kBleSettleDelayMs));

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (wifiManager && wifiManager->isConnected()) {
        ESP_LOGI(kTag, "WiFi connected, starting TCP config server...");
        if (initTcpConfigServer() != ESP_OK) {
            ESP_LOGW(kTag, "TCP config server init failed");
        }
    } else {
        ESP_LOGI(kTag, "TCP config server not started (WiFi not connected)");
    }
#endif

    // Serial OTA - this takes over console, must be last
    if (initSerialOta() != ESP_OK) {
        ESP_LOGW(kTag, "Serial OTA init failed, continuing without serial OTA");
    }

    // =========================================================================
    // Phase 7: Boot Complete (status LED, stats log)
    // =========================================================================
    if (ledDriver) {
        ledDriver->setAll(domes::Color::green());
        ledDriver->refresh();
    }

    ESP_LOGI(kTag, "Init complete. Tasks: %zu, Heap: %lu", taskManager.getActiveTaskCount(),
             static_cast<unsigned long>(esp_get_free_heap_size()));

    // =========================================================================
    // Phase 8: Background Tasks (OTA auto-check)
    // =========================================================================
#if defined(CONFIG_DOMES_OTA_AUTO_CHECK) && defined(CONFIG_DOMES_WIFI_AUTO_CONNECT)
    startOtaAutoCheck();
#endif
}
