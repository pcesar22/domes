# 04 - Communication

## AI Agent Instructions

Load this file when:
- Implementing ESP-NOW communication
- Implementing BLE services
- Defining protocol messages
- Handling RF coexistence

Prerequisites: `03-driver-development.md`

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Phone App                                │
│                   (iOS/Android)                              │
└──────────────────────┬──────────────────────────────────────┘
                       │ BLE (GATT)
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    Master Pod                                │
│              BLE Server + ESP-NOW Master                     │
└──────────┬────────────────┬─────────────┬───────────────────┘
           │ ESP-NOW        │ ESP-NOW     │ ESP-NOW
           ▼                ▼             ▼
     ┌─────────┐      ┌─────────┐   ┌─────────┐
     │  Pod 2  │      │  Pod 3  │   │  Pod N  │
     │ESP-NOW  │      │ESP-NOW  │   │ESP-NOW  │
     └─────────┘      └─────────┘   └─────────┘
```

**Key Points:**
- Phone connects to ONE pod via BLE (the "master")
- Master relays commands to other pods via ESP-NOW
- ESP-NOW provides <1ms latency between pods
- Any pod can be master (role is configurable)

---

## ESP-NOW Implementation

### ESP-NOW Configuration

```cpp
// services/comm_service.hpp
#pragma once

#include "esp_now.h"
#include "esp_err.h"
#include <functional>
#include <array>

namespace domes {

struct EspNowMessage {
    uint8_t type;
    uint8_t payload[ESP_NOW_MAX_DATA_LEN - 1];
    size_t payloadLen;
    uint8_t srcMac[6];
    int8_t rssi;
};

using EspNowCallback = std::function<void(const EspNowMessage&)>;

class CommService {
public:
    static CommService& instance();

    esp_err_t init();
    esp_err_t deinit();

    /**
     * @brief Add a peer device
     * @param macAddr 6-byte MAC address
     * @return ESP_OK on success
     */
    esp_err_t addPeer(const uint8_t* macAddr);

    /**
     * @brief Send message to a peer
     * @param macAddr Destination MAC (nullptr = broadcast)
     * @param data Message data
     * @param len Data length (max 250)
     * @return ESP_OK on success
     */
    esp_err_t send(const uint8_t* macAddr, const uint8_t* data, size_t len);

    /**
     * @brief Broadcast message to all peers
     */
    esp_err_t broadcast(const uint8_t* data, size_t len);

    /**
     * @brief Register receive callback
     */
    void onReceive(EspNowCallback callback);

    /**
     * @brief Get own MAC address
     */
    void getOwnMac(uint8_t* macOut);

private:
    CommService() = default;

    static void recvCallback(const esp_now_recv_info_t* info,
                             const uint8_t* data, int len);
    static void sendCallback(const uint8_t* macAddr,
                             esp_now_send_status_t status);

    EspNowCallback receiveCallback_;
    bool initialized_ = false;
};

}  // namespace domes
```

### ESP-NOW Initialization

```cpp
// services/comm_service.cpp
#include "comm_service.hpp"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

namespace domes {

namespace {
    constexpr const char* kTag = "comm";
    CommService* instance_ = nullptr;
}

CommService& CommService::instance() {
    static CommService inst;
    instance_ = &inst;
    return inst;
}

esp_err_t CommService::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    // Initialize WiFi in station mode (required for ESP-NOW)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), kTag, "WiFi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), kTag, "Set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "WiFi start failed");

    // Set channel (must match all pods)
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE),
                        kTag, "Set channel failed");

    // Initialize ESP-NOW
    ESP_RETURN_ON_ERROR(esp_now_init(), kTag, "ESP-NOW init failed");

    // Register callbacks
    esp_now_register_recv_cb(recvCallback);
    esp_now_register_send_cb(sendCallback);

    initialized_ = true;
    ESP_LOGI(kTag, "ESP-NOW initialized on channel 1");
    return ESP_OK;
}

esp_err_t CommService::addPeer(const uint8_t* macAddr) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, macAddr, 6);
    peer.channel = 1;
    peer.encrypt = false;

    if (esp_now_is_peer_exist(macAddr)) {
        return ESP_OK;  // Already added
    }

    return esp_now_add_peer(&peer);
}

