#include "sdkconfig.h"

#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <cinttypes>
#include <cstdio>

#if defined(CONFIG_FREERTOS_SMP) && CONFIG_FREERTOS_SMP
#error "FS-WP-002B must use the production ESP-IDF FreeRTOS kernel, not Amazon SMP"
#endif

#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
#error "FS-WP-002B requires both ESP32-S3 target CPUs"
#endif

#if !defined(CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0) || !CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0
#error "FS-WP-002B requires app_main and GPTimer interrupt allocation on CPU0"
#endif

static_assert(CONFIG_IDF_TARGET_ESP32S3, "FS-WP-002B is an ESP32-S3 target-execution probe");
static_assert(CONFIG_FREERTOS_NUMBER_OF_CORES == 2,
              "FS-WP-002B requires the ESP32-S3 dual-core FreeRTOS configuration");
static_assert(CONFIG_FREERTOS_HZ == 1000,
              "FS-WP-002B must retain the production 1 kHz tick configuration");

struct ProbeState {
    uint32_t schema;
    uint32_t failureMask;
    int32_t controllerCore;
    int32_t core0Observed;
    int32_t core1Observed;
    uint32_t core0Runs;
    uint32_t core1Runs;
    uint32_t core0Phases;
    uint32_t core1Phases;
    uint32_t core0Blocks;
    uint32_t core1Blocks;
    uint32_t core0Wakeups;
    uint32_t core1Wakeups;
    uint32_t taskHandoff0To1;
    uint32_t taskHandoff1To0;
    uint32_t core0WaitTicks;
    uint32_t core1WaitTicks;
    uint32_t irqWaitTicks;
    uint32_t tickStart;
    uint32_t tickEnd;
    uint32_t tickDelta;
    int32_t irqCore;
    int32_t irqConsumerCore;
    uint32_t irqConsumerWakeups;
    uint32_t irqCount;
    uint32_t irqDrops;
    uint32_t irqSequence;
    uint64_t irqAlarm;
    uint64_t irqCountValue;
    uint64_t irqCountDelta;
    uint32_t timerCleanup;
};

extern "C" {
volatile ProbeState gProbeState = {};

__attribute__((noinline)) void domesQemuProbeComplete() {
    asm volatile("" ::: "memory");
}
}

namespace {

constexpr char kTag[] = "domes_qemu_probe";
constexpr EventBits_t kCore1Ready = BIT0;
constexpr EventBits_t kCore0Done = BIT1;
constexpr EventBits_t kCore1IrqReady = BIT2;
constexpr EventBits_t kCore1Done = BIT3;
constexpr TickType_t kCore1WakeDelayTicks = 4;
constexpr TickType_t kCore0WakeDelayTicks = 2;
constexpr TickType_t kOperationTimeoutTicks = pdMS_TO_TICKS(250);
constexpr uint64_t kTimerAlarmCount = 2000;

enum Failure : uint32_t {
    kAllocationFailure = 1U << 0,
    kTaskCreationFailure = 1U << 1,
    kHandshakeTimeout = 1U << 2,
    kTaskProgressFailure = 1U << 3,
    kBlockWakeFailure = 1U << 4,
    kTimerDriverFailure = 1U << 5,
    kInterruptFailure = 1U << 6,
    kInterruptWakeFailure = 1U << 7,
    kTickFailure = 1U << 8,
};

struct InterruptRecord {
    uint32_t sequence;
    uint64_t alarmValue;
    uint64_t countValue;
    int32_t core;
};

EventGroupHandle_t eventGroup = nullptr;
SemaphoreHandle_t toCore1Semaphore = nullptr;
SemaphoreHandle_t toCore0Semaphore = nullptr;
QueueHandle_t interruptQueue = nullptr;
TaskHandle_t core1Task = nullptr;

bool IRAM_ATTR onTimerAlarm(gptimer_handle_t, const gptimer_alarm_event_data_t* eventData, void*) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    const uint32_t sequence = gProbeState.irqCount + 1U;
    gProbeState.irqCount = sequence;
    gProbeState.irqCore = esp_cpu_get_core_id();
    gProbeState.irqAlarm = eventData->alarm_value;
    gProbeState.irqCountValue = eventData->count_value;

