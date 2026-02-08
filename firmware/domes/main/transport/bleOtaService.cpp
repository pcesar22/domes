/**
 * @file bleOtaService.cpp
 * @brief BLE OTA service implementation using NimBLE
 */

#include "bleOtaService.hpp"

#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "infra/nvsConfig.hpp"

// NimBLE headers (must include esp_nimble_hci.h first in ESP-IDF)
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <cstring>

namespace domes {

static const char* kTag = "ble_ota";

// ============================================================================
// UUIDs (128-bit, little-endian format for NimBLE)
// Must be static for BLE_UUID128_DECLARE to work in C++
// ============================================================================

// OTA Service: 12345678-1234-5678-1234-56789abcdef0
const uint8_t kOtaServiceUuid[16] = {0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12};

// OTA Data Characteristic: 12345678-1234-5678-1234-56789abcdef1
const uint8_t kOtaDataCharUuid[16] = {0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                                      0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12};

// OTA Status Characteristic: 12345678-1234-5678-1234-56789abcdef2
const uint8_t kOtaStatusCharUuid[16] = {0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                                        0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12};

// Static UUID structures for NimBLE (avoids taking address of temporaries in C++)
static const ble_uuid128_t kOtaServiceUuid128 = {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
              0x34, 0x12}};

static const ble_uuid128_t kOtaDataCharUuid128 = {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
              0x34, 0x12}};

static const ble_uuid128_t kOtaStatusCharUuid128 = {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
              0x34, 0x12}};

// ============================================================================
// Global instance pointer (for NimBLE callbacks)
// ============================================================================

static BleOtaService* g_bleOtaService = nullptr;

// ============================================================================
// GATT Access Callback
// ============================================================================

static int otaGattAccessCb(uint16_t connHandle, uint16_t attrHandle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (g_bleOtaService == nullptr) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    const ble_uuid_t* uuid = ctxt->chr->uuid;

    // Check if this is the Data characteristic (write)
    if (ble_uuid_cmp(uuid, &kOtaDataCharUuid128.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            // Data written to OTA Data characteristic
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t buf[512];

            if (len > sizeof(buf)) {
                ESP_LOGW(kTag, "Write too large: %u bytes", len);
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, len, nullptr);
            if (rc != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }

            g_bleOtaService->onDataReceived(buf, len);
            return 0;
        }
    }

    // Check if this is the Status characteristic (read/notify)
    if (ble_uuid_cmp(uuid, &kOtaStatusCharUuid128.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            // Allow read (returns empty)
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ============================================================================
// GATT Service Definition
// ============================================================================

static uint16_t g_statusCharHandle = 0;

static const struct ble_gatt_chr_def otaCharacteristics[] = {
    {
        // OTA Data characteristic (write without response for speed)
        .uuid = &kOtaDataCharUuid128.u,
        .access_cb = otaGattAccessCb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = nullptr,
    },
    {
        // OTA Status characteristic (notify for ACK/ABORT)
        .uuid = &kOtaStatusCharUuid128.u,
        .access_cb = otaGattAccessCb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &g_statusCharHandle,
    },
    {
        // Terminator
        .uuid = nullptr,
        .access_cb = nullptr,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = 0,
        .min_key_size = 0,
        .val_handle = nullptr,
    }};

static const struct ble_gatt_svc_def otaServices[] = {{
                                                          .type = BLE_GATT_SVC_TYPE_PRIMARY,
                                                          .uuid = &kOtaServiceUuid128.u,
                                                          .includes = nullptr,
                                                          .characteristics = otaCharacteristics,
                                                      },
                                                      {
                                                          // Terminator
                                                          .type = 0,
                                                          .uuid = nullptr,
                                                          .includes = nullptr,
                                                          .characteristics = nullptr,
                                                      }};

// ============================================================================
// GAP Event Handler
// ============================================================================

static int bleGapEventCb(struct ble_gap_event* event, void* arg) {
    if (g_bleOtaService == nullptr) {
        return 0;
    }

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(kTag, "BLE connected, handle=%d", event->connect.conn_handle);
                g_bleOtaService->onConnectionStateChanged(true, event->connect.conn_handle);

                // Request MTU exchange for larger packets
                ble_gattc_exchange_mtu(event->connect.conn_handle, nullptr, nullptr);
            } else {
                ESP_LOGW(kTag, "BLE connection failed, status=%d", event->connect.status);
                g_bleOtaService->startAdvertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "BLE disconnected, reason=%d", event->disconnect.reason);
            g_bleOtaService->onConnectionStateChanged(false, 0);
            g_bleOtaService->startAdvertising();
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(kTag, "MTU updated: %d", event->mtu.value);
            g_bleOtaService->onMtuChanged(event->mtu.value);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGD(kTag, "Advertising complete");
            break;

        default:
            break;
    }

    return 0;
}

// ============================================================================
// NimBLE Host Task
// ============================================================================

static void bleHostTask(void* param) {
    ESP_LOGI(kTag, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void bleSyncCb() {
    ESP_LOGI(kTag, "NimBLE host synced");

    // Generate random address if needed
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to ensure address: %d", rc);
        return;
    }

    // Start advertising
    if (g_bleOtaService != nullptr) {
        g_bleOtaService->startAdvertising();
    } else {
        ESP_LOGE(kTag, "BLE service instance is null!");
    }
}

static void bleResetCb(int reason) {
    ESP_LOGW(kTag, "BLE reset, reason=%d", reason);
}

// ============================================================================
// BleOtaService Implementation
// ============================================================================

BleOtaService::BleOtaService() {
    rxMutex_ = xSemaphoreCreateMutex();
    rxSemaphore_ = xSemaphoreCreateBinary();
}

BleOtaService::~BleOtaService() {
    disconnect();

    if (rxMutex_) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(rxMutex_));
    }
    if (rxSemaphore_) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(rxSemaphore_));
    }
}