esp_err_t CommService::send(const uint8_t* macAddr, const uint8_t* data, size_t len) {
    if (len > ESP_NOW_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    return esp_now_send(macAddr, data, len);
}

esp_err_t CommService::broadcast(const uint8_t* data, size_t len) {
    static const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return send(broadcastMac, data, len);
}

void CommService::onReceive(EspNowCallback callback) {
    receiveCallback_ = callback;
}

void CommService::recvCallback(const esp_now_recv_info_t* info,
                                const uint8_t* data, int len) {
    if (!instance_ || !instance_->receiveCallback_) return;

    EspNowMessage msg;
    msg.type = data[0];
    msg.payloadLen = len - 1;
    memcpy(msg.payload, data + 1, msg.payloadLen);
    memcpy(msg.srcMac, info->src_addr, 6);
    msg.rssi = info->rx_ctrl->rssi;

    instance_->receiveCallback_(msg);
}

void CommService::sendCallback(const uint8_t* macAddr,
                                esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(kTag, "Send failed to %02X:%02X:%02X:%02X:%02X:%02X",
                 macAddr[0], macAddr[1], macAddr[2],
                 macAddr[3], macAddr[4], macAddr[5]);
    }
}

void CommService::getOwnMac(uint8_t* macOut) {
    esp_wifi_get_mac(WIFI_IF_STA, macOut);
}

}  // namespace domes
```

---

## Protocol Messages

### Message Format

```cpp
// game/protocol.hpp
#pragma once

#include <cstdint>

namespace domes {

#pragma pack(push, 1)

struct MessageHeader {
    uint8_t version;        // Protocol version (1)
    uint8_t type;           // MessageType
    uint16_t sequence;      // For ordering/dedup
    uint32_t timestampUs;   // Sender's timestamp
};

enum class MessageType : uint8_t {
    // Master → Pod (0x01-0x0F)
    kSyncClock      = 0x01,
    kSetColor       = 0x02,
    kArmTouch       = 0x03,
    kPlaySound      = 0x04,
    kHapticPulse    = 0x05,
    kStopAll        = 0x06,

    // Pod → Master (0x10-0x1F)
    kTouchEvent     = 0x10,
    kTimeoutEvent   = 0x11,
    kStatusReport   = 0x12,
    kErrorReport    = 0x13,

    // System (0x20-0x2F)
    kPing           = 0x20,
    kPong           = 0x21,
    kOtaBegin       = 0x22,
    kOtaData        = 0x23,
    kOtaEnd         = 0x24,
    kReboot         = 0x2F,
};

// --- Control Messages (Master → Pod) ---

struct SetColorMsg {
    MessageHeader header;
    uint8_t r, g, b, w;
    uint16_t durationMs;
    uint8_t transition;  // 0=instant, 1=fade
};

struct ArmTouchMsg {
    MessageHeader header;
    uint16_t timeoutMs;
    uint8_t feedbackMode;  // 0=silent, 1=audio, 2=haptic, 3=all
};

struct PlaySoundMsg {
    MessageHeader header;
    uint8_t soundId;
    uint8_t volume;
};

struct HapticPulseMsg {
    MessageHeader header;
    uint8_t effectId;
    uint8_t intensity;
};

// --- Event Messages (Pod → Master) ---

struct TouchEventMsg {
    MessageHeader header;
    uint8_t podId;
    uint32_t reactionTimeUs;
    uint8_t touchStrength;
};

struct StatusReportMsg {
    MessageHeader header;
    uint8_t podId;
    uint8_t batteryPercent;
    int8_t rssi;
    uint8_t state;
};

// --- System Messages ---

struct PingMsg {
    MessageHeader header;
    uint32_t sendTimeUs;
};

struct PongMsg {
    MessageHeader header;
    uint32_t pingTimeUs;   // Echo back sender's time
    uint32_t pongTimeUs;   // Responder's time
};

#pragma pack(pop)

// Helper functions
inline MessageHeader makeHeader(MessageType type, uint16_t seq) {
    return {
        .version = 1,
        .type = static_cast<uint8_t>(type),
        .sequence = seq,
        .timestampUs = static_cast<uint32_t>(esp_timer_get_time()),
    };
}

}  // namespace domes
```

---

## BLE Implementation

### GATT Service Definition

```cpp
// services/ble_service.hpp
#pragma once

#include "esp_err.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include <functional>

namespace domes {

// Service UUIDs (128-bit custom)
// Base: xxxxxxxx-0000-1000-8000-00805F9B34FB
constexpr uint16_t kDomesServiceUuid        = 0x1234;
constexpr uint16_t kCharPodStatusUuid       = 0x1235;  // Read, Notify
constexpr uint16_t kCharDrillControlUuid    = 0x1236;  // Write
constexpr uint16_t kCharTouchEventUuid      = 0x1237;  // Notify
constexpr uint16_t kCharConfigUuid          = 0x1238;  // Read, Write

using BleConnectCallback = std::function<void(uint16_t connHandle)>;
using BleDisconnectCallback = std::function<void(uint16_t connHandle)>;
using BleWriteCallback = std::function<void(uint16_t charUuid, const uint8_t* data, size_t len)>;

class BleService {
public:
    static BleService& instance();

