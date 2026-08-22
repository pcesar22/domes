#include "config/featureManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/qemu/deterministicPlatformInputs.hpp"
#include "platform/qemu/qemuEspNowRadio.hpp"
#include "services/espNowProtocol.hpp"
#include "services/espNowService.hpp"
#include "trace/traceRecorder.hpp"
#include "transport/espNowTransport.hpp"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

constexpr char kTag[] = "qemu_link_slice";
constexpr domes::PlatformIdentity kLocalIdentity = {2, 0, 0, 0, 0, 1};
constexpr std::array<uint32_t, 1> kRandomInputs = {0x14100001U};
constexpr TickType_t kDispatchTimeout = pdMS_TO_TICKS(3000);

void serviceTask(void* context) {
    static_cast<domes::EspNowService*>(context)->run();
    vTaskDelete(nullptr);
}

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
    if (domes::trace::Recorder::init(16 * 1024) != ESP_OK ||
        !domes::trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "main", 1, 1, 0)) {
        failure |= 1U << 0;
    }
    if (transport.init() != domes::TransportError::kOk) {
        failure |= 1U << 1;
    }
    if (radio.taskHandle() == nullptr ||
        !domes::trace::Recorder::registerTask(radio.taskHandle(), "qemu_radio", 2, 23, 0)) {
        failure |= 1U << 2;
    }
    domes::trace::Recorder::finalizeTaskCatalog();
    domes::trace::Recorder::setEnabled(true);

    static domes::config::FeatureManager features(
        1U << static_cast<uint8_t>(domes::config::Feature::kEspNow));
    static domes::platform::FixedPlatformIdentity identity(kLocalIdentity);
    static domes::platform::RecordedRandomSource random(kRandomInputs);
    static domes::EspNowService service(transport, features, identity, random);
    static StaticTask_t serviceTaskStorage;
    static std::array<StackType_t, 8192> serviceTaskStack{};
    if (service.init() != ESP_OK ||
        xTaskCreateStaticPinnedToCore(serviceTask, "espnow_service", serviceTaskStack.size(),
                                      &service, 5, serviceTaskStack.data(), &serviceTaskStorage,
                                      0) == nullptr) {
        failure |= 1U << 3;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    const domes::espnow::MsgHeader peerBeacon = {
        .type = static_cast<uint8_t>(domes::espnow::kBeacon),
        .senderMac = {2, 0, 0, 0, 0, 2},
        .timestampUs = 141,
    };
    if (failure == 0 && transport.send(reinterpret_cast<const uint8_t*>(&peerBeacon),
                                       sizeof(peerBeacon)) != domes::TransportError::kOk) {
        failure |= 1U << 4;
    }

    const TickType_t dispatchStart = xTaskGetTickCount();
    while (service.peerCount() == 0 && xTaskGetTickCount() - dispatchStart < kDispatchTimeout) {
        vTaskDelay(1);
    }
    const uint32_t serviceDispatches = service.peerCount();
    if (serviceDispatches != 1) {
        failure |= 1U << 5;
    }

    int8_t rssi = 0;
    std::array<uint8_t, domes::kEspNowAddressSize> source{};
    const domes::EspNowCorrelationToken token = transport.lastReceivedCorrelationToken();
    if (token != 0 && serviceDispatches == 1 && domes::trace::Recorder::isEnabled()) {
        domes::trace::Recorder::record(domes::trace::makeEvent(
            domes::trace::EventType::kInstant, domes::trace::Category::kEspNow,
            TRACE_ID("QemuLink.ServiceDispatch"), token));
    }
    if (token == 0 || !transport.lastReceivedRssi(rssi) || rssi != -42 ||
        !transport.lastReceivedSource(source.data()) ||
        source != domes::EspNowAddress{2, 0, 0, 0, 0, 2}) {
        failure |= 1U << 6;
    }
    if (domes::trace::Recorder::droppedCount() != 0 ||
        domes::trace::Recorder::discontinuityCount() != 0) {
        failure |= 1U << 7;
    }

    service.requestStop();
    vTaskDelay(1);

    domes::trace::Recorder::setEnabled(false);
    emitTrace();
    std::printf("DOMES_QEMU_LINK_RESULT schema=2 status=%s failure_mask=0x%08" PRIx32
                " token=%" PRIu32 " service_dispatches=%" PRIu32 " trace_drops=%" PRIu32
                " trace_discontinuities=%" PRIu32 "\n",
                failure == 0 ? "PASS" : "FAIL", failure, token, serviceDispatches,
                domes::trace::Recorder::droppedCount(),
                domes::trace::Recorder::discontinuityCount());
    std::fflush(stdout);
    ESP_LOGI(kTag, "FS-WP-002F vertical slice complete");
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}
