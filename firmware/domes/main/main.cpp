/**
 * @file main.cpp
 * @brief DOMES Firmware entry point
 */

#include "config.hpp"
#include "sdkconfig.h"

#include "config/featureManager.hpp"
#include "config/modeManager.hpp"
#include "drivers/drv2605l.hpp"
#include "drivers/injectableTouchDriver.hpp"
#include "drivers/ledStrip.hpp"
#include "drivers/lis2dw12.hpp"
#include "drivers/max98357a.hpp"
#include "drivers/touchDriver.hpp"
#include "game/gameEngine.hpp"
#include "infra/appMetadata.hpp"
#include "infra/crashDumpHandler.hpp"
#include "infra/diagnostics.hpp"
#include "infra/hardwareStatus.hpp"
#include "infra/logging.hpp"
#include "infra/memoryProfiler.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/taskManager.hpp"
#include "infra/taskStartEvidence.hpp"
#include "infra/taskTopology.hpp"
#include "infra/watchdog.hpp"
#include "platform/physical/espPlatformInputs.hpp"
#include "runtime/initOrderTracker.hpp"
#include "runtime/runtimeAssembly.hpp"
#include "runtime/runtimeTraceRegistration.hpp"
#include "services/audioService.hpp"
#include "services/espNowService.hpp"
#include "services/githubClient.hpp"
#include "services/imuService.hpp"
#include "services/ledService.hpp"
#include "services/otaManager.hpp"
#include "services/touchService.hpp"
#include "trace/traceAcceptanceProbe.hpp"
#include "trace/traceApi.hpp"
#include "trace/traceRecorder.hpp"
#include "trace/traceStreamServer.hpp"
#include "transport/bleOtaService.hpp"
#include "transport/espNowTransport.hpp"
#include "transport/serialOtaReceiver.hpp"
#include "transport/tcpConfigServer.hpp"
#include "transport/uartTransport.hpp"

// WiFi manager and secrets are only needed when WiFi auto-connect is enabled
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
#include "secrets.hpp"
#include "services/wifiManager.hpp"
#endif

#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"

#include <array>
#include <cstdint>
#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kMain;

using namespace domes::config;

// GitHub configuration for OTA updates
static constexpr const char* kGithubOwner = "pcesar22";
static constexpr const char* kGithubRepo = "domes";

static bool advanceInitStage(domes::runtime::InitOrderTracker& initOrder, const char* stage) {
    if (initOrder.advance(stage)) {
        return true;
    }
    const char* expected = initOrder.expected();
    ESP_LOGE(kTag, "Init-order violation: expected=%s actual=%s",
             expected ? expected : "<complete>", stage ? stage : "<null>");
    return false;
}

// Global instances
static domes::LedStripDriver<pins::kLedCount>* ledDriver = nullptr;
static domes::infra::TaskManager taskManager;
static domes::runtime::RuntimeAssembly runtimeAssembly;
static domes::infra::NvsConfig configStorage;
static domes::GithubClient* githubClient = nullptr;
static domes::OtaManager* otaManager = nullptr;
static domes::UartTransport* uartTransport = nullptr;
static domes::SerialOtaReceiver* serialOtaReceiver = nullptr;
static domes::BleOtaService* bleOtaService = nullptr;
static domes::SerialOtaReceiver* bleOtaReceiver =
    nullptr;  // Reuses SerialOtaReceiver with BLE transport
static domes::EspNowTransport* espNowTransport = nullptr;
static domes::EspNowService* espNowService = nullptr;
static domes::config::FeatureManager* featureManager = nullptr;          // Runtime feature toggles
static domes::config::ModeManager* modeManager = nullptr;                // System mode manager
static domes::LedService* ledService = nullptr;                          // LED pattern service
static i2c_master_bus_handle_t i2cBus = nullptr;                         // I2C master bus
static domes::Lis2dw12Driver* imuDriver = nullptr;                       // LIS2DW12 IMU driver
static domes::ImuService* imuService = nullptr;                          // IMU triage service
static domes::Drv2605lDriver* hapticDriver = nullptr;                    // DRV2605L haptic driver
static domes::Max98357aDriver* audioDriver = nullptr;                    // MAX98357A audio driver
static domes::AudioService* audioService = nullptr;                      // Audio playback service
static domes::TouchDriver<pins::kTouchPadCount>* touchDriver = nullptr;  // Touch pad driver
static domes::InjectableTouchDriver* injectableTouchDriver = nullptr;  // Touch injection decorator
static domes::TouchService* touchService = nullptr;                    // Touch monitoring service
static domes::game::GameEngine* gameEngine = nullptr;                  // Game logic FSM
static uint8_t runtimePodId = 0;

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
static domes::TcpConfigServer* tcpConfigServer = nullptr;  // WiFi config server
static domes::WifiManager* wifiManager = nullptr;
static domes::infra::NvsConfig wifiStorage;
#endif

static void publishBleTouchEvent(void*, uint8_t padIndex, uint64_t timestampUs) {
    if (!bleOtaService || !bleOtaService->isConnected() || !bleOtaReceiver) {
        return;
    }
    if (!bleOtaReceiver->sendTouchEvent(runtimePodId, padIndex, timestampUs)) {
        ESP_LOGW(kTag, "Failed to publish BLE touch event for pad %u", padIndex);
    }
}

/**
 * @brief Read pod_id from NVS and log it at boot
 *
 * Pod ID determines the BLE device name suffix and identifies the pod
 * in a multi-pod setup (0 = not set, will use MAC suffix for BLE name).
 *
 * @return pod_id value (0 if not set)
 */
static uint8_t readPodId() {
    domes::infra::NvsConfig config;
    if (config.open(domes::infra::nvs_ns::kConfig) != ESP_OK) {
        ESP_LOGW(kTag, "Failed to open NVS config for pod_id");
        return 0;
    }
    uint8_t podId = config.getOrDefault<uint8_t>(domes::infra::config_key::kPodId, 0);
    config.close();

    if (podId > 0) {
        ESP_LOGI(kTag, "Pod ID: %u", podId);
    } else {
        ESP_LOGI(kTag, "Pod ID: not set (will use MAC suffix for BLE name)");
    }
    return podId;
}

