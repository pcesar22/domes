#pragma once

#include "esp_err.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#define ESP_NOW_ETH_ALEN 6
#define ESP_ERR_ESPNOW_EXIST 0x3064
#define ESP_ERR_ESPNOW_NOT_FOUND 0x3065

typedef enum {
    ESP_NOW_SEND_SUCCESS = 0,
    ESP_NOW_SEND_FAIL,
} esp_now_send_status_t;

struct wifi_pkt_rx_ctrl_t {
    int8_t rssi = 0;
};

struct esp_now_recv_info_t {
    const uint8_t* src_addr = nullptr;
    const uint8_t* des_addr = nullptr;
    wifi_pkt_rx_ctrl_t* rx_ctrl = nullptr;
};

struct esp_now_peer_info_t {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN]{};
    uint8_t channel = 0;
    bool encrypt = false;
};

struct esp_now_peer_num_t {
    int total_num = 0;
    int encrypt_num = 0;
};

using esp_now_recv_cb_t = void (*)(const esp_now_recv_info_t*, const uint8_t*, int);
using esp_now_send_cb_t = void (*)(const uint8_t*, esp_now_send_status_t);

namespace esp_now_test_stub {
inline esp_err_t initResult = ESP_OK;
inline esp_err_t sendResult = ESP_OK;
inline esp_now_recv_cb_t receiveCallback = nullptr;
inline esp_now_send_cb_t sendCallback = nullptr;
inline std::vector<std::array<uint8_t, ESP_NOW_ETH_ALEN>> peers;

inline void reset() {
    initResult = ESP_OK;
    sendResult = ESP_OK;
    receiveCallback = nullptr;
    sendCallback = nullptr;
    peers.clear();
}
}  // namespace esp_now_test_stub

inline esp_err_t esp_now_init() {
    return esp_now_test_stub::initResult;
}
inline esp_err_t esp_now_deinit() {
    return ESP_OK;
}
inline esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback) {
    esp_now_test_stub::receiveCallback = callback;
    return ESP_OK;
}
inline esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback) {
    esp_now_test_stub::sendCallback = callback;
    return ESP_OK;
}
inline esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
    std::array<uint8_t, ESP_NOW_ETH_ALEN> address{};
    std::memcpy(address.data(), peer->peer_addr, address.size());
    for (const auto& existing : esp_now_test_stub::peers) {
        if (existing == address) {
            return ESP_ERR_ESPNOW_EXIST;
        }
    }
    esp_now_test_stub::peers.push_back(address);
    return ESP_OK;
}
inline esp_err_t esp_now_del_peer(const uint8_t* address) {
    for (auto it = esp_now_test_stub::peers.begin(); it != esp_now_test_stub::peers.end(); ++it) {
        if (std::memcmp(it->data(), address, it->size()) == 0) {
            esp_now_test_stub::peers.erase(it);
            return ESP_OK;
        }
    }
    return ESP_ERR_ESPNOW_NOT_FOUND;
}
inline esp_err_t esp_now_get_peer_num(esp_now_peer_num_t* counts) {
    counts->total_num = static_cast<int>(esp_now_test_stub::peers.size());
    return ESP_OK;
}
inline bool esp_now_is_peer_exist(const uint8_t* address) {
    for (const auto& peer : esp_now_test_stub::peers) {
        if (std::memcmp(peer.data(), address, peer.size()) == 0) {
            return true;
        }
    }
    return false;
}
inline esp_err_t esp_now_send(const uint8_t*, const uint8_t*, size_t) {
    return esp_now_test_stub::sendResult;
}