    const InterruptRecord record = {
        .sequence = sequence,
        .alarmValue = eventData->alarm_value,
        .countValue = eventData->count_value,
        .core = esp_cpu_get_core_id(),
    };
    if (xQueueSendFromISR(interruptQueue, &record, &higherPriorityTaskWoken) != pdTRUE) {
        gProbeState.irqDrops = gProbeState.irqDrops + 1U;
    }
    vTaskNotifyGiveFromISR(core1Task, &higherPriorityTaskWoken);
    return higherPriorityTaskWoken == pdTRUE;
}

void core1Worker(void*) {
    gProbeState.core1Observed = esp_cpu_get_core_id();
    gProbeState.core1Runs = gProbeState.core1Runs + 1U;
    gProbeState.core1Phases = 1;

    const TickType_t semaphoreWaitStart = xTaskGetTickCount();
    gProbeState.core1Blocks = gProbeState.core1Blocks + 1U;
    xEventGroupSetBits(eventGroup, kCore1Ready);
    if (xSemaphoreTake(toCore1Semaphore, kOperationTimeoutTicks) == pdTRUE) {
        gProbeState.core1WaitTicks = xTaskGetTickCount() - semaphoreWaitStart;
        gProbeState.core1Wakeups = gProbeState.core1Wakeups + 1U;
        gProbeState.taskHandoff0To1 = gProbeState.taskHandoff0To1 + 1U;
        gProbeState.core1Phases = 2;

        vTaskDelay(kCore0WakeDelayTicks);
        xSemaphoreGive(toCore0Semaphore);
        gProbeState.core1Phases = 3;

        const TickType_t irqWaitStart = xTaskGetTickCount();
        gProbeState.core1Blocks = gProbeState.core1Blocks + 1U;
        xEventGroupSetBits(eventGroup, kCore1IrqReady);
        if (ulTaskNotifyTake(pdTRUE, kOperationTimeoutTicks) == 1U) {
            gProbeState.irqWaitTicks = xTaskGetTickCount() - irqWaitStart;
            gProbeState.core1Wakeups = gProbeState.core1Wakeups + 1U;
            gProbeState.irqConsumerCore = esp_cpu_get_core_id();
            gProbeState.irqConsumerWakeups = gProbeState.irqConsumerWakeups + 1U;
            gProbeState.core1Phases = 4;
        }
    }

    gProbeState.core1Phases = gProbeState.core1Phases + 1U;
    xEventGroupSetBits(eventGroup, kCore1Done);
    vTaskDelete(nullptr);
}

void core0Worker(void*) {
    gProbeState.core0Observed = esp_cpu_get_core_id();
    gProbeState.core0Runs = gProbeState.core0Runs + 1U;
    gProbeState.core0Phases = 1;

    const EventBits_t ready =
        xEventGroupWaitBits(eventGroup, kCore1Ready, pdFALSE, pdTRUE, kOperationTimeoutTicks);
    if ((ready & kCore1Ready) != 0) {
        gProbeState.core0Phases = 2;
        vTaskDelay(kCore1WakeDelayTicks);
        xSemaphoreGive(toCore1Semaphore);
        gProbeState.core0Phases = 3;

        const TickType_t waitStart = xTaskGetTickCount();
        gProbeState.core0Blocks = gProbeState.core0Blocks + 1U;
        if (xSemaphoreTake(toCore0Semaphore, kOperationTimeoutTicks) == pdTRUE) {
            gProbeState.core0WaitTicks = xTaskGetTickCount() - waitStart;
            gProbeState.core0Wakeups = gProbeState.core0Wakeups + 1U;
            gProbeState.taskHandoff1To0 = gProbeState.taskHandoff1To0 + 1U;
            gProbeState.core0Phases = 4;
        }
    }

    gProbeState.core0Phases = gProbeState.core0Phases + 1U;
    xEventGroupSetBits(eventGroup, kCore0Done);
    vTaskDelete(nullptr);
}