#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
static bool readAutoUpdateEnabled() {
#ifdef CONFIG_DOMES_OTA_AUTO_CHECK
    constexpr uint8_t kBuildDefault = 1;
#else
    constexpr uint8_t kBuildDefault = 0;
#endif

    domes::infra::NvsConfig config;
    if (config.open(domes::infra::nvs_ns::kConfig) != ESP_OK) {
        ESP_LOGW(kTag, "Failed to read auto-update setting; using build default");
        return kBuildDefault != 0;
    }

    const bool enabled =
        config.getOrDefault<uint8_t>(domes::infra::config_key::kAutoUpdate, kBuildDefault) != 0;
    config.close();
    return enabled;
}
#endif

#ifndef DOMES_FORCE_OTA_VERIFY_FAILURE
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

    // Test 3: safety-critical internal heap remains available. External PSRAM
    // must not mask exhaustion of memory needed by task stacks and DMA.
    constexpr uint32_t kInternalHeapCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    size_t freeHeap = heap_caps_get_free_size(kInternalHeapCaps);
    if (freeHeap < 30 * 1024) {
        ESP_LOGE(kTag, "Self-test FAIL: Heap too low (%zu bytes)", freeHeap);
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "  [PASS] Heap OK (%zu bytes free)", freeHeap);

    // Test 4: every active-board peripheral completed initialization.
    constexpr std::array requiredHardware = {
        domes::infra::HardwareSubsystem::kLed,    domes::infra::HardwareSubsystem::kImu,
        domes::infra::HardwareSubsystem::kHaptic, domes::infra::HardwareSubsystem::kAudio,
        domes::infra::HardwareSubsystem::kTouch,
    };
    for (auto subsystem : requiredHardware) {
        if (!domes::infra::HardwareStatus::isReady(subsystem)) {
            ESP_LOGE(kTag, "Self-test FAIL: required hardware was not initialized");
            return ESP_FAIL;
        }
    }
    ESP_LOGI(kTag, "  [PASS] Required hardware initialized");

    // Test 5: LED output path accepts and transmits a frame.
    if (!ledDriver) {
        ESP_LOGE(kTag, "Self-test FAIL: LED driver unavailable");
        return ESP_FAIL;
    }
    err = ledDriver->setPixel(0, domes::Color::green());
    if (err == ESP_OK) {
        err = ledDriver->refresh();
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Self-test FAIL: LED output error: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(kTag, "  [PASS] LED output path OK");

    // Test 6: host-facing transports and runtime control services are live.
    if (!modeManager || !serialOtaReceiver || !bleOtaReceiver || !espNowService) {
        ESP_LOGE(kTag, "Self-test FAIL: required runtime service unavailable");
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "  [PASS] Runtime services initialized");

    ESP_LOGI(kTag, "Self-test PASSED");
    return ESP_OK;
}
#endif

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

#ifdef DOMES_FORCE_OTA_VERIFY_FAILURE
    ESP_LOGE(kTag, "Injecting OTA verification failure for rollback test image");
    esp_err_t selfTestResult = ESP_FAIL;
#else
    esp_err_t selfTestResult = performSelfTest();
#endif

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
        otaManager = nullptr;
        githubClient = nullptr;
        return err;
    }

    domes::FirmwareVersion ver = otaManager->getCurrentVersion();
    ESP_LOGI(kTag, "Firmware version: %lu.%lu.%lu (partition: %s)",
             static_cast<unsigned long>(ver.major), static_cast<unsigned long>(ver.minor),
             static_cast<unsigned long>(ver.patch), otaManager->getCurrentPartition());

    return ESP_OK;
}

/**
 * @brief Initialize feature manager for runtime config
 *
 * Must be called before TCP config server and serial OTA receiver,
 * as both use the feature manager.
 */
static void initFeatureManager() {
    const esp_err_t err = runtimeAssembly.initFeatureManager(
        domes::runtime_profile::kSupportedFeatureMask,
        [](domes::config::Feature feature, bool enabled) {
            if (feature == domes::config::Feature::kBleAdvertising && bleOtaService) {
                bleOtaService->setAdvertisingEnabled(enabled);
            }
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
            if (feature == domes::config::Feature::kWifi && wifiManager) {
                if (enabled) {
                    const esp_err_t wifiErr = wifiManager->connect();
                    if (wifiErr != ESP_OK) {
                        ESP_LOGE(kTag, "Failed to enable WiFi client: %s",
                                 esp_err_to_name(wifiErr));
                    }
                } else {
                    const esp_err_t wifiErr = wifiManager->disconnect();
                    if (wifiErr != ESP_OK) {
                        ESP_LOGE(kTag, "Failed to disable WiFi client: %s",
                                 esp_err_to_name(wifiErr));
                    }
                }
            }
#endif
        });
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Feature manager init failed: %s", esp_err_to_name(err));
        return;
    }
    featureManager = runtimeAssembly.handles().features;
    ESP_LOGI(kTag, "Feature manager initialized");
}

/**
 * @brief Initialize system mode manager
 *
 * Creates ModeManager and starts a 10Hz tick task for timeout monitoring.
 * Must be called after initFeatureManager().
 */
static esp_err_t initModeManager() {
    if (!featureManager) {
        ESP_LOGE(kTag, "Cannot init mode manager: featureManager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t err = runtimeAssembly.initModeManager();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create mode manager tick task");
        return err;
    }
    modeManager = runtimeAssembly.handles().modes;
    ESP_LOGI(kTag, "Mode manager initialized (BOOTING)");
    return ESP_OK;
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

    const esp_err_t err = runtimeAssembly.initLedService(*ledDriver);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "LED service start failed: %s", esp_err_to_name(err));
        return err;
    }
    ledService = runtimeAssembly.handles().led;
    ESP_LOGI(kTag, "LED service started, LED effects enabled");
    return ESP_OK;
}

/**
 * @brief Initialize I2C master bus
 *
 * Sets up the I2C bus for IMU and haptic driver communication.
 */
static esp_err_t initI2c() {
    ESP_LOGI(kTag, "Initializing I2C bus (SDA=%d, SCL=%d)...", static_cast<int>(pins::kI2cSda),
             static_cast<int>(pins::kI2cScl));

    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = I2C_NUM_0;
    busConfig.sda_io_num = pins::kI2cSda;
    busConfig.scl_io_num = pins::kI2cScl;
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&busConfig, &i2cBus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "I2C bus initialized");
    return ESP_OK;
}