    esp_err_t init(const char* deviceName);
    esp_err_t startAdvertising();
    esp_err_t stopAdvertising();

    /**
     * @brief Send notification on characteristic
     */
    esp_err_t notify(uint16_t charUuid, const uint8_t* data, size_t len);

    /**
     * @brief Check if any device is connected
     */
    bool isConnected() const;

    void onConnect(BleConnectCallback cb);
    void onDisconnect(BleDisconnectCallback cb);
    void onWrite(BleWriteCallback cb);

private:
    BleService() = default;

    static int gapEventHandler(struct ble_gap_event* event, void* arg);
    static int gattAccessHandler(uint16_t connHandle, uint16_t attrHandle,
                                  struct ble_gatt_access_ctxt* ctxt, void* arg);

    uint16_t connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    BleConnectCallback connectCb_;
    BleDisconnectCallback disconnectCb_;
    BleWriteCallback writeCb_;
};

}  // namespace domes
```

### BLE Advertising

```cpp
// Simplified BLE init (in ble_service.cpp)
esp_err_t BleService::startAdvertising() {
    struct ble_gap_adv_params advParams = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN,
        .itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX,
    };

    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)"DOMES Pod";
    fields.name_len = strlen("DOMES Pod");
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    return ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr,
                             BLE_HS_FOREVER, &advParams,
                             gapEventHandler, nullptr) == 0
               ? ESP_OK : ESP_FAIL;
}
```

---

## RF Coexistence

### Configuration

```cpp
// In main.cpp or comm_service.cpp

esp_err_t initRfStacks() {
    // 1. Initialize WiFi first
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), kTag, "WiFi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), kTag, "WiFi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "WiFi start");

    // 2. Enable coexistence
    ESP_RETURN_ON_ERROR(esp_coex_preference_set(ESP_COEX_PREFER_BALANCE),
                        kTag, "Coex preference");

    // 3. Initialize ESP-NOW (uses WiFi)
    ESP_RETURN_ON_ERROR(esp_now_init(), kTag, "ESP-NOW init");

    // 4. Initialize BLE with NimBLE
    ESP_RETURN_ON_ERROR(nimble_port_init(), kTag, "NimBLE init");
    ble_hs_cfg.sync_cb = bleOnSync;
    nimble_port_freertos_init(bleHostTask);

    // 5. Configure BLE scan parameters to not starve WiFi
    // (done when starting scan, not here)

    return ESP_OK;
}
```

### Coexistence Best Practices

| Setting | Recommendation |
|---------|----------------|
| Coex mode | `ESP_COEX_PREFER_BALANCE` |
| BLE scan window | < scan interval (e.g., 0x10 / 0x80) |
| WiFi channel | Fixed channel 1 for ESP-NOW |
| BLE connection interval | 30-50ms for responsive, 100ms+ for power |

---

## Latency Measurement

### Ping-Pong Test

```cpp
esp_err_t measureLatency(const uint8_t* peerMac, uint32_t* avgLatencyUs) {
    constexpr int kIterations = 100;
    uint64_t totalUs = 0;

    for (int i = 0; i < kIterations; i++) {
        PingMsg ping;
        ping.header = makeHeader(MessageType::kPing, i);
        ping.sendTimeUs = esp_timer_get_time();

        // Send ping
        esp_err_t err = CommService::instance().send(
            peerMac, reinterpret_cast<uint8_t*>(&ping), sizeof(ping));
        if (err != ESP_OK) continue;

        // Wait for pong (with timeout)
        uint32_t startWait = esp_timer_get_time();
        while (!pongReceived && (esp_timer_get_time() - startWait) < 10000) {
            vTaskDelay(1);
        }

        if (pongReceived) {
            uint32_t rtt = pongTimestamp - ping.sendTimeUs;
            totalUs += rtt;
            pongReceived = false;
        }
    }

    *avgLatencyUs = totalUs / kIterations;
    ESP_LOGI(kTag, "Average RTT: %lu us", *avgLatencyUs);

    return ESP_OK;
}
```

### Pass Criteria

| Metric | Target | Measurement |
|--------|--------|-------------|
| P50 latency | < 1ms | Median of 1000 pings |
| P95 latency | < 2ms | 95th percentile |
| P99 latency | < 5ms | 99th percentile |
| Packet loss | < 1% | Count failures |

---

*Related: `03-driver-development.md`, `09-reference.md` (message types)*
