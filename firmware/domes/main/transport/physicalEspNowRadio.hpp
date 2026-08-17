#pragma once

#include "esp_now.h"
#include "iEspNowRadio.hpp"

#include <atomic>
#include <cstring>

namespace domes {

/** Production adapter for the ESP-IDF esp_now implementation. */
class PhysicalEspNowRadio final : public IEspNowRadio {
public:
    PhysicalEspNowRadio() = default;
    ~PhysicalEspNowRadio() override { deinit(); }

    PhysicalEspNowRadio(const PhysicalEspNowRadio&) = delete;
    PhysicalEspNowRadio& operator=(const PhysicalEspNowRadio&) = delete;

    EspNowRadioResult init(void* context, ReceiveCallback receiveCallback,
                           SendCallback sendCallback) override {
        if (!receiveCallback || !sendCallback || initialized_.load(std::memory_order_acquire)) {
            return EspNowRadioResult::kError;
        }
        PhysicalEspNowRadio* expected = nullptr;
        if (!activeRadio_.compare_exchange_strong(expected, this, std::memory_order_acq_rel)) {
            return EspNowRadioResult::kError;
        }
        callbackContext_ = context;
        receiveCallback_ = receiveCallback;
        sendCallback_ = sendCallback;

        esp_err_t result = esp_now_init();
        if (result != ESP_OK) {
            activeRadio_.store(nullptr, std::memory_order_release);
            return EspNowRadioResult::kError;
        }
        result = esp_now_register_recv_cb(vendorReceiveCallback);
        if (result == ESP_OK) {
            result = esp_now_register_send_cb(vendorSendCallback);
        }
        if (result != ESP_OK) {
            esp_now_deinit();
            activeRadio_.store(nullptr, std::memory_order_release);
            return EspNowRadioResult::kError;
        }
        initialized_.store(true, std::memory_order_release);
        return EspNowRadioResult::kOk;
    }

    void deinit() override {
        PhysicalEspNowRadio* expected = this;
        activeRadio_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
        if (initialized_.exchange(false, std::memory_order_acq_rel)) {
            esp_now_deinit();
        }
        pendingTxToken_.store(0, std::memory_order_release);
    }

    EspNowRadioResult addPeer(const EspNowAddress& address) override {
        esp_now_peer_info_t peer{};
        std::memcpy(peer.peer_addr, address.data(), address.size());
        peer.channel = 0;
        peer.encrypt = false;
        return mapResult(esp_now_add_peer(&peer));
    }

    EspNowRadioResult removePeer(const EspNowAddress& address) override {
        return mapResult(esp_now_del_peer(address.data()));
    }

    EspNowRadioResult getPeerCounts(EspNowPeerCounts& counts) const override {
        esp_now_peer_num_t vendorCounts{};
        const esp_err_t result = esp_now_get_peer_num(&vendorCounts);
        counts.total = static_cast<uint8_t>(vendorCounts.total_num);
        return mapResult(result);
    }

    bool peerExists(const EspNowAddress& address) const override {
        return esp_now_is_peer_exist(address.data());
    }

    EspNowRadioResult send(const EspNowAddress& destination, const uint8_t* data, size_t len,
                           EspNowCorrelationToken token) override {
        EspNowCorrelationToken expected = 0;
        if (token == 0 ||
            !pendingTxToken_.compare_exchange_strong(expected, token, std::memory_order_acq_rel)) {
            return EspNowRadioResult::kError;
        }
        const esp_err_t result = esp_now_send(destination.data(), data, len);
        if (result != ESP_OK) {
            expected = token;
            pendingTxToken_.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
        }
        return mapResult(result);
    }

private:
    static EspNowRadioResult mapResult(esp_err_t result) {
        if (result == ESP_OK) {
            return EspNowRadioResult::kOk;
        }
        if (result == ESP_ERR_ESPNOW_EXIST) {
            return EspNowRadioResult::kAlreadyExists;
        }
        if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
            return EspNowRadioResult::kNotFound;
        }
        return EspNowRadioResult::kError;
    }

    static EspNowCorrelationToken nextToken(std::atomic<EspNowCorrelationToken>& counter) {
        EspNowCorrelationToken token = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        if (token == 0) {
            token = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        return token;
    }

    static void vendorReceiveCallback(const esp_now_recv_info_t* info, const uint8_t* data,
                                      int len) {
        PhysicalEspNowRadio* radio = activeRadio_.load(std::memory_order_acquire);
        if (!radio || !data || len <= 0 || !radio->initialized_.load(std::memory_order_acquire)) {
            return;
        }
        EspNowReceiveMetadata metadata{};
        if (info && info->src_addr) {
            std::memcpy(metadata.source.data(), info->src_addr, metadata.source.size());
            metadata.sourceValid = true;
        }
        if (info && info->rx_ctrl) {
            metadata.rssi = info->rx_ctrl->rssi;
            metadata.rssiValid = true;
        }
        // Vendor metadata is valid only during this callback. The transport synchronously
        // copies it and the payload into its bounded queue.
        radio->receiveCallback_(radio->callbackContext_, nextToken(radio->rxToken_), metadata, data,
                                static_cast<size_t>(len));
    }

    static void vendorSendCallback(const uint8_t* macAddress, esp_now_send_status_t status) {
        PhysicalEspNowRadio* radio = activeRadio_.load(std::memory_order_acquire);
        if (!radio || !macAddress) {
            return;
        }
        const EspNowCorrelationToken token =
            radio->pendingTxToken_.exchange(0, std::memory_order_acq_rel);
        if (!radio->initialized_.load(std::memory_order_acquire) || token == 0) {
            return;
        }
        EspNowAddress destination{};
        std::memcpy(destination.data(), macAddress, destination.size());
        radio->sendCallback_(radio->callbackContext_, token, destination,
                             status == ESP_NOW_SEND_SUCCESS ? EspNowRadioSendStatus::kSuccess
                                                            : EspNowRadioSendStatus::kFailure);
    }

    inline static std::atomic<PhysicalEspNowRadio*> activeRadio_{nullptr};
    void* callbackContext_ = nullptr;
    ReceiveCallback receiveCallback_ = nullptr;
    SendCallback sendCallback_ = nullptr;
    std::atomic<EspNowCorrelationToken> rxToken_{0};
    std::atomic<EspNowCorrelationToken> pendingTxToken_{0};
    std::atomic<bool> initialized_{false};
};

}  // namespace domes