/**
 * @brief Initialize IMU driver
 *
 * Creates and initializes the LIS2DW12 IMU driver.
 * Requires I2C bus to be initialized first.
 */
static esp_err_t initImu() {
    if (!i2cBus) {
        ESP_LOGE(kTag, "Cannot init IMU: I2C bus not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Initializing LIS2DW12 IMU at address 0x%02X...", pins::kLis2dw12Addr);

    static domes::Lis2dw12Driver driver(i2cBus, pins::kLis2dw12Addr);
    imuDriver = &driver;

    esp_err_t err = imuDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "IMU init failed: %s", esp_err_to_name(err));
        imuDriver = nullptr;
        return err;
    }

    ESP_LOGI(kTag, "LIS2DW12 IMU initialized");
    domes::infra::HardwareStatus::markReady(domes::infra::HardwareSubsystem::kImu);
    return ESP_OK;
}

/**
 * @brief Initialize haptic driver
 *
 * Creates and initializes the DRV2605L haptic driver.
 * Requires I2C bus to be initialized first.
 */
static esp_err_t initHaptic() {
    if (!i2cBus) {
        ESP_LOGE(kTag, "Cannot init haptic: I2C bus not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Initializing DRV2605L haptic driver at address 0x%02X...", pins::kDrv2605lAddr);

    static domes::Drv2605lDriver driver(i2cBus, pins::kDrv2605lAddr);
    hapticDriver = &driver;

    esp_err_t err = hapticDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Haptic driver init failed: %s", esp_err_to_name(err));
        hapticDriver = nullptr;
        return err;
    }

    // Play a short click to confirm haptic is working
    err = hapticDriver->playEffect(domes::HapticEffect::kSharpClick100);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Haptic output test failed: %s", esp_err_to_name(err));
        hapticDriver = nullptr;
        return err;
    }

    ESP_LOGI(kTag, "DRV2605L haptic driver initialized");
    domes::infra::HardwareStatus::markReady(domes::infra::HardwareSubsystem::kHaptic);
    return ESP_OK;
}

/**
 * @brief Initialize IMU service
 *
 * Creates and starts the IMU service for triage mode.
 * Requires IMU driver and LED service to be initialized first.
 */
static esp_err_t initImuService() {
    if (!imuDriver || !ledService || !featureManager) {
        ESP_LOGE(kTag, "Cannot init IMU service: dependencies not ready");
        return ESP_FAIL;
    }

    const esp_err_t err = runtimeAssembly.initImuService(*imuDriver);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "IMU service start failed: %s", esp_err_to_name(err));
        imuService = nullptr;
        return err;
    }
    imuService = runtimeAssembly.handles().imu;
    ESP_LOGI(kTag, "IMU service started (triage mode enabled by default)");
    return ESP_OK;
}

/**
 * @brief Initialize audio driver
 *
 * Creates and initializes the MAX98357A audio driver.
 */
static esp_err_t initAudioDriver() {
    ESP_LOGI(kTag, "Initializing MAX98357A audio driver...");
    ESP_LOGI(kTag, "  BCLK=%d, LRCLK=%d, DOUT=%d, SD=%d", static_cast<int>(pins::kI2sBclk),
             static_cast<int>(pins::kI2sLrclk), static_cast<int>(pins::kI2sDout),
             static_cast<int>(pins::kAudioSd));

    static domes::Max98357aDriver driver(pins::kI2sBclk, pins::kI2sLrclk, pins::kI2sDout,
                                         pins::kAudioSd);
    audioDriver = &driver;

    esp_err_t err = audioDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Audio driver init failed: %s", esp_err_to_name(err));
        audioDriver = nullptr;
        return err;
    }

    ESP_LOGI(kTag, "MAX98357A audio driver initialized");
    domes::infra::HardwareStatus::markReady(domes::infra::HardwareSubsystem::kAudio);
    return ESP_OK;
}

/**
 * @brief Initialize audio service
 *
 * Creates and starts the audio service for playback.
 * Requires audio driver and feature manager to be initialized first.
 */
static esp_err_t initAudioService() {
    if (!audioDriver || !featureManager) {
        ESP_LOGE(kTag, "Cannot init audio service: dependencies not ready");
        return ESP_FAIL;
    }

    const esp_err_t err = runtimeAssembly.initAudioService(*audioDriver);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Audio service start failed: %s", esp_err_to_name(err));
        audioService = nullptr;
        return err;
    }
    audioService = runtimeAssembly.handles().audio;
    ESP_LOGI(kTag, "Audio service started");
    return ESP_OK;
}

/**
 * @brief Initialize touch driver and service
 *
 * Sets up capacitive touch sensing on 4 pads and starts the touch
 * monitoring service that controls LED colors based on which pad is touched.
 *
 * Requires ledDriver and featureManager to be initialized first.
 */
