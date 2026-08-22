#include "qemuEspNowRadio.hpp"

#include "esp_attr.h"
#include "trace/kernelTrace.hpp"
#include "trace/traceApi.hpp"

#include <algorithm>
#include <cstring>

namespace domes::platform {
namespace {

constexpr uint32_t kSubmit = 1U;
constexpr uint32_t kConsume = 1U;
constexpr UBaseType_t kRadioTaskPriority = 23;
constexpr BaseType_t kRadioTaskCore = 0;

volatile uint32_t* IRAM_ATTR mmio(qemu_link::Register reg) {
    return reinterpret_cast<volatile uint32_t*>(qemu_link::address(reg));
}

}  // namespace

uint32_t IRAM_ATTR QemuEspNowRadio::read(qemu_link::Register reg) {
    return *mmio(reg);
}

void IRAM_ATTR QemuEspNowRadio::write(qemu_link::Register reg, uint32_t value) {
    *mmio(reg) = value;
}

void QemuEspNowRadio::packAddress(const EspNowAddress& address, uint32_t& low, uint32_t& high) {
    low = static_cast<uint32_t>(address[0]) | (static_cast<uint32_t>(address[1]) << 8U) |
          (static_cast<uint32_t>(address[2]) << 16U) |
          (static_cast<uint32_t>(address[3]) << 24U);
    high = static_cast<uint32_t>(address[4]) | (static_cast<uint32_t>(address[5]) << 8U);
}

void IRAM_ATTR QemuEspNowRadio::unpackAddress(uint32_t low, uint32_t high,
                                              EspNowAddress& address) {
    address = {static_cast<uint8_t>(low), static_cast<uint8_t>(low >> 8U),
               static_cast<uint8_t>(low >> 16U), static_cast<uint8_t>(low >> 24U),
               static_cast<uint8_t>(high), static_cast<uint8_t>(high >> 8U)};
}

EspNowRadioResult QemuEspNowRadio::init(void* context, ReceiveCallback receiveCallback,
                                        SendCallback sendCallback) {
    if (!receiveCallback || !sendCallback || initialized_.load(std::memory_order_acquire)) {
        return EspNowRadioResult::kError;
    }
    if (read(qemu_link::Register::kCapability) != qemu_link::kCapabilityMagic ||
        read(qemu_link::Register::kVersion) != qemu_link::kAbiVersion ||
        read(qemu_link::Register::kMaxPayload) != kEspNowMaxPayload) {
        return EspNowRadioResult::kError;
    }
    callbackContext_ = context;
    receiveCallback_ = receiveCallback;
    sendCallback_ = sendCallback;
    eventQueue_ = xQueueCreateStatic(4, sizeof(DeferredEvent), eventQueueBytes_.data(),
                                     &eventQueueStorage_);
    if (!eventQueue_) {
        return EspNowRadioResult::kError;
    }
    task_ = xTaskCreateStaticPinnedToCore(taskEntry, "qemu_radio", taskStack_.size(), this,
                                         kRadioTaskPriority, taskStack_.data(), &taskStorage_,
                                         kRadioTaskCore);
    if (!task_) {
        eventQueue_ = nullptr;
        return EspNowRadioResult::kError;
    }
    if (esp_intr_alloc(qemu_link::kInterruptSource, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM,
                       interruptHandler, this, &interrupt_) != ESP_OK) {
        vTaskDelete(task_);
        task_ = nullptr;
        eventQueue_ = nullptr;
        return EspNowRadioResult::kError;
    }
    initialized_.store(true, std::memory_order_release);
    handoffFailed_.store(false, std::memory_order_release);
    write(qemu_link::Register::kInterruptAck,
          qemu_link::kInterruptTxComplete | qemu_link::kInterruptRxReady);
    write(qemu_link::Register::kInterruptMask,
          qemu_link::kInterruptTxComplete | qemu_link::kInterruptRxReady);
    return EspNowRadioResult::kOk;
}

void QemuEspNowRadio::deinit() {
    initialized_.store(false, std::memory_order_release);
    write(qemu_link::Register::kInterruptMask, 0);
    if (interrupt_) {
        esp_intr_free(interrupt_);
        interrupt_ = nullptr;
    }
    if (task_) {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    eventQueue_ = nullptr;
    peerCount_ = 0;
    callbackContext_ = nullptr;
    receiveCallback_ = nullptr;
    sendCallback_ = nullptr;
}

EspNowRadioResult QemuEspNowRadio::addPeer(const EspNowAddress& address) {
    if (peerExists(address)) {
        return EspNowRadioResult::kAlreadyExists;
    }
    if (peerCount_ == peers_.size()) {
        return EspNowRadioResult::kError;
    }
    peers_[peerCount_++] = address;
    return EspNowRadioResult::kOk;
}

EspNowRadioResult QemuEspNowRadio::removePeer(const EspNowAddress& address) {
    const auto end = peers_.begin() + peerCount_;
    const auto found = std::find(peers_.begin(), end, address);
    if (found == end) {
        return EspNowRadioResult::kNotFound;
    }
    std::move(found + 1, end, found);
    --peerCount_;
    return EspNowRadioResult::kOk;
}

EspNowRadioResult QemuEspNowRadio::getPeerCounts(EspNowPeerCounts& counts) const {
    counts.total = static_cast<uint8_t>(peerCount_);
    return EspNowRadioResult::kOk;
}

bool QemuEspNowRadio::peerExists(const EspNowAddress& address) const {
    return std::find(peers_.begin(), peers_.begin() + peerCount_, address) !=
           peers_.begin() + peerCount_;
}

EspNowRadioResult QemuEspNowRadio::send(const EspNowAddress& destination, const uint8_t* data,
                                        size_t len, EspNowCorrelationToken token) {
    if (!initialized_.load(std::memory_order_acquire) ||
        handoffFailed_.load(std::memory_order_acquire) || !data || len == 0 ||
        len > kEspNowMaxPayload || token == 0 ||
        read(qemu_link::Register::kStickyStatus) != 0 ||
        read(qemu_link::Register::kTxStatus) ==
            static_cast<uint32_t>(qemu_link::TxStatus::kPending)) {
        return EspNowRadioResult::kError;
    }
    uint32_t low = 0;
    uint32_t high = 0;
    packAddress(destination, low, high);
    write(qemu_link::Register::kTxDestinationLow, low);
    write(qemu_link::Register::kTxDestinationHigh, high);
    auto* payload = reinterpret_cast<volatile uint32_t*>(
        qemu_link::address(qemu_link::Register::kTxPayload));
    for (size_t index = 0; index < (len + 3U) / 4U; ++index) {
        uint32_t word = 0;
        const size_t start = index * 4U;
        for (size_t byte = 0; byte < 4U && start + byte < len; ++byte) {
            word |= static_cast<uint32_t>(data[start + byte]) << (byte * 8U);
        }
        payload[index] = word;
    }
    write(qemu_link::Register::kTxLength, static_cast<uint32_t>(len));
    write(qemu_link::Register::kTxCorrelation, token);
    if (trace::Recorder::isEnabled()) {
        trace::Recorder::record(trace::makeEvent(trace::EventType::kSchedQueueSend,
                                                 trace::Category::kEspNow,
                                                 TRACE_ID("QemuLink.MmioSubmit"), token));
    }
    write(qemu_link::Register::kTxSubmit, kSubmit);
    const uint32_t status = read(qemu_link::Register::kTxStatus);
    return status == static_cast<uint32_t>(qemu_link::TxStatus::kPending) ||
                   status == static_cast<uint32_t>(qemu_link::TxStatus::kSuccess)
               ? EspNowRadioResult::kOk
               : EspNowRadioResult::kError;
}

void IRAM_ATTR QemuEspNowRadio::interruptHandler(void* context) {
    auto* radio = static_cast<QemuEspNowRadio*>(context);
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    const uint32_t status = read(qemu_link::Register::kInterruptStatus);
    const EspNowCorrelationToken isrToken =
        (status & qemu_link::kInterruptRxReady) != 0
            ? read(qemu_link::Register::kRxCorrelation)
            : read(qemu_link::Register::kTxCorrelation);
    trace::KernelTrace::recordFromIsr(trace::KernelTrace::makeKernelEvent(
        trace::EventType::kSchedIsrEnter, 0, qemu_link::kInterruptSource, isrToken, true));

    if ((status & qemu_link::kInterruptTxComplete) != 0) {
        DeferredEvent event{};
        event.kind = EventKind::kTxComplete;
        event.token = read(qemu_link::Register::kTxCorrelation);
        unpackAddress(read(qemu_link::Register::kTxDestinationLow),
                      read(qemu_link::Register::kTxDestinationHigh), event.address);
        event.status = read(qemu_link::Register::kTxStatus) ==
                               static_cast<uint32_t>(qemu_link::TxStatus::kSuccess)
                           ? EspNowRadioSendStatus::kSuccess
                           : EspNowRadioSendStatus::kFailure;
        if (xQueueSendFromISR(radio->eventQueue_, &event, &higherPriorityTaskWoken) != pdTRUE) {
            radio->handoffFailed_.store(true, std::memory_order_release);
        }
    }
    if ((status & qemu_link::kInterruptRxReady) != 0) {
        DeferredEvent event{};
        event.kind = EventKind::kReceive;
        event.token = read(qemu_link::Register::kRxCorrelation);
        unpackAddress(read(qemu_link::Register::kRxSourceLow),
                      read(qemu_link::Register::kRxSourceHigh), event.address);
        event.rssi = static_cast<int8_t>(read(qemu_link::Register::kRxRssi));
        event.length = static_cast<uint16_t>(read(qemu_link::Register::kRxLength));
        if (event.length <= kEspNowMaxPayload) {
            auto* payload = reinterpret_cast<volatile uint32_t*>(
                qemu_link::address(qemu_link::Register::kRxPayload));
            for (size_t index = 0; index < (event.length + 3U) / 4U; ++index) {
                const uint32_t word = payload[index];
                const size_t start = index * 4U;
                for (size_t byte = 0; byte < 4U && start + byte < event.length; ++byte) {
                    event.payload[start + byte] = static_cast<uint8_t>(word >> (byte * 8U));
                }
            }
            if (xQueueSendFromISR(radio->eventQueue_, &event, &higherPriorityTaskWoken) != pdTRUE) {
                radio->handoffFailed_.store(true, std::memory_order_release);
            }
        } else {
            radio->handoffFailed_.store(true, std::memory_order_release);
        }
        write(qemu_link::Register::kRxConsume, kConsume);
    }
    write(qemu_link::Register::kInterruptAck, status);
    if (radio->handoffFailed_.load(std::memory_order_acquire)) {
        write(qemu_link::Register::kInterruptMask, 0);
    }
    trace::KernelTrace::recordFromIsr(trace::KernelTrace::makeKernelEvent(
        trace::EventType::kSchedIsrExit, 0, qemu_link::kInterruptSource, isrToken, true));
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void QemuEspNowRadio::taskEntry(void* context) {
    static_cast<QemuEspNowRadio*>(context)->runTask();
}

void QemuEspNowRadio::runTask() {
    DeferredEvent event{};
    while (xQueueReceive(eventQueue_, &event, portMAX_DELAY) == pdTRUE) {
        if (!initialized_.load(std::memory_order_acquire) || event.token == 0) {
            continue;
        }
        if (trace::Recorder::isEnabled()) {
            trace::Recorder::record(trace::makeEvent(trace::EventType::kSchedQueueReceive,
                                                     trace::Category::kEspNow,
                                                     TRACE_ID("QemuLink.TaskHandoff"),
                                                     event.token));
        }
        if (event.kind == EventKind::kTxComplete) {
            sendCallback_(callbackContext_, event.token, event.address, event.status);
        } else {
            const EspNowReceiveMetadata metadata = {
                .source = event.address,
                .rssi = event.rssi,
                .sourceValid = true,
                .rssiValid = true,
            };
            receiveCallback_(callbackContext_, event.token, metadata, event.payload.data(),
                             event.length);
        }
    }
}

}  // namespace domes::platform