bool runTimerProbe(InterruptRecord& record) {
    gptimer_handle_t timer = nullptr;
    bool enabled = false;
    bool started = false;
    const gptimer_config_t timerConfig = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
        .intr_priority = 0,
        .flags = {},
    };
    if (gptimer_new_timer(&timerConfig, &timer) != ESP_OK) {
        return false;
    }

    const gptimer_event_callbacks_t callbacks = {
        .on_alarm = onTimerAlarm,
    };
    const gptimer_alarm_config_t alarmConfig = {
        .alarm_count = kTimerAlarmCount,
        .reload_count = 0,
        .flags = {},
    };

    bool passed = gptimer_register_event_callbacks(timer, &callbacks, nullptr) == ESP_OK;
    if (passed) {
        passed = gptimer_enable(timer) == ESP_OK;
        enabled = passed;
    }
    if (passed) {
        passed = gptimer_set_alarm_action(timer, &alarmConfig) == ESP_OK;
    }
    if (passed) {
        gProbeState.tickStart = xTaskGetTickCount();
        passed = gptimer_start(timer) == ESP_OK;
        started = passed;
    }
    if (passed) {
        passed = xQueueReceive(interruptQueue, &record, kOperationTimeoutTicks) == pdTRUE;
        gProbeState.tickEnd = xTaskGetTickCount();
        gProbeState.tickDelta = gProbeState.tickEnd - gProbeState.tickStart;
    }

    bool cleanupPassed = true;
    if (started) {
        cleanupPassed = gptimer_stop(timer) == ESP_OK;
    }
    if (enabled) {
        cleanupPassed = gptimer_disable(timer) == ESP_OK && cleanupPassed;
    }
    cleanupPassed = gptimer_del_timer(timer) == ESP_OK && cleanupPassed;
    gProbeState.timerCleanup = cleanupPassed ? 1U : 0U;
    return passed && cleanupPassed;
}

void recordFailures(bool initialized, BaseType_t core0Created, BaseType_t core1Created,
                    EventBits_t handshake, EventBits_t completed, bool timerPassed,
                    const InterruptRecord& record) {
    uint32_t failureMask = 0;
    if (!initialized) {
        failureMask |= kAllocationFailure;
    }
    if (core0Created != pdPASS || core1Created != pdPASS) {
        failureMask |= kTaskCreationFailure;
    }
    if ((handshake & (kCore0Done | kCore1IrqReady)) != (kCore0Done | kCore1IrqReady) ||
        (completed & kCore1Done) == 0) {
        failureMask |= kHandshakeTimeout;
    }
    if (gProbeState.controllerCore != 0 || gProbeState.core0Observed != 0 ||
        gProbeState.core1Observed != 1 || gProbeState.core0Runs != 1 ||
        gProbeState.core1Runs != 1 || gProbeState.core0Phases != 5 ||
        gProbeState.core1Phases != 5) {
        failureMask |= kTaskProgressFailure;
    }
    if (gProbeState.core0Blocks != 1 || gProbeState.core1Blocks != 2 ||
        gProbeState.core0Wakeups != 1 || gProbeState.core1Wakeups != 2 ||
        gProbeState.taskHandoff0To1 != 1 || gProbeState.taskHandoff1To0 != 1 ||
        gProbeState.core0WaitTicks < kCore0WakeDelayTicks ||
        gProbeState.core1WaitTicks < kCore1WakeDelayTicks || gProbeState.irqWaitTicks == 0) {
        failureMask |= kBlockWakeFailure;
    }
    if (!timerPassed) {
        failureMask |= kTimerDriverFailure;
    }
    if (gProbeState.irqCount != 1 || gProbeState.irqDrops != 0 || record.sequence != 1 ||
        record.core != 0 || record.alarmValue != kTimerAlarmCount ||
        record.countValue < kTimerAlarmCount) {
        failureMask |= kInterruptFailure;
    }
    if (gProbeState.irqConsumerCore != 1 || gProbeState.irqConsumerWakeups != 1 ||
        gProbeState.core1Wakeups != 2 || gProbeState.core1Phases != 5) {
        failureMask |= kInterruptWakeFailure;
    }
    if (gProbeState.tickDelta == 0) {
        failureMask |= kTickFailure;
    }
    gProbeState.failureMask = failureMask;
}