static esp_err_t initTouch() {
    if (!ledService || !featureManager) {
        ESP_LOGE(kTag, "Cannot init touch: dependencies not ready");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Initializing touch driver...");

    // Create touch driver with pin configuration from config.hpp
    static std::array<gpio_num_t, pins::kTouchPadCount> touchPins = {pins::kTouch1, pins::kTouch2,
                                                                     pins::kTouch3, pins::kTouch4};
    static domes::TouchDriver<pins::kTouchPadCount> driver(touchPins);
    touchDriver = &driver;

    esp_err_t err = touchDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Touch driver init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(kTag, "Touch driver initialized with %d pads", pins::kTouchPadCount);

    err = runtimeAssembly.initTouchService(
        *touchDriver, bleOtaReceiver ? publishBleTouchEvent : nullptr, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Touch service start failed: %s", esp_err_to_name(err));
        return err;
    }
    touchService = runtimeAssembly.handles().touch;
    domes::infra::HardwareStatus::markReady(domes::infra::HardwareSubsystem::kTouch);
    ESP_LOGI(kTag, "Touch service started, feature enabled");
    return ESP_OK;
}

/**
 * @brief Initialize game engine
 *
 * Creates the GameEngine, wires feedback callbacks to LED/audio services,
 * and starts the game tick task on Core 1.
 *
 * Requires touchDriver, ledService, and audioService to be initialized first.
 */
static esp_err_t initGameEngine() {
    if (!touchDriver) {
        ESP_LOGE(kTag, "Cannot init game engine: touchDriver not initialized");
        return ESP_FAIL;
    }

    const esp_err_t err = runtimeAssembly.initGameEngine(*touchDriver);
    if (err != ESP_OK) {
        gameEngine = nullptr;
        injectableTouchDriver = nullptr;
        ESP_LOGE(kTag, "Failed to create game tick task");
        return err;
    }
    injectableTouchDriver = runtimeAssembly.handles().injectableTouch;
    gameEngine = runtimeAssembly.handles().game;
    ESP_LOGI(kTag, "Game engine initialized");
    return ESP_OK;
}

/**
 * @brief Initialize the ESP-NOW transport
 *
 * Creates ESP-NOW transport and receiver task.
 * Requires WiFi stack to be initialized first.
 */
static esp_err_t initEspNowTransport() {
    // Always init ESP-NOW hardware — feature flag gates runtime behavior, not init
    ESP_LOGI(kTag, "Initializing ESP-NOW transport...");

    static domes::EspNowTransport transport;
    espNowTransport = &transport;

    domes::TransportError err = espNowTransport->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "ESP-NOW transport init failed: %s", domes::transportErrorToString(err));
        espNowTransport = nullptr;
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "ESP-NOW transport initialized");
    return ESP_OK;
}

/** Prepare and start the ESP-NOW service after its platform inputs exist. */
static esp_err_t initEspNowService(domes::IPlatformIdentity& platformIdentity,
                                   domes::IRandomSource& randomSource) {
    if (!espNowTransport) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t inputErr =
        runtimeAssembly.prepareEspNowService(*espNowTransport, platformIdentity, randomSource);
    if (inputErr != ESP_OK) {
        ESP_LOGE(kTag, "ESP-NOW platform input init failed: %s", esp_err_to_name(inputErr));
        espNowTransport->disconnect();
        espNowTransport = nullptr;
        return inputErr;
    }

    const esp_err_t espErr = runtimeAssembly.startEspNowService(taskManager);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create ESP-NOW service task: %s", esp_err_to_name(espErr));
        espNowService = nullptr;
        return espErr;
    }
    espNowService = runtimeAssembly.handles().espNow;

    ESP_LOGI(kTag, "ESP-NOW service initialized");
    return ESP_OK;
}

/**
 * @brief Initialize serial OTA receiver
 *
 * Sets up UART0 through the DevKit's CP2102N bridge and starts the serial
 * protocol receiver task. Native USB remains dedicated to console logging.
 */
