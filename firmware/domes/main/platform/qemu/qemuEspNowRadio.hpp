#pragma once

#if !defined(DOMES_RUNTIME_PROFILE_QEMU)
#error "QemuEspNowRadio is available only in the isolated QEMU image"
#endif

#include "platform/qemu/qemuLinkAbi.hpp"
#include "transport/iEspNowRadio.hpp"

#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <array>
#include <atomic>

namespace domes::platform {

class QemuEspNowRadio final : public IEspNowRadio {
public:
    QemuEspNowRadio() = default;
    ~QemuEspNowRadio() override { deinit(); }

    EspNowRadioResult init(void* context, ReceiveCallback receiveCallback,
                           SendCallback sendCallback) override;
    void deinit() override;
    EspNowRadioResult addPeer(const EspNowAddress& address) override;
    EspNowRadioResult removePeer(const EspNowAddress& address) override;
    EspNowRadioResult getPeerCounts(EspNowPeerCounts& counts) const override;
    bool peerExists(const EspNowAddress& address) const override;
    EspNowRadioResult send(const EspNowAddress& destination, const uint8_t* data, size_t len,
                           EspNowCorrelationToken token) override;
    TaskHandle_t taskHandle() const { return task_; }

private:
    enum class EventKind : uint8_t { kTxComplete, kReceive };
    struct DeferredEvent {
        EventKind kind = EventKind::kTxComplete;
        EspNowCorrelationToken token = 0;
        EspNowAddress address{};
        std::array<uint8_t, kEspNowMaxPayload> payload{};
        uint16_t length = 0;
        int8_t rssi = 0;
        EspNowRadioSendStatus status = EspNowRadioSendStatus::kFailure;
    };

    static void IRAM_ATTR interruptHandler(void* context);
    static void taskEntry(void* context);
    void runTask();
    static uint32_t IRAM_ATTR read(qemu_link::Register reg);
    static void IRAM_ATTR write(qemu_link::Register reg, uint32_t value);
    static void IRAM_ATTR unpackAddress(uint32_t low, uint32_t high, EspNowAddress& address);
    static void packAddress(const EspNowAddress& address, uint32_t& low, uint32_t& high);

    void* callbackContext_ = nullptr;
    ReceiveCallback receiveCallback_ = nullptr;
    SendCallback sendCallback_ = nullptr;
    QueueHandle_t eventQueue_ = nullptr;
    StaticQueue_t eventQueueStorage_{};
    std::array<uint8_t, sizeof(DeferredEvent) * 4> eventQueueBytes_{};
    TaskHandle_t task_ = nullptr;
    StaticTask_t taskStorage_{};
    std::array<StackType_t, 4096> taskStack_{};
    intr_handle_t interrupt_ = nullptr;
    std::array<EspNowAddress, 9> peers_{};
    size_t peerCount_ = 0;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> handoffFailed_{false};
};

}  // namespace domes::platform