TransportError BleOtaService::init() {
    if (initialized_) {
        return TransportError::kAlreadyInit;
    }

    ESP_LOGI(kTag, "Initializing BLE OTA service");

    // Set global instance for callbacks
    g_bleOtaService = this;

    // Initialize NimBLE
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init NimBLE port: %d", rc);
        return TransportError::kIoError;
    }

    // Configure NimBLE host
    ble_hs_cfg.reset_cb = bleResetCb;
    ble_hs_cfg.sync_cb = bleSyncCb;
    ble_hs_cfg.gatts_register_cb = nullptr;
    ble_hs_cfg.store_status_cb = nullptr;

    // Initialize standard services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Register OTA service
    rc = ble_gatts_count_cfg(otaServices);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to count GATT services: %d", rc);
        return TransportError::kIoError;
    }

    rc = ble_gatts_add_svcs(otaServices);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to add GATT services: %d", rc);
        return TransportError::kIoError;
    }

    // Build dynamic device name from pod_id (NVS) or BT MAC suffix
    {
        char bleName[32] = {};
        domes::infra::NvsConfig nvsConfig;
        uint8_t podId = 0;
        if (nvsConfig.open(domes::infra::nvs_ns::kConfig) == ESP_OK) {
            podId = nvsConfig.getOrDefault<uint8_t>(domes::infra::config_key::kPodId, 0);
            nvsConfig.close();
        }

        if (podId > 0) {
            snprintf(bleName, sizeof(bleName), "DOMES-Pod-%02u", podId);
        } else {
            // Fall back to last 2 bytes of Bluetooth MAC address
            uint8_t mac[6] = {};
            esp_read_mac(mac, ESP_MAC_BT);
            snprintf(bleName, sizeof(bleName), "DOMES-Pod-%02X%02X", mac[4], mac[5]);
        }

        rc = ble_svc_gap_device_name_set(bleName);
        if (rc != 0) {
            ESP_LOGW(kTag, "Failed to set device name: %d", rc);
        } else {
            ESP_LOGI(kTag, "BLE device name: %s", bleName);
        }
    }

    // NOTE: g_statusCharHandle is populated by NimBLE after host task syncs,
    // so we can't copy it here. We'll read it directly in send().
    // statusCharHandle_ will be set after sync in bleSyncCb.

    // Set initialized BEFORE starting host task so sync callback can advertise
    initialized_ = true;

    // Start NimBLE host task - this will trigger sync callback which starts advertising
    nimble_port_freertos_init(bleHostTask);

    ESP_LOGI(kTag, "BLE OTA service initialized");

    return TransportError::kOk;
}

TransportError BleOtaService::send(const uint8_t* data, size_t len) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (!connected_) {
        return TransportError::kDisconnected;
    }

    if (data == nullptr || len == 0) {
        return TransportError::kInvalidArg;
    }

    // Use global status char handle (populated by NimBLE after sync)
    uint16_t charHandle = g_statusCharHandle;

    if (charHandle == 0) {
        ESP_LOGE(kTag, "Status characteristic handle not initialized!");
        return TransportError::kNotInitialized;
    }

    // Send via notification on status characteristic
    struct os_mbuf* om = ble_hs_mbuf_from_flat(data, len);
    if (om == nullptr) {
        ESP_LOGE(kTag, "Failed to allocate mbuf for notification");
        return TransportError::kIoError;
    }

    int rc = ble_gatts_notify_custom(connHandle_, charHandle, om);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to send notification: %d", rc);
        return TransportError::kIoError;
    }

    ESP_LOGD(kTag, "Sent %zu bytes via notification", len);
    return TransportError::kOk;
}

