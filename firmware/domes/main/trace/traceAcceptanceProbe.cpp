#include "traceAcceptanceProbe.hpp"

#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "kernelTrace.hpp"
#include "traceRecorder.hpp"

namespace {

constexpr uint32_t kQueueId = 1;
constexpr uint32_t kSemaphoreId = 2;
constexpr uint32_t kInterruptId = 3;
constexpr uint32_t kCallbackId = 4;
constexpr uint32_t kActionId = 5;
constexpr uint32_t kTimeoutId = 6;
constexpr uint32_t kCausalId = 1;
constexpr uint64_t kAlarmUs = 2000;
constexpr uint32_t kOverheadSamples = 32;

struct ProbeContext {
    QueueHandle_t queue = nullptr;
    SemaphoreHandle_t semaphore = nullptr;
};

DRAM_ATTR ProbeContext gProbeContext;
DRAM_ATTR uint8_t gCallbackIdentity;
DRAM_ATTR uint8_t gActionIdentity;
DRAM_ATTR uint8_t gTimeoutIdentity;

void IRAM_ATTR record(domes::trace::EventType type, uint16_t taskId, uint32_t arg1, uint32_t arg2,
                      bool isr = false, bool callback = false) {
    domes::trace::KernelTrace::recordFromIsr(
        domes::trace::KernelTrace::makeKernelEvent(type, taskId, arg1, arg2, isr, callback));
}

bool IRAM_ATTR onTraceAlarm(gptimer_handle_t, const gptimer_alarm_event_data_t*, void*) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    const uint32_t causalId = kCausalId;
    record(domes::trace::EventType::kSchedIsrEnter, 0, kInterruptId, causalId, true);
    record(domes::trace::EventType::kCallbackBegin, 0, kCallbackId, causalId, true, true);
    const bool queued =
        xQueueSendFromISR(gProbeContext.queue, &causalId, &higherPriorityTaskWoken) == pdTRUE;
    if (queued) {
        record(domes::trace::EventType::kSchedQueueSend, 0, kQueueId, causalId, true);
    }
    const bool signaled =
        xSemaphoreGiveFromISR(gProbeContext.semaphore, &higherPriorityTaskWoken) == pdTRUE;
    if (signaled) {
        record(domes::trace::EventType::kSemGive, 0, kSemaphoreId, causalId, true);
    }
    record(domes::trace::EventType::kCallbackEnd, 0, kCallbackId, causalId, true, true);
    record(domes::trace::EventType::kSchedIsrExit, 0, kInterruptId, causalId, true);
    return queued && signaled && higherPriorityTaskWoken == pdTRUE;
}

uint32_t measureRecordLoop() {
    const int64_t start = esp_timer_get_time();
    for (uint32_t index = 0; index < kOverheadSamples; ++index) {
        domes::trace::Recorder::record(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kCounter,
            static_cast<uint16_t>(uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle())), kActionId,
            index));
    }
    const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() - start);
    return elapsed;
}

}  // namespace

namespace domes::trace {

TraceAcceptanceResult runTraceAcceptanceProbe() {
    TraceAcceptanceResult result{};
    result.causalId = kCausalId;
    const void* leaseOwner = &gProbeContext;
    if (!Recorder::isInitialized() || !Recorder::acquireSessionLease(leaseOwner)) {
        return result;
    }

    result.disabledRecordUs = measureRecordLoop();
    gProbeContext.queue = xQueueCreate(1, sizeof(uint32_t));
    gProbeContext.semaphore = xSemaphoreCreateBinary();
    gptimer_handle_t timer = nullptr;
    const gptimer_config_t timerConfig = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
        .intr_priority = 0,
        .flags = {},
    };
    if (gProbeContext.queue == nullptr || gProbeContext.semaphore == nullptr ||
        gptimer_new_timer(&timerConfig, &timer) != ESP_OK) {
        if (timer != nullptr) {
            gptimer_del_timer(timer);
        }
        if (gProbeContext.queue != nullptr) {
            vQueueDelete(gProbeContext.queue);
        }
        if (gProbeContext.semaphore != nullptr) {
            vSemaphoreDelete(gProbeContext.semaphore);
        }
        gProbeContext = {};
        Recorder::releaseSessionLease(leaseOwner);
        return result;
    }