void printResult() {
    std::printf("DOMES_QEMU_OBSERVATION schema=3 core0_wait_ticks=%" PRIu32
                " core1_wait_ticks=%" PRIu32 " irq_wait_ticks=%" PRIu32 " tick_start=%" PRIu32
                " tick_end=%" PRIu32 " tick_delta=%" PRIu32 " irq_alarm=%" PRIu64
                " irq_count_value=%" PRIu64 " irq_count_delta=%" PRIu64 "\n",
                gProbeState.core0WaitTicks, gProbeState.core1WaitTicks, gProbeState.irqWaitTicks,
                gProbeState.tickStart, gProbeState.tickEnd, gProbeState.tickDelta,
                gProbeState.irqAlarm, gProbeState.irqCountValue, gProbeState.irqCountDelta);
    std::printf(
        "DOMES_QEMU_RESULT schema=3 status=%s failure_mask=%" PRIu32
        " cores=2 controller_core=%" PRId32 " core0_task_core=%" PRId32 " core1_task_core=%" PRId32
        " core0_runs=%" PRIu32 " core1_runs=%" PRIu32 " core0_phases=%" PRIu32
        " core1_phases=%" PRIu32 " core0_blocks=%" PRIu32 " core1_blocks=%" PRIu32
        " core0_wakeups=%" PRIu32 " core1_wakeups=%" PRIu32 " task_handoff_0_to_1=%" PRIu32
        " task_handoff_1_to_0=%" PRIu32 " tick_progress=%" PRIu32 " irq_source_core=%" PRId32
        " irq_count=%" PRIu32 " irq_drops=%" PRIu32 " irq_sequence=%" PRIu32
        " irq_consumer_core=%" PRId32 " irq_consumer_wakeups=%" PRIu32
        " irq_to_core1_handoff=%" PRIu32 " timer_cleanup=%" PRIu32 " probe_state=complete\n",
        gProbeState.failureMask == 0 ? "PASS" : "FAIL", gProbeState.failureMask,
        gProbeState.controllerCore, gProbeState.core0Observed, gProbeState.core1Observed,
        gProbeState.core0Runs, gProbeState.core1Runs, gProbeState.core0Phases,
        gProbeState.core1Phases, gProbeState.core0Blocks, gProbeState.core1Blocks,
        gProbeState.core0Wakeups, gProbeState.core1Wakeups, gProbeState.taskHandoff0To1,
        gProbeState.taskHandoff1To0, static_cast<uint32_t>(gProbeState.tickDelta > 0),
        gProbeState.irqCore, gProbeState.irqCount, gProbeState.irqDrops, gProbeState.irqSequence,
        gProbeState.irqConsumerCore, gProbeState.irqConsumerWakeups,
        static_cast<uint32_t>(gProbeState.irqConsumerCore == 1 &&
                              gProbeState.irqConsumerWakeups == 1),
        gProbeState.timerCleanup);
    std::fflush(stdout);
}

}  // namespace

extern "C" void app_main() {
    gProbeState.schema = 3;
    gProbeState.controllerCore = esp_cpu_get_core_id();
    gProbeState.core0Observed = -1;
    gProbeState.core1Observed = -1;
    gProbeState.irqCore = -1;
    gProbeState.irqConsumerCore = -1;
    ESP_LOGI(kTag, "FS-WP-002B bounded feasibility probe starting");

    eventGroup = xEventGroupCreate();
    toCore1Semaphore = xSemaphoreCreateBinary();
    toCore0Semaphore = xSemaphoreCreateBinary();
    interruptQueue = xQueueCreate(1, sizeof(InterruptRecord));
    const bool initialized = eventGroup != nullptr && toCore1Semaphore != nullptr &&
                             toCore0Semaphore != nullptr && interruptQueue != nullptr;

    BaseType_t core0Created = pdFAIL;
    BaseType_t core1Created = pdFAIL;
    if (initialized) {
        core1Created =
            xTaskCreatePinnedToCore(core1Worker, "probe_core1", 3072, nullptr, 6, &core1Task, 1);
        core0Created =
            xTaskCreatePinnedToCore(core0Worker, "probe_core0", 3072, nullptr, 6, nullptr, 0);
    }

    EventBits_t handshake = 0;
    if (core0Created == pdPASS && core1Created == pdPASS) {
        handshake = xEventGroupWaitBits(eventGroup, kCore0Done | kCore1IrqReady, pdFALSE, pdTRUE,
                                        kOperationTimeoutTicks);
    }

    InterruptRecord interruptRecord = {};
    const bool timerPassed =
        initialized &&
        (handshake & (kCore0Done | kCore1IrqReady)) == (kCore0Done | kCore1IrqReady) &&
        runTimerProbe(interruptRecord);
    if (timerPassed) {
        gProbeState.irqSequence = interruptRecord.sequence;
        gProbeState.irqAlarm = interruptRecord.alarmValue;
        gProbeState.irqCountValue = interruptRecord.countValue;
        gProbeState.irqCountDelta = interruptRecord.countValue - interruptRecord.alarmValue;
        gProbeState.irqCore = interruptRecord.core;
    }

    EventBits_t completed = 0;
    if (initialized) {
        completed =
            xEventGroupWaitBits(eventGroup, kCore1Done, pdFALSE, pdTRUE, kOperationTimeoutTicks);
    }

    recordFailures(initialized, core0Created, core1Created, handshake, completed, timerPassed,
                   interruptRecord);
    domesQemuProbeComplete();
    printResult();

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}