static esp_err_t initSerialOta(uint8_t podId = 0) {
    ESP_LOGI(kTag, "Initializing serial OTA receiver...");

    static domes::UartTransport transport(UART_NUM_0, pins::kUartTx, pins::kUartRx);
    uartTransport = &transport;

    domes::TransportError err = uartTransport->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "UART transport init failed: %s", domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "UART protocol transport initialized");

    // Create serial OTA receiver with config support
    static domes::SerialOtaReceiver receiver(*uartTransport, featureManager, podId);
    serialOtaReceiver = &receiver;

    // Wire up services for config commands
    if (ledService) {
        serialOtaReceiver->setLedService(ledService);
    }
    if (imuService) {
        serialOtaReceiver->setImuService(imuService);
    }
    if (modeManager) {
        serialOtaReceiver->setModeManager(modeManager);
    }
    if (espNowTransport) {
        serialOtaReceiver->setEspNowTransport(espNowTransport);
    }
    if (espNowService) {
        serialOtaReceiver->setEspNowService(espNowService);
    }
    if (otaManager) {
        serialOtaReceiver->setOtaManager(otaManager);
    }
    if (injectableTouchDriver) {
        serialOtaReceiver->setInjectableTouchDriver(injectableTouchDriver);
    }

    // Create receiver task
    const esp_err_t espErr = taskManager.createTask(domes::infra::task::kSerialOta, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create serial OTA task: %s", esp_err_to_name(espErr));
        serialOtaReceiver = nullptr;
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
static esp_err_t initBleOta(uint8_t podId = 0) {
    if (!featureManager) {
        ESP_LOGE(kTag, "Cannot init BLE OTA: featureManager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(kTag, "Initializing BLE OTA service...");

    // Create BLE OTA service (GATT server)
    static domes::BleOtaService service;
    bleOtaService = &service;

    domes::TransportError err = bleOtaService->init();
    if (!domes::isOk(err)) {
        ESP_LOGE(kTag, "BLE OTA service init failed: %s", domes::transportErrorToString(err));
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "BLE OTA service initialized, advertising started");

    // Create BLE OTA receiver (reuses SerialOtaReceiver with BLE transport).
    static domes::SerialOtaReceiver receiver(*bleOtaService, featureManager, podId);
    bleOtaReceiver = &receiver;
    if (touchService) {
        touchService->setEventCallback(publishBleTouchEvent, nullptr);
    }

    // Create receiver task (needs 8KB stack for config command processing and protobuf)
    const esp_err_t espErr = taskManager.createTask(domes::infra::task::kBleOta, receiver);
    if (espErr != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create BLE OTA task: %s", esp_err_to_name(espErr));
        bleOtaService->setAdvertisingEnabled(false);
        bleOtaReceiver = nullptr;
        bleOtaService = nullptr;
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

    // Create TCP config server
    static domes::TcpConfigServer server(*featureManager, domes::kConfigServerPort);
    tcpConfigServer = &server;

    // Wire up services for config commands over TCP
    if (ledService) {
        tcpConfigServer->setLedService(ledService);
    }
    if (imuService) {
        tcpConfigServer->setImuService(imuService);
    }
    if (modeManager) {
        tcpConfigServer->setModeManager(modeManager);
    }
    if (espNowTransport) {
        tcpConfigServer->setEspNowTransport(espNowTransport);
    }
    if (espNowService) {
        tcpConfigServer->setEspNowService(espNowService);
    }
    if (otaManager) {
        tcpConfigServer->setOtaManager(otaManager);
    }
    if (injectableTouchDriver) {
        tcpConfigServer->setInjectableTouchDriver(injectableTouchDriver);
    }

    // Create server task
    const esp_err_t err = taskManager.createTask(domes::infra::task::kTcpConfig, server);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create TCP config server task: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "TCP config server started on port %u", domes::kConfigServerPort);
    return ESP_OK;
}
#endif  // CONFIG_DOMES_WIFI_AUTO_CONNECT

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

    // Provisioned credentials are authoritative. Compile-time secrets seed NVS
    // only on the first boot of a WiFi-enabled development build.
    if (wifiManager->hasStoredCredentials()) {
        ESP_LOGI(kTag, "Connecting with provisioned WiFi credentials");
        err = wifiManager->connect();
    } else {
        ESP_LOGI(kTag, "No provisioned WiFi credentials; seeding from secrets.hpp");
        err = wifiManager->connect(domes::secrets::kWifiSsid, domes::secrets::kWifiPassword, true);
    }
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
        ESP_LOGI(kTag, "WiFi connected! IP: %s, RSSI: %d dBm", ip, wifiManager->getRssi());
        return ESP_OK;
    } else {
        ESP_LOGE(kTag, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}
/**
 * @brief Initialize mDNS service advertisement
 *
 * Advertises _domes._tcp.local. on port 5000 so the CLI can auto-discover
 * pods on the same network. Hostname is domes-pod-{pod_id}.local.
 *
 * @param podId Pod ID from NVS (0 = use MAC suffix)
 */
static esp_err_t initMdns(uint8_t podId) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Set hostname: domes-pod-1 or domes-pod-xxxx
    char hostname[32] = {};
    if (podId > 0) {
        snprintf(hostname, sizeof(hostname), "domes-pod-%u", podId);
    } else {
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(hostname, sizeof(hostname), "domes-pod-%02x%02x", mac[4], mac[5]);
    }
    mdns_hostname_set(hostname);
    mdns_instance_name_set("DOMES Pod");

    // Advertise TCP config service
    mdns_service_add("DOMES Config", "_domes", "_tcp", 5000, nullptr, 0);

    // Add TXT records with pod info
    char podIdStr[8] = {};
    snprintf(podIdStr, sizeof(podIdStr), "%u", podId);
    mdns_service_txt_item_set("_domes", "_tcp", "pod_id", podIdStr);
    mdns_service_txt_item_set("_domes", "_tcp", "version", domes::infra::firmwareVersion());

    ESP_LOGI(kTag, "mDNS: %s.local (_domes._tcp:5000)", hostname);
    return ESP_OK;
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

    // A disconnected station may enter modem sleep and miss peer broadcasts.
    // ESP-NOW discovery requires the radio to remain continuously receptive.
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_ps(WIFI_PS_NONE) failed: %s", esp_err_to_name(err));
        return err;
    }

    // Pin to channel 1 so all pods use the same channel for ESP-NOW.
    // Without this, STA mode without an AP connection has an undefined channel,
    // and pods may fail to discover each other.
    static constexpr uint8_t kEspNowChannel = 1;
    err = esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "esp_wifi_set_channel(%d) failed: %s", kEspNowChannel, esp_err_to_name(err));
    } else {
        ESP_LOGI(kTag, "WiFi channel pinned to %d for ESP-NOW", kEspNowChannel);
    }

    // Log success and heap usage
    size_t heapAfter = esp_get_free_heap_size();
    ESP_LOGI(kTag, "WiFi stack initialized (STA mode, channel %d, not connected)", kEspNowChannel);
    ESP_LOGI(kTag, "WiFi heap usage: %zu bytes", heapBefore - heapAfter);

    return ESP_OK;
}

static esp_err_t initLedStrip() {
    ESP_LOGI(kTag, "LED init: GPIO=%d, count=%d, RGBW=%s", static_cast<int>(pins::kLedData),
             pins::kLedCount, pins::kLedIsRgbw ? "yes" : "no");

    static domes::LedStripDriver<pins::kLedCount> driver(pins::kLedData, pins::kLedIsRgbw);
    ledDriver = &driver;

    esp_err_t err = ledDriver->init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "LED strip init FAILED: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(kTag, "LED strip init OK");
    ledDriver->setBrightness(led::kDefaultBrightness);
    err = ledDriver->clear();
    if (err == ESP_OK) {
        err = ledDriver->refresh();
    }
    if (err == ESP_OK) {
        domes::infra::HardwareStatus::markReady(domes::infra::HardwareSubsystem::kLed);
    }
    return err;
}

static esp_err_t initInfrastructure(uint32_t& bootCount) {
    bootCount = 0;
    esp_err_t err = domes::infra::NvsConfig::initFlash();
    if (err != ESP_OK)
        return err;

    err = configStorage.open(domes::infra::nvs_ns::kConfig);
    if (err != ESP_OK)
        return err;

    err = domes::infra::Watchdog::init(timing::kWatchdogTimeoutS, true);
    if (err != ESP_OK)
        return err;

    domes::infra::NvsConfig stats;
    err = stats.open(domes::infra::nvs_ns::kStats);
    if (err != ESP_OK)
        return err;

    uint32_t previousBootCount = 0;
    err = stats.getU32(domes::infra::stats_key::kBootCount, previousBootCount);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        previousBootCount = 0;
    } else if (err != ESP_OK) {
        stats.close();
        return err;
    }
    if (previousBootCount == UINT32_MAX) {
        bootCount = UINT32_MAX;
        ESP_LOGW(kTag, "Boot count saturated at %lu", static_cast<unsigned long>(bootCount));
    } else {
        bootCount = previousBootCount + 1;
    }
    err = stats.setU32(domes::infra::stats_key::kBootCount, bootCount);
    if (err == ESP_OK) {
        err = stats.commit();
    }
    stats.close();
    if (err != ESP_OK)
        return err;

    ESP_LOGI(kTag, "Boot #%lu", static_cast<unsigned long>(bootCount));

    return ESP_OK;
}

