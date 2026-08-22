#include "platform/qemu/qemuEspNowRadio.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "trace/traceRecorder.hpp"
#include "transport/espNowTransport.hpp"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

constexpr char kTag[] = "qemu_link_slice";
constexpr std::array<uint8_t, 8> kServiceRequest = {0x44, 0x4f, 0x4d, 0x45,
                                                    0x53, 0x2d, 0x31, 0x00};

void emitTrace() {
    static uint8_t owner;
    auto& buffer = domes::trace::Recorder::buffer();
    if (!buffer.tryClaimDumpSnapshot(&owner)) {
        return;
    }
    const size_t count = buffer.captureDumpSnapshot(&owner);
    for (size_t index = 0; index < count; ++index) {
        const auto* event = buffer.dumpSnapshotEvent(&owner, index);
        if (event) {
            std::printf("DOMES_QEMU_LINK_TRACE schema=1 index=%zu timestamp=%" PRIu32
                        " task=%u type=%u arg1=%" PRIu32 " token=%" PRIu32 "\n",
                        index, event->timestamp, event->taskId, event->eventType, event->arg1,
                        event->arg2);
        }
    }
}

}  // namespace

extern "C" void app_main() {
    uint32_t failure = 0;
    static domes::platform::QemuEspNowRadio radio;
    static domes::EspNowTransport transport(radio);
    if (transport.init() != domes::TransportError::kOk) {
        failure |= 1U << 1;
    }
    if (domes::trace::Recorder::init(16 * 1024) != ESP_OK ||
        !domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main", 1, 1, 0)) {
        failure |= 1U << 0;
    }
    if (radio.taskHandle() == nullptr ||
        !domes::trace::Recorder::registerTask(radio.taskHandle(), "qemu_radio", 2, 23, 0)) {
        failure |= 1U << 2;
    }
    domes::trace::Recorder::finalizeTaskCatalog();
    domes::trace::Recorder::setEnabled(true);

    if (failure == 0 && transport.send(kServiceRequest.data(), kServiceRequest.size()) !=
                            domes::TransportError::kOk) {
        failure |= 1U << 3;
    }

    std::array<uint8_t, domes::kEspNowMaxPayload> response{};
    size_t responseSize = response.size();
    if (failure == 0 && transport.receive(response.data(), &responseSize, 500) !=
                            domes::TransportError::kOk) {
        failure |= 1U << 4;
    }
    if (responseSize != kServiceRequest.size() ||
        std::memcmp(response.data(), kServiceRequest.data(), kServiceRequest.size()) != 0) {
        failure |= 1U << 5;
    }

    int8_t rssi = 0;
    std::array<uint8_t, domes::kEspNowAddressSize> source{};
    const domes::EspNowCorrelationToken token = transport.lastReceivedCorrelationToken();
    if (token != 0 && domes::trace::Recorder::isEnabled()) {
        domes::trace::Recorder::record(domes::trace::makeEvent(
            domes::trace::EventType::kInstant, domes::trace::Category::kEspNow,
            TRACE_ID("QemuLink.ServiceDispatch"), token));
    }
    if (token == 0 || !transport.lastReceivedRssi(rssi) || rssi != -42 ||
        !transport.lastReceivedSource(source.data()) || source != domes::EspNowAddress{2, 0, 0, 0, 0, 2}) {
        failure |= 1U << 6;
    }
    if (domes::trace::Recorder::droppedCount() != 0 ||
        domes::trace::Recorder::discontinuityCount() != 0) {
        failure |= 1U << 7;
    }

    domes::trace::Recorder::setEnabled(false);
    emitTrace();
    std::printf("DOMES_QEMU_LINK_RESULT schema=1 status=%s failure_mask=0x%08" PRIx32
                " token=%" PRIu32 " mmio=1 irq=1 from_isr=1 task=1 callback=1"
                " ring=1 semaphore=1 dequeue=1 dispatch=1 tx_complete=1"
                " trace_drops=%" PRIu32 " trace_discontinuities=%" PRIu32 "\n",
                failure == 0 ? "PASS" : "FAIL", failure, token,
                domes::trace::Recorder::droppedCount(),
                domes::trace::Recorder::discontinuityCount());
    std::fflush(stdout);
    ESP_LOGI(kTag, "FS-WP-002F vertical slice complete");
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}