TransportError BleOtaService::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (buf == nullptr || len == nullptr || *len == 0) {
        return TransportError::kInvalidArg;
    }

    // Wait for data with timeout
    TickType_t ticks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(rxSemaphore_), ticks) != pdTRUE) {
        *len = 0;
        return TransportError::kTimeout;
    }

    // Copy data from ring buffer
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(rxMutex_), portMAX_DELAY);

    size_t available = 0;
    if (rxHead_ >= rxTail_) {
        available = rxHead_ - rxTail_;
    } else {
        available = kRxBufferSize - rxTail_ + rxHead_;
    }

    size_t toCopy = (*len < available) ? *len : available;
    for (size_t i = 0; i < toCopy; i++) {
        buf[i] = rxBuffer_[rxTail_];
        rxTail_ = (rxTail_ + 1) % kRxBufferSize;
    }

    // If there's still data, re-signal the semaphore
    if (rxHead_ != rxTail_) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(rxSemaphore_));
    }

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(rxMutex_));

    *len = toCopy;
    return TransportError::kOk;
}

bool BleOtaService::isConnected() const {
    return initialized_ && connected_;
}

void BleOtaService::disconnect() {
    if (connected_ && connHandle_ != 0) {
        ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
    }
    connected_ = false;
    connHandle_ = 0;
}

void BleOtaService::startAdvertising() {
    bool isInit = initialized_.load();
    ESP_LOGI(kTag, "startAdvertising() called, initialized_=%d", isInit);

    if (!isInit) {
        ESP_LOGW(kTag, "Not initialized, skipping advertising");
        return;
    }

    struct ble_gap_adv_params advParams = {};
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    advParams.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    advParams.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    // Build advertising data (must fit in 31 bytes)
    // Flags (3) + Name (11) = 14 bytes - fits easily
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char* name = ble_svc_gap_device_name();
    ESP_LOGI(kTag, "Device name: %s (len=%d)", name, (int)strlen(name));
    fields.name = reinterpret_cast<const uint8_t*>(name);
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    ESP_LOGI(kTag, "ble_gap_adv_set_fields rc=%d", rc);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to set adv fields: %d", rc);
        return;
    }

    // Put the 128-bit service UUID in scan response data (separate 31 bytes)
    struct ble_hs_adv_fields rspFields = {};
    rspFields.uuids128 = &kOtaServiceUuid128;
    rspFields.num_uuids128 = 1;
    rspFields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rspFields);
    ESP_LOGI(kTag, "ble_gap_adv_rsp_set_fields rc=%d", rc);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to set scan response: %d", rc);
        return;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &advParams, bleGapEventCb,
                           nullptr);
    ESP_LOGI(kTag, "ble_gap_adv_start rc=%d", rc);
    if (rc != 0) {
        ESP_LOGE(kTag, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(kTag, "BLE advertising started successfully!");
    }
}

void BleOtaService::stopAdvertising() {
    ble_gap_adv_stop();
}

void BleOtaService::setDeviceName(const char* name) {
    ble_svc_gap_device_name_set(name);
}

void BleOtaService::onDataReceived(const uint8_t* data, size_t len) {
    ESP_LOGD(kTag, "Received %zu bytes via BLE", len);

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(rxMutex_), portMAX_DELAY);

    // Copy to ring buffer
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead_ + 1) % kRxBufferSize;
        if (nextHead == rxTail_) {
            // Buffer full, drop oldest byte
            rxTail_ = (rxTail_ + 1) % kRxBufferSize;
        }
        rxBuffer_[rxHead_] = data[i];
        rxHead_ = nextHead;
    }

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(rxMutex_));

    // Signal data available
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(rxSemaphore_));
}

void BleOtaService::onConnectionStateChanged(bool connected, uint16_t connHandle) {
    connected_ = connected;
    connHandle_ = connHandle;

    if (!connected) {
        // Clear receive buffer on disconnect
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(rxMutex_), portMAX_DELAY);
        rxHead_ = 0;
        rxTail_ = 0;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(rxMutex_));
    }
}

void BleOtaService::onMtuChanged(uint16_t mtu) {
    currentMtu_ = mtu;
}

}  // namespace domes