extern "C" void app_main() {
    domes::infra::TaskStartEvidence::markStarted(domes::infra::task::kMain);
    domes::runtime::InitOrderTracker initOrder;
    ESP_LOGI(kTag, "DOMES Firmware %s", domes::infra::firmwareVersion());

    // Initialize trace system early
    if (!advanceInitStage(initOrder, "trace")) {
        return;
    }
    esp_err_t traceErr = domes::trace::Recorder::init();
    if (traceErr == ESP_OK) {
        domes::trace::Recorder::setEnabled(false);
        const auto& mainTask = domes::infra::task::kMain;
        domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), mainTask.name,
                                             mainTask.traceId, mainTask.priority,
                                             mainTask.coreAffinity);
        ESP_LOGI(kTag, "Trace system initialized (recording disabled until requested)");
    } else {
        ESP_LOGW(kTag, "Trace init failed: %s", esp_err_to_name(traceErr));
    }

    // Initialize infrastructure first
    if (!advanceInitStage(initOrder, "infrastructure")) {
        return;
    }
    uint32_t bootCount = 0;
    if (initInfrastructure(bootCount) != ESP_OK) {
        ESP_LOGE(kTag, "Infrastructure init failed, halting");
        return;
    }
    ESP_LOGI(kTag, "Infrastructure initialized");

    // Initialize shutdown dump handler (captures diagnostics on clean esp_restart() only)
    if (!advanceInitStage(initOrder, "shutdown_dump")) {
        return;
    }
    if (domes::infra::ShutdownDumpHandler::init(bootCount) != ESP_OK) {
        ESP_LOGW(kTag, "Shutdown dump handler init failed");
    }

    // Read pod ID from NVS (used for BLE naming and multi-pod identification)
    [[maybe_unused]] uint8_t podId = readPodId();
    runtimePodId = podId;

    // Initialize hardware drivers
    if (!advanceInitStage(initOrder, "led_driver")) {
        return;
    }
    if (initLedStrip() != ESP_OK) {
        ESP_LOGW(kTag, "LED init failed, continuing without LED");
    }

    // Initialize I2C and I2C devices (IMU, haptic)
    if (!advanceInitStage(initOrder, "i2c_bus")) {
        return;
    }
    const bool i2cReady = initI2c() == ESP_OK;
    if (!i2cReady) {
        ESP_LOGW(kTag, "I2C init failed, continuing without I2C devices");
    }
    if (!advanceInitStage(initOrder, "imu_driver")) {
        return;
    }
    if (i2cReady) {
        if (initImu() != ESP_OK) {
            ESP_LOGW(kTag, "IMU init failed, continuing without IMU");
        }
    }
    if (!advanceInitStage(initOrder, "haptic_driver")) {
        return;
    }
    if (i2cReady) {
        if (initHaptic() != ESP_OK) {
            ESP_LOGW(kTag, "Haptic init failed, continuing without haptic");
        }
    }

    // Initialize audio driver
    if (!advanceInitStage(initOrder, "audio_driver")) {
        return;
    }
    if (initAudioDriver() != ESP_OK) {
        ESP_LOGW(kTag, "Audio driver init failed, continuing without audio");
    }

    // Initialize WiFi stack (required for ESP-NOW and BLE coexistence)
    if (!advanceInitStage(initOrder, "wifi")) {
        return;
    }
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
    if (!advanceInitStage(initOrder, "ota")) {
        return;
    }
    if (initOta() != ESP_OK) {
        ESP_LOGW(kTag, "OTA init failed, continuing without OTA support");
    }

    // Initialize feature manager FIRST (needed by BLE, TCP config server, and serial OTA)
    if (!advanceInitStage(initOrder, "feature_manager")) {
        return;
    }
    initFeatureManager();

    // Initialize mode manager (after feature manager, before services)
    if (!advanceInitStage(initOrder, "mode_manager")) {
        return;
    }
    if (initModeManager() != ESP_OK) {
        ESP_LOGE(kTag, "Mode manager init failed");
        if (otaManager && otaManager->isPendingVerification()) {
            otaManager->rollback();
        }
        return;
    }

    // Initialize diagnostics (after trace, before services)
    if (!advanceInitStage(initOrder, "diagnostics")) {
        return;
    }
    if (runtimeAssembly.initDiagnostics() != ESP_OK) {
        ESP_LOGW(kTag, "Diagnostics task start failed");
    }

    // Initialize memory profiler (periodic heap sampling + trace counters)
    if (!advanceInitStage(initOrder, "memory_profiler")) {
        return;
    }
    if (runtimeAssembly.initMemoryProfiler() == ESP_OK) {
        ESP_LOGI(kTag, "Memory profiler initialized (5s interval)");
    } else {
        ESP_LOGW(kTag, "Memory profiler init failed");
    }

    // Initialize LED service (needed for LED pattern commands)
    if (!advanceInitStage(initOrder, "led_service")) {
        return;
    }
    if (initLedService() != ESP_OK) {
        ESP_LOGW(kTag, "LED service init failed, continuing without LED patterns");
    }

    // Initialize IMU service (needed for triage mode)
    if (!advanceInitStage(initOrder, "imu_service")) {
        return;
    }
    if (imuDriver && ledService) {
        if (initImuService() != ESP_OK) {
            ESP_LOGW(kTag, "IMU service init failed, continuing without triage mode");
        } else if (hapticDriver) {
            runtimeAssembly.connectImuFeedback(hapticDriver, audioService);
        }
    }

    // Initialize audio service (needed for audio playback)
    if (!advanceInitStage(initOrder, "audio_service")) {
        return;
    }
    if (audioDriver) {
        if (initAudioService() != ESP_OK) {
            ESP_LOGW(kTag, "Audio service init failed, continuing without audio");
        } else {
            // Wire up audio service to IMU service for tap sounds
            if (imuService) {
                runtimeAssembly.connectImuFeedback(hapticDriver, audioService);
            }
        }
    }

    // Initialize touch driver and service
    if (!advanceInitStage(initOrder, "touch")) {
        return;
    }
    if (initTouch() != ESP_OK) {
        ESP_LOGW(kTag, "Touch init failed, continuing without touch support");
    }

    // Initialize game engine (after touch, LED, and audio services)
    if (!advanceInitStage(initOrder, "game_engine")) {
        return;
    }
    if (touchDriver) {
        if (initGameEngine() != ESP_OK) {
            ESP_LOGW(kTag, "Game engine init failed, continuing without game support");
        }
    }

    // Initialize BLE OTA service (after feature manager so config commands work over BLE)
    if (!advanceInitStage(initOrder, "ble_ota")) {
        return;
    }
    ESP_LOGI(kTag, "Initializing BLE stack...");
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay to flush logs
    size_t heapBeforeBle = esp_get_free_heap_size();
    if (initBleOta(podId) != ESP_OK) {
        ESP_LOGW(kTag, "BLE OTA init failed, continuing without BLE OTA");
    } else {
        bleOtaService->setAdvertisingEnabled(
            featureManager->isEnabled(domes::config::Feature::kBleAdvertising));
        size_t heapAfterBle = esp_get_free_heap_size();
        ESP_LOGI(kTag, "BLE stack initialized (NimBLE + advertising)");
        ESP_LOGI(kTag, "BLE heap usage: %zu bytes", heapBeforeBle - heapAfterBle);

        // Wire up services for config commands over BLE
        if (bleOtaReceiver && ledService) {
            bleOtaReceiver->setLedService(ledService);
        }
        if (bleOtaReceiver && imuService) {
            bleOtaReceiver->setImuService(imuService);
        }
        if (bleOtaReceiver && modeManager) {
            bleOtaReceiver->setModeManager(modeManager);
        }
        if (bleOtaReceiver && espNowTransport) {
            bleOtaReceiver->setEspNowTransport(espNowTransport);
        }
        if (bleOtaReceiver && espNowService) {
            bleOtaReceiver->setEspNowService(espNowService);
        }
        if (bleOtaReceiver && otaManager) {
            bleOtaReceiver->setOtaManager(otaManager);
        }
        if (bleOtaReceiver && injectableTouchDriver) {
            bleOtaReceiver->setInjectableTouchDriver(injectableTouchDriver);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay to flush logs

    vTaskDelay(pdMS_TO_TICKS(500));  // Let BLE settle

    // Initialize ESP-NOW transport and service (after WiFi init, after BLE).
    if (!advanceInitStage(initOrder, "esp_now_transport")) {
        return;
    }
    const bool espNowTransportReady = initEspNowTransport() == ESP_OK;
    if (!espNowTransportReady) {
        ESP_LOGW(kTag, "ESP-NOW transport init failed, continuing without ESP-NOW");
    }

    if (!advanceInitStage(initOrder, "platform_identity")) {
        return;
    }
    static domes::platform::EspPlatformIdentity platformIdentity;
    if (!advanceInitStage(initOrder, "platform_random")) {
        return;
    }
    static domes::platform::EspRandomSource randomSource;
    if (!advanceInitStage(initOrder, "esp_now_service")) {
        return;
    }
    const bool espNowReady =
        espNowTransportReady && initEspNowService(platformIdentity, randomSource) == ESP_OK;
    if (espNowTransportReady && !espNowReady) {
        ESP_LOGW(kTag, "ESP-NOW service init failed, continuing without ESP-NOW");
    } else if (espNowReady && bleOtaReceiver) {
        // BLE is initialized first, so its config handler could not receive
        // ESP-NOW dependencies during the initial wiring pass.
        bleOtaReceiver->setEspNowTransport(espNowTransport);
        bleOtaReceiver->setEspNowService(espNowService);
    }

    // Initialize TCP config server (WiFi-based config) - BEFORE serial OTA takes console
    if (!advanceInitStage(initOrder, "tcp_config")) {
        return;
    }
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    const bool wifiConfigReady = wifiManager && wifiManager->isConnected();
    if (wifiConfigReady) {
        ESP_LOGI(kTag, "WiFi connected, starting TCP config server...");
        if (initTcpConfigServer() != ESP_OK) {
            ESP_LOGW(kTag, "TCP config server init failed");
        }
    } else {
        ESP_LOGI(kTag, "TCP config server not started (WiFi not connected)");
    }
#endif

    // Advertise via mDNS for device discovery.
    if (!advanceInitStage(initOrder, "mdns")) {
        return;
    }
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (wifiConfigReady) {
        initMdns(podId);
    }
#endif

    // Start trace stream server (port 5001 for live trace streaming).
    if (!advanceInitStage(initOrder, "trace_stream")) {
        return;
    }
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (wifiConfigReady) {
        static domes::trace::TraceStreamServer traceStreamServer;
        const auto& traceTask = domes::infra::task::kTraceStream;
        struct TraceStreamTaskContext {
            domes::trace::TraceStreamServer* server;
            SemaphoreHandle_t registrationDone;
            bool registered;
        };
        static StaticSemaphore_t traceRegistrationStorage;
        static TraceStreamTaskContext traceContext{
            &traceStreamServer, xSemaphoreCreateBinaryStatic(&traceRegistrationStorage), false};
        const BaseType_t traceTaskCreated =
            traceContext.registrationDone != nullptr
                ? xTaskCreate(
                      [](void* param) {
                          auto* context = static_cast<TraceStreamTaskContext*>(param);
                          const auto& task = domes::infra::task::kTraceStream;
                          context->registered = domes::trace::Recorder::registerTask(
                              xTaskGetCurrentTaskHandle(), task.name, task.traceId, task.priority,
                              task.coreAffinity);
                          xSemaphoreGive(context->registrationDone);
                          context->server->run();
                          vTaskDelete(nullptr);
                      },
                      traceTask.name, traceTask.stackSize, &traceContext, traceTask.priority,
                      nullptr)
                : pdFAIL;
        if (traceTaskCreated == pdPASS &&
            xSemaphoreTake(traceContext.registrationDone, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(kTag, "Trace stream server started on port 5001 (trace_id=%s)",
                     traceContext.registered ? "registered" : "unavailable");
        } else {
            ESP_LOGW(kTag, "Failed to create or register trace stream task");
        }
    }
#endif

    // Initialize the CP2102N/UART protocol receiver. Console logs stay on native USB.
    if (!advanceInitStage(initOrder, "serial_ota")) {
        return;
    }
    if (initSerialOta(podId) != ESP_OK) {
        ESP_LOGW(kTag, "Serial OTA init failed, continuing without serial OTA");
    }

    // Transition from BOOTING → IDLE now that all services are up
    if (!advanceInitStage(initOrder, "mode_idle")) {
        return;
    }
    if (modeManager) {
        runtimeAssembly.transitionToIdle();
        ESP_LOGI(kTag, "System mode: BOOTING → IDLE");
    }

    // Confirm a new image only after the complete runtime is initialized.
    if (!advanceInitStage(initOrder, "ota_verification")) {
        return;
    }
    handleOtaVerification();

    // Green LED = boot success
    if (!advanceInitStage(initOrder, "ready_led")) {
        return;
    }
    if (ledDriver) {
        ledDriver->setAll(domes::Color::green());
        ledDriver->refresh();
    }

    ESP_LOGI(kTag, "Init complete. Tasks: %zu, Heap: %lu", taskManager.getActiveTaskCount(),
             static_cast<unsigned long>(esp_get_free_heap_size()));

    // The persisted setting overrides the Kconfig build default on subsequent boots.
    if (!advanceInitStage(initOrder, "ota_check")) {
        return;
    }
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    const bool autoUpdateEnabled = readAutoUpdateEnabled();
    if (autoUpdateEnabled && wifiManager && wifiManager->isConnected() && otaManager) {
        ESP_LOGI(kTag, "Creating OTA check task...");

        const auto& otaTask = domes::infra::task::kOtaCheck;
        struct OtaTaskContext {
            domes::OtaManager* manager;
            SemaphoreHandle_t registrationDone;
            bool registered;
        };
        static StaticSemaphore_t otaRegistrationStorage;
        static OtaTaskContext otaContext{
            otaManager, xSemaphoreCreateBinaryStatic(&otaRegistrationStorage), false};
        const BaseType_t otaTaskCreated =
            otaContext.registrationDone != nullptr
                ? xTaskCreate(
                      [](void* param) {
                          const auto& task = domes::infra::task::kOtaCheck;
                          auto* context = static_cast<OtaTaskContext*>(param);
                          context->registered = domes::trace::Recorder::registerTask(
                              xTaskGetCurrentTaskHandle(), task.name, task.traceId, task.priority,
                              task.coreAffinity);
                          xSemaphoreGive(context->registrationDone);
                          auto* manager = context->manager;
                          ESP_LOGI(kTag, "OTA check task started");
                          ESP_LOGI(kTag, "Checking for firmware updates...");

                          domes::OtaCheckResult updateResult;
                          esp_err_t err = manager->checkForUpdate(updateResult);

                          if (err == ESP_OK) {
                              if (updateResult.updateAvailable) {
                                  ESP_LOGI(
                                      kTag, "Update available: v%lu.%lu.%lu -> v%lu.%lu.%lu",
                                      static_cast<unsigned long>(updateResult.currentVersion.major),
                                      static_cast<unsigned long>(updateResult.currentVersion.minor),
                                      static_cast<unsigned long>(updateResult.currentVersion.patch),
                                      static_cast<unsigned long>(
                                          updateResult.availableVersion.major),
                                      static_cast<unsigned long>(
                                          updateResult.availableVersion.minor),
                                      static_cast<unsigned long>(
                                          updateResult.availableVersion.patch));
                                  ESP_LOGI(kTag, "Download URL: %s", updateResult.downloadUrl);
                                  ESP_LOGI(kTag, "Firmware size: %zu bytes",
                                           updateResult.firmwareSize);

                                  // Start OTA update
                                  ESP_LOGI(kTag, "Starting OTA update...");
                                  err = manager->startUpdate(
                                      updateResult.downloadUrl,
                                      updateResult.sha256[0] ? updateResult.sha256 : nullptr);
                                  if (err != ESP_OK) {
                                      ESP_LOGE(kTag, "OTA update failed to start: %s",
                                               esp_err_to_name(err));
                                  }
                                  // If successful, device will reboot
                              } else {
                                  ESP_LOGI(
                                      kTag, "Firmware is up to date (v%lu.%lu.%lu)",
                                      static_cast<unsigned long>(updateResult.currentVersion.major),
                                      static_cast<unsigned long>(updateResult.currentVersion.minor),
                                      static_cast<unsigned long>(
                                          updateResult.currentVersion.patch));
                              }
                          } else {
                              ESP_LOGW(kTag, "Update check failed: %s", esp_err_to_name(err));
                          }

                          ESP_LOGI(kTag, "OTA check task done, deleting self");
                          vTaskDelete(nullptr);
                      },
                      otaTask.name, otaTask.stackSize, &otaContext, otaTask.priority, nullptr)
                : pdFAIL;
        if (otaTaskCreated != pdPASS ||
            xSemaphoreTake(otaContext.registrationDone, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(kTag, "Failed to create or register OTA check task");
        } else if (!otaContext.registered) {
            ESP_LOGW(kTag, "OTA check running without a trace identity");
        }
    } else if (autoUpdateEnabled) {
        ESP_LOGW(kTag, "Auto-update enabled but WiFi or OTA manager is unavailable");
    } else {
        ESP_LOGI(kTag, "Automatic OTA check disabled by persisted setting");
    }
#endif  // CONFIG_DOMES_WIFI_AUTO_CONNECT
    const size_t registeredTraceTasks = domes::runtime::registerRuntimeTraceTasks();
    domes::trace::Recorder::finalizeTaskCatalog();
    ESP_LOGI(kTag, "Registered %zu runtime trace task identities", registeredTraceTasks);
#ifdef CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE
    const auto traceAcceptance = domes::trace::runTraceAcceptanceProbe();
    ESP_LOGI(kTag,
             "Trace acceptance: passed=%u events=%" PRIu32 " drops=%" PRIu32
             " discontinuities=%" PRIu32 " disabled_us=%" PRIu32 " enabled_us=%" PRIu32,
             traceAcceptance.passed ? 1U : 0U, traceAcceptance.eventCount,
             traceAcceptance.droppedCount, traceAcceptance.discontinuityCount,
             traceAcceptance.disabledRecordUs, traceAcceptance.enabledRecordUs);
#endif
    if (!initOrder.complete()) {
        ESP_LOGE(kTag, "Init-order incomplete: expected=%s", initOrder.expected());
    }
}