    const bool mapped =
        KernelTrace::registerObject(gProbeContext.queue, kQueueId, ObjectKind::kQueue,
                                    "probe_queue") &&
        KernelTrace::registerObject(gProbeContext.semaphore, kSemaphoreId, ObjectKind::kSemaphore,
                                    "probe_sem") &&
        KernelTrace::registerObject(timer, kInterruptId, ObjectKind::kInterrupt, "probe_irq") &&
        KernelTrace::registerObject(&gCallbackIdentity, kCallbackId, ObjectKind::kCallback,
                                    "probe_callback") &&
        KernelTrace::registerObject(&gActionIdentity, kActionId, ObjectKind::kAction,
                                    "probe_action") &&
        KernelTrace::registerObject(&gTimeoutIdentity, kTimeoutId, ObjectKind::kTimeout,
                                    "probe_timeout");
    const gptimer_event_callbacks_t callbacks = {.on_alarm = onTraceAlarm};
    const gptimer_alarm_config_t alarmConfig = {
        .alarm_count = kAlarmUs,
        .reload_count = 0,
        .flags = {},
    };
    bool timerEnabled = false;
    bool timerStarted = false;
    bool passed = mapped && gptimer_register_event_callbacks(timer, &callbacks, nullptr) == ESP_OK;
    if (passed) {
        passed = gptimer_enable(timer) == ESP_OK;
        timerEnabled = passed;
    }
    if (passed) {
        passed = gptimer_set_alarm_action(timer, &alarmConfig) == ESP_OK;
    }

    const bool traceStarted = Recorder::setEnabledForLease(true, leaseOwner);
    passed = traceStarted && passed;
    result.enabledRecordUs = measureRecordLoop();
    uint32_t receivedCausalId = 0;
    if (xQueueReceive(gProbeContext.queue, &receivedCausalId, 1) != pdFALSE) {
        passed = false;
    } else {
        record(EventType::kSchedTimeout,
               static_cast<uint16_t>(uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle())), kTimeoutId,
               kCausalId);
    }
    if (passed) {
        passed = gptimer_start(timer) == ESP_OK;
        timerStarted = passed;
    }
    if (passed) {
        passed =
            xQueueReceive(gProbeContext.queue, &receivedCausalId, pdMS_TO_TICKS(100)) == pdTRUE &&
            receivedCausalId == kCausalId;
    }
    const uint16_t taskId = static_cast<uint16_t>(uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle()));
    if (passed) {
        record(EventType::kSchedQueueReceive, taskId, kQueueId, kCausalId);
        passed = xSemaphoreTake(gProbeContext.semaphore, 0) == pdTRUE;
        if (passed) {
            record(EventType::kSemTake, taskId, kSemaphoreId, kCausalId);
        }
    }
    if (passed) {
        record(EventType::kCausalComplete, taskId, kActionId, kCausalId);
    }
    record(EventType::kTraceOverhead, taskId, result.disabledRecordUs, result.enabledRecordUs);
    if (timerStarted) {
        passed = gptimer_stop(timer) == ESP_OK && passed;
    }
    if (timerEnabled) {
        passed = gptimer_disable(timer) == ESP_OK && passed;
    }
    passed = Recorder::setEnabledForLease(false, leaseOwner) && passed;
    result.eventCount = Recorder::eventCount();
    result.droppedCount = Recorder::droppedCount();
    result.discontinuityCount = Recorder::discontinuityCount();
    KernelTrace::unregisterObject(gProbeContext.queue);
    KernelTrace::unregisterObject(gProbeContext.semaphore);
    KernelTrace::unregisterObject(timer);
    KernelTrace::unregisterObject(&gCallbackIdentity);
    KernelTrace::unregisterObject(&gActionIdentity);
    KernelTrace::unregisterObject(&gTimeoutIdentity);
    passed = gptimer_del_timer(timer) == ESP_OK && passed;
    vQueueDelete(gProbeContext.queue);
    vSemaphoreDelete(gProbeContext.semaphore);
    gProbeContext = {};
    passed = Recorder::releaseSessionLease(leaseOwner) && passed;
    result.passed = passed && result.eventCount > 0 && result.droppedCount == 0 &&
                    result.discontinuityCount == 0;
    return result;
}

}  // namespace domes::trace
