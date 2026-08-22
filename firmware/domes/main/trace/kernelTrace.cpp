#include "kernelTrace.hpp"

#include "esp_attr.h"
#include "esp_timer.h"
#include "soc/interrupts.h"
#include "traceBuffer.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace {

constexpr size_t kCoreCount = domes::trace::KernelTrace::kCoreCount;
constexpr size_t kEventsPerCore = domes::trace::KernelTrace::kEventsPerCore;
constexpr size_t kMaxIsrNesting = 8;
constexpr size_t kMaxTaskId = 31;

struct CoreBuffer {
    std::array<domes::trace::TraceEvent, kEventsPerCore> events{};
    std::atomic<uint32_t> reserved{0};
    std::atomic<uint32_t> dropped{0};
};

DRAM_ATTR CoreBuffer gCoreBuffers[kCoreCount];
static_assert(sizeof(gCoreBuffers[0].events) == domes::trace::KernelTrace::kPerCoreCaptureBytes);
static_assert(sizeof(gCoreBuffers[0].events) * kCoreCount ==
              domes::trace::KernelTrace::kCaptureCapacityBytes);
static_assert(domes::trace::TraceBuffer::kMaxEvents >=
                  domes::trace::KernelTrace::kCoreCount * kEventsPerCore,
              "downstream trace buffer must retain the complete per-core capture");
DRAM_ATTR portMUX_TYPE gCoreLocks[kCoreCount] = {portMUX_INITIALIZER_UNLOCKED,
                                                 portMUX_INITIALIZER_UNLOCKED};
DRAM_ATTR portMUX_TYPE gTaskLifecycleLock = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR portMUX_TYPE gTaskHandleLock = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR std::atomic<bool> gEnabled{false};
DRAM_ATTR std::atomic<uint32_t> gDiscontinuities{0};
DRAM_ATTR std::atomic<uint32_t> gActiveTaskMask{0};
// The dedicated DRAM critical-section lock avoids libatomic pointer helpers
// and C++ data races in cache-disabled hooks. Task registration is frozen
// before capture, and task deletion only clears entries.
DRAM_ATTR const void* gTaskHandles[kMaxTaskId + 1] = {};
DRAM_ATTR const void* gRunningTaskHandles[kCoreCount] = {};
DRAM_ATTR std::atomic<uint32_t> gIsrDepth[kCoreCount];
DRAM_ATTR uint32_t gInterruptStack[kCoreCount][kMaxIsrNesting];
DRAM_ATTR std::array<domes::trace::ObjectNameEntry, domes::trace::kMaxTraceObjects> gObjects{};

uint32_t boundedCount(const CoreBuffer& buffer) {
    return std::min<uint32_t>(buffer.reserved.load(std::memory_order_acquire), kEventsPerCore);
}

uint32_t sanitizeIsrSessionBoundary(CoreBuffer& buffer, uint32_t count) {
    std::array<uint32_t, kMaxIsrNesting> openIndexes{};
    std::array<uint32_t, kMaxIsrNesting> interruptIds{};
    size_t depth = 0;
    uint32_t output = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const auto& event = buffer.events[index];
        const auto type = static_cast<domes::trace::EventType>(event.eventType);
        if (type == domes::trace::EventType::kSchedIsrExit && depth == 0) {
            continue;  // ISR entered before this trace session.
        }
        if (type == domes::trace::EventType::kSchedIsrEnter) {
            if (depth >= kMaxIsrNesting) {
                gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            openIndexes[depth] = output;
            interruptIds[depth] = event.arg1;
            ++depth;
        } else if (type == domes::trace::EventType::kSchedIsrExit) {
            if (interruptIds[depth - 1] != event.arg1) {
                gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
            }
            --depth;
        }
        buffer.events[output++] = event;
    }
    // An ISR still open at stop crossed the session boundary. Truncate this
    // core at its outermost open entry instead of manufacturing a lifecycle
    // discontinuity; no subsequent task event can occur on that core first.
    return depth == 0 ? output : openIndexes[0];
}

uint32_t sanitizeSchedulerSessionBoundary(CoreBuffer& buffer, uint32_t count) {
    uint16_t runningTask = 0;
    uint32_t openIndex = 0;
    uint32_t output = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const auto& event = buffer.events[index];
        const auto type = static_cast<domes::trace::EventType>(event.eventType);
        if (type == domes::trace::EventType::kSchedSwitchOut && runningTask == 0) {
            continue;  // Task was already running when capture started.
        }
        if (type == domes::trace::EventType::kSchedSwitchIn) {
            if (runningTask != 0) {
                gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
            }
            runningTask = event.taskId;
            openIndex = output;
        } else if (type == domes::trace::EventType::kSchedSwitchOut) {
            if (runningTask != event.taskId) {
                gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
            }
            runningTask = 0;
        }
        buffer.events[output++] = event;
    }
    if (runningTask != 0) {
        for (uint32_t index = openIndex + 1; index < output; ++index) {
            buffer.events[index - 1] = buffer.events[index];
        }
        --output;  // Task was still running when capture stopped.
    }
    return output;
}

bool timestampBefore(const domes::trace::TraceEvent& left, const domes::trace::TraceEvent& right) {
    return static_cast<int32_t>(left.timestamp - right.timestamp) < 0;
}

void stableSortByTimestamp(CoreBuffer& buffer, uint32_t count) {
    // Timestamps can be captured before a nested ISR claims the same core's
    // next slot. Sort in place after capture so the hook path stays bounded
    // and allocation-free without retaining a second full-session buffer.
    for (uint32_t index = 1; index < count; ++index) {
        const domes::trace::TraceEvent event = buffer.events[index];
        uint32_t position = index;
        while (position > 0 && timestampBefore(event, buffer.events[position - 1])) {
            buffer.events[position] = buffer.events[position - 1];
            --position;
        }
        buffer.events[position] = event;
    }
}

uint32_t IRAM_ATTR currentCore() {
    const BaseType_t core = xPortGetCoreID();
    return core == 1 ? 1U : 0U;
}

uint16_t IRAM_ATTR traceTaskIdLocked(const void* candidate) {
    if (candidate == nullptr) {
        return 0;
    }
    for (size_t id = 1; id <= kMaxTaskId; ++id) {
        if (gTaskHandles[id] == candidate) {
            return static_cast<uint16_t>(id);
        }
    }
    return 0;
}

int32_t IRAM_ATTR beginTraceHook() {
    if (!gEnabled.load(std::memory_order_acquire)) {
        return -1;
    }
    const uint32_t core = currentCore();
    portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
    if (!gEnabled.load(std::memory_order_acquire)) {
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
        return -1;
    }
    return static_cast<int32_t>(core);
}

void IRAM_ATTR endTraceHook(uint32_t core) {
    portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
}

uint16_t IRAM_ATTR currentTraceTaskId(uint32_t core) {
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    const uint16_t taskId = traceTaskIdLocked(gRunningTaskHandles[core]);
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
    return taskId;
}

void IRAM_ATTR clearRunningTaskHandle(const volatile void* task) {
    const void* candidate = const_cast<const void*>(task);
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    for (auto& running : gRunningTaskHandles) {
        if (running == candidate) {
            running = nullptr;
        }
    }
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
}

void IRAM_ATTR setRunningTaskHandle(uint32_t core, const volatile void* task) {
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    gRunningTaskHandles[core] = const_cast<const void*>(task);
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
}

}  // namespace

namespace domes::trace {

void KernelTrace::start() {
    gEnabled.store(false, std::memory_order_release);
    for (size_t core = 0; core < kCoreCount; ++core) {
        portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
        auto& buffer = gCoreBuffers[core];
        buffer.reserved.store(0, std::memory_order_relaxed);
        buffer.dropped.store(0, std::memory_order_relaxed);
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
    }
    for (size_t core = 0; core < kCoreCount; ++core) {
        gIsrDepth[core].store(0, std::memory_order_relaxed);
        for (size_t depth = 0; depth < kMaxIsrNesting; ++depth) {
            gInterruptStack[core][depth] = 0;
        }
    }
    gDiscontinuities.store(0, std::memory_order_relaxed);
}

void KernelTrace::enable() {
    gEnabled.store(true, std::memory_order_release);
}

void KernelTrace::stopAndFlush(TraceBuffer& destination) {
    gEnabled.store(false, std::memory_order_release);
    for (size_t core = 0; core < kCoreCount; ++core) {
        portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
    }

    destination.clear();
    const std::array<uint32_t, kCoreCount> count = {
        sanitizeSchedulerSessionBoundary(
            gCoreBuffers[0],
            sanitizeIsrSessionBoundary(gCoreBuffers[0], boundedCount(gCoreBuffers[0]))),
        sanitizeSchedulerSessionBoundary(
            gCoreBuffers[1],
            sanitizeIsrSessionBoundary(gCoreBuffers[1], boundedCount(gCoreBuffers[1])))};
    for (size_t core = 0; core < kCoreCount; ++core) {
        stableSortByTimestamp(gCoreBuffers[core], count[core]);
    }

    // Stable two-way merge. Core 0 wins equal timestamps, matching the old
    // stable sort's core-0-then-core-1 input order without a 16 KiB scratch
    // array.
    std::array<uint32_t, kCoreCount> position{};
    while (position[0] < count[0] || position[1] < count[1]) {
        size_t selectedCore = 0;
        if (position[0] >= count[0] ||
            (position[1] < count[1] && timestampBefore(gCoreBuffers[1].events[position[1]],
                                                       gCoreBuffers[0].events[position[0]]))) {
            selectedCore = 1;
        }
        if (!destination.record(gCoreBuffers[selectedCore].events[position[selectedCore]++])) {
            gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void KernelTrace::clear() {
    const bool restart = gEnabled.exchange(false, std::memory_order_acq_rel);
    for (size_t core = 0; core < kCoreCount; ++core) {
        portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
        auto& buffer = gCoreBuffers[core];
        buffer.reserved.store(0, std::memory_order_relaxed);
        buffer.dropped.store(0, std::memory_order_relaxed);
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
    }
    gDiscontinuities.store(0, std::memory_order_relaxed);
    if (restart) {
        gEnabled.store(true, std::memory_order_release);
    }
}

bool IRAM_ATTR KernelTrace::isEnabled() {
    return gEnabled.load(std::memory_order_acquire);
}

bool IRAM_ATTR KernelTrace::recordOnCurrentCore(const TraceEvent& event) {
    if (!gEnabled.load(std::memory_order_acquire)) {
        return false;
    }
    const uint32_t core = currentCore();
    CoreBuffer& buffer = gCoreBuffers[core];
    portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
    if (!gEnabled.load(std::memory_order_acquire)) {
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
        return false;
    }
    const uint32_t index = buffer.reserved.fetch_add(1, std::memory_order_acq_rel);
    if (index >= kEventsPerCore) {
        buffer.dropped.fetch_add(1, std::memory_order_relaxed);
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
        return false;
    }
    buffer.events[index] = event;
    portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
    return true;
}

bool KernelTrace::record(const TraceEvent& event) {
    return recordOnCurrentCore(event);
}

bool IRAM_ATTR KernelTrace::recordFromIsr(const TraceEvent& event) {
    return recordOnCurrentCore(event);
}

bool KernelTrace::recordPreamble(const TraceEvent& event) {
    const uint32_t core = currentCore();
    CoreBuffer& buffer = gCoreBuffers[core];
    portENTER_CRITICAL_SAFE(&gCoreLocks[core]);
    const uint32_t index = buffer.reserved.fetch_add(1, std::memory_order_relaxed);
    if (index >= kEventsPerCore) {
        buffer.dropped.fetch_add(1, std::memory_order_relaxed);
        portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
        return false;
    }
    buffer.events[index] = event;
    portEXIT_CRITICAL_SAFE(&gCoreLocks[core]);
    return true;
}

bool KernelTrace::registerTaskHandle(const void* handle, uint16_t taskId) {
    if (handle == nullptr || taskId == 0 || taskId > kMaxTaskId || isEnabled()) {
        return false;
    }
    bool available = true;
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    for (size_t id = 1; id <= kMaxTaskId; ++id) {
        const void* registered = gTaskHandles[id];
        if ((id == taskId && registered != nullptr && registered != handle) ||
            (id != taskId && registered == handle)) {
            available = false;
            break;
        }
    }
    if (available) {
        gTaskHandles[taskId] = handle;
    }
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
    return available;
}

uint16_t IRAM_ATTR KernelTrace::taskId(const volatile void* handle) {
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    const uint16_t taskId = traceTaskIdLocked(const_cast<const void*>(handle));
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
    return taskId;
}

uint16_t IRAM_ATTR KernelTrace::unregisterTaskHandle(const volatile void* handle) {
    const void* candidate = const_cast<const void*>(handle);
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    const uint16_t id = traceTaskIdLocked(candidate);
    if (id != 0) {
        gTaskHandles[id] = nullptr;
    }
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
    return id;
}

void KernelTrace::clearTaskHandles() {
    if (isEnabled()) {
        noteDiscontinuity();
        return;
    }
    portENTER_CRITICAL_SAFE(&gTaskHandleLock);
    for (auto& handle : gTaskHandles) {
        handle = nullptr;
    }
    portEXIT_CRITICAL_SAFE(&gTaskHandleLock);
    gActiveTaskMask.store(0, std::memory_order_release);
}

void IRAM_ATTR KernelTrace::setTaskActive(uint16_t taskId, bool active) {
    if (taskId == 0 || taskId > 31) {
        return;
    }
    const uint32_t bit = 1U << taskId;
    portENTER_CRITICAL_SAFE(&gTaskLifecycleLock);
    if (active) {
        gActiveTaskMask.fetch_or(bit, std::memory_order_acq_rel);
    } else {
        gActiveTaskMask.fetch_and(~bit, std::memory_order_acq_rel);
    }
    portEXIT_CRITICAL_SAFE(&gTaskLifecycleLock);
}

bool KernelTrace::isTaskActive(uint16_t taskId) {
    return taskId != 0 && taskId <= 31 &&
           (gActiveTaskMask.load(std::memory_order_acquire) & (1U << taskId)) != 0;
}

void KernelTrace::beginTaskSnapshot() {
    portENTER_CRITICAL_SAFE(&gTaskLifecycleLock);
}

void KernelTrace::endTaskSnapshot() {
    portEXIT_CRITICAL_SAFE(&gTaskLifecycleLock);
}

bool KernelTrace::registerObject(const void* handle, uint32_t objectId, ObjectKind kind,
                                 const char* name) {
    if (isEnabled() || handle == nullptr || objectId == 0 || kind == ObjectKind::kUnknown ||
        name == nullptr || name[0] == '\0' || std::strlen(name) >= kMaxTraceObjectNameLength) {
        return false;
    }
    for (auto& entry : gObjects) {
        if (entry.valid && (entry.handle == handle || entry.objectId == objectId)) {
            const bool samePublishedName = std::strcmp(entry.name, name) == 0;
            if (entry.handle == nullptr && entry.objectId == objectId && entry.kind == kind &&
                samePublishedName) {
                entry.handle = handle;
                return true;
            }
            return entry.handle == handle && entry.objectId == objectId && entry.kind == kind &&
                   samePublishedName;
        }
    }
    for (auto& entry : gObjects) {
        if (!entry.valid) {
            entry.handle = handle;
            entry.objectId = objectId;
            entry.kind = kind;
            std::strncpy(entry.name, name, sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            entry.valid = true;
            return true;
        }
    }
    return false;
}

bool KernelTrace::unregisterObject(const void* handle) {
    if (isEnabled() || handle == nullptr) {
        return false;
    }
    for (auto& entry : gObjects) {
        if (entry.valid && entry.handle == handle) {
            entry.handle = nullptr;
            return true;
        }
    }
    return false;
}

void KernelTrace::clearObjects() {
    if (isEnabled()) {
        noteDiscontinuity();
        return;
    }
    gObjects = {};
}

const std::array<ObjectNameEntry, kMaxTraceObjects>& KernelTrace::objects() {
    return gObjects;
}

uint32_t IRAM_ATTR KernelTrace::objectId(const void* handle) {
    for (const auto& entry : gObjects) {
        if (entry.valid && entry.handle == handle) {
            return entry.objectId;
        }
    }
    return 0;
}

ObjectKind IRAM_ATTR KernelTrace::objectKind(const void* handle) {
    for (const auto& entry : gObjects) {
        if (entry.valid && entry.handle == handle) {
            return entry.kind;
        }
    }
    return ObjectKind::kUnknown;
}

bool IRAM_ATTR KernelTrace::isRegisteredInterruptId(uint32_t objectId) {
    for (const auto& entry : gObjects) {
        if (entry.valid && entry.objectId == objectId && entry.kind == ObjectKind::kInterrupt) {
            return true;
        }
    }
    return false;
}

uint32_t KernelTrace::eventCount() {
    return boundedCount(gCoreBuffers[0]) + boundedCount(gCoreBuffers[1]);
}

uint32_t KernelTrace::droppedCount() {
    return gCoreBuffers[0].dropped.load(std::memory_order_relaxed) +
           gCoreBuffers[1].dropped.load(std::memory_order_relaxed);
}

uint32_t KernelTrace::discontinuityCount() {
    return gDiscontinuities.load(std::memory_order_relaxed);
}

void IRAM_ATTR KernelTrace::noteDiscontinuity() {
    gDiscontinuities.fetch_add(1, std::memory_order_relaxed);
}

uint8_t IRAM_ATTR KernelTrace::contextFlags(bool isr, bool callback) {
    const uint8_t coreCode = currentCore() == 0 ? 1U : 2U;
    const uint8_t context = callback ? 2U : (isr ? 1U : 0U);
    return static_cast<uint8_t>(coreCode | (context << 2));
}

TraceEvent IRAM_ATTR KernelTrace::makeKernelEvent(EventType type, uint16_t taskId, uint32_t arg1,
                                                  uint32_t arg2, bool isr, bool callback) {
    TraceEvent event{};
    event.timestamp = static_cast<uint32_t>(esp_timer_get_time());
    event.taskId = taskId;
    event.eventType = static_cast<uint8_t>(type);
    event.flags = contextFlags(isr, callback);
    event.setCategory(Category::kKernel);
    event.arg1 = arg1;
    event.arg2 = arg2;
    return event;
}

}  // namespace domes::trace

extern "C" void IRAM_ATTR domes_trace_hook_task_switch_in(const volatile void* task) {
    const uint32_t taskCore = currentCore();
    setRunningTaskHandle(taskCore, task);
    const int32_t core = beginTraceHook();
    if (core < 0) {
        return;
    }
    const uint16_t taskId = domes::trace::KernelTrace::taskId(task);
    if (taskId != 0) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedSwitchIn, taskId));
    }
    endTraceHook(static_cast<uint32_t>(core));
}

extern "C" void IRAM_ATTR domes_trace_hook_task_switch_out(const volatile void* task) {
    const int32_t core = beginTraceHook();
    if (core < 0) {
        clearRunningTaskHandle(task);
        return;
    }
    const uint16_t taskId = domes::trace::KernelTrace::taskId(task);
    if (taskId != 0) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedSwitchOut, taskId));
    }
    endTraceHook(static_cast<uint32_t>(core));
    clearRunningTaskHandle(task);
}

extern "C" void IRAM_ATTR domes_trace_hook_task_ready(const volatile void* task) {
    const int32_t core = beginTraceHook();
    if (core < 0) {
        return;
    }
    const uint16_t taskId = domes::trace::KernelTrace::taskId(task);
    if (taskId != 0) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedTaskReady, taskId));
    }
    endTraceHook(static_cast<uint32_t>(core));
}

extern "C" void IRAM_ATTR domes_trace_hook_task_delete(const volatile void* task) {
    const uint16_t taskId = domes::trace::KernelTrace::taskId(task);
    if (taskId == 0) {
        clearRunningTaskHandle(task);
        return;
    }
    domes::trace::KernelTrace::setTaskActive(taskId, false);
    const int32_t core = beginTraceHook();
    if (core >= 0) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedTaskDelete, taskId));
        endTraceHook(static_cast<uint32_t>(core));
    }
    domes::trace::KernelTrace::unregisterTaskHandle(task);
    clearRunningTaskHandle(task);
}

extern "C" void IRAM_ATTR domes_trace_hook_task_block(const volatile void* object,
                                                      uint32_t timeoutTicks) {
    const int32_t core = beginTraceHook();
    if (core < 0) {
        return;
    }
    const uint16_t taskId = currentTraceTaskId(static_cast<uint32_t>(core));
    const uint32_t objectId = domes::trace::KernelTrace::objectId(const_cast<const void*>(object));
    if (taskId != 0 && objectId != 0) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedTaskBlock, taskId, objectId, timeoutTicks));
    }
    endTraceHook(static_cast<uint32_t>(core));
}

extern "C" void IRAM_ATTR domes_trace_hook_queue_send(const volatile void* object,
                                                      uint32_t fromIsr) {
    const int32_t core = beginTraceHook();
    if (core < 0) {
        return;
    }
    const void* handle = const_cast<const void*>(object);
    const uint32_t objectId = domes::trace::KernelTrace::objectId(handle);
    if (objectId == 0) {
        endTraceHook(static_cast<uint32_t>(core));
        return;
    }
    const auto kind = domes::trace::KernelTrace::objectKind(handle);
    const auto type = kind == domes::trace::ObjectKind::kSemaphore
                          ? domes::trace::EventType::kSemGive
                          : domes::trace::EventType::kSchedQueueSend;
    domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
        type, fromIsr ? 0 : currentTraceTaskId(static_cast<uint32_t>(core)), objectId, 0,
        fromIsr != 0));
    endTraceHook(static_cast<uint32_t>(core));
}

extern "C" void IRAM_ATTR domes_trace_hook_queue_receive(const volatile void* object,
                                                         uint32_t fromIsr) {
    const int32_t core = beginTraceHook();
    if (core < 0) {
        return;
    }
    const void* handle = const_cast<const void*>(object);
    const uint32_t objectId = domes::trace::KernelTrace::objectId(handle);
    if (objectId == 0) {
        endTraceHook(static_cast<uint32_t>(core));
        return;
    }
    const auto kind = domes::trace::KernelTrace::objectKind(handle);
    const auto type = kind == domes::trace::ObjectKind::kSemaphore
                          ? domes::trace::EventType::kSemTake
                          : domes::trace::EventType::kSchedQueueReceive;
    domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
        type, fromIsr ? 0 : currentTraceTaskId(static_cast<uint32_t>(core)), objectId, 0,
        fromIsr != 0));
    endTraceHook(static_cast<uint32_t>(core));
}

extern "C" void IRAM_ATTR domes_trace_hook_isr_enter(uint32_t interruptId) {
    const int32_t hookCore = beginTraceHook();
    if (hookCore < 0) {
        return;
    }
    const uint32_t core = static_cast<uint32_t>(hookCore);
    const uint32_t previous = gIsrDepth[core].fetch_add(1, std::memory_order_acq_rel);
    if (previous >= kMaxIsrNesting) {
        domes::trace::KernelTrace::noteDiscontinuity();
    } else {
        gInterruptStack[core][previous] = interruptId;
    }
    // The 1 kHz per-core scheduler tick would consume the complete bounded
    // session in about 250 ms while adding no application causality. Keep it
    // on the nesting stack so paired exits remain correct, but do not emit it.
    if (interruptId != ETS_SYSTIMER_TARGET0_INTR_SOURCE &&
        domes::trace::KernelTrace::isRegisteredInterruptId(interruptId)) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedIsrEnter, 0, interruptId, 0, true));
    }
    endTraceHook(core);
}

extern "C" void IRAM_ATTR domes_trace_hook_isr_exit(void) {
    const int32_t hookCore = beginTraceHook();
    if (hookCore < 0) {
        return;
    }
    const uint32_t core = static_cast<uint32_t>(hookCore);
    const uint32_t depth = gIsrDepth[core].load(std::memory_order_acquire);
    if (depth == 0) {
        endTraceHook(core);
        return;
    }
    const uint32_t interruptId = depth <= kMaxIsrNesting ? gInterruptStack[core][depth - 1] : 0U;
    if (interruptId != ETS_SYSTIMER_TARGET0_INTR_SOURCE &&
        domes::trace::KernelTrace::isRegisteredInterruptId(interruptId)) {
        domes::trace::KernelTrace::recordFromIsr(domes::trace::KernelTrace::makeKernelEvent(
            domes::trace::EventType::kSchedIsrExit, 0, interruptId, 0, true));
    }
    gIsrDepth[core].fetch_sub(1, std::memory_order_release);
    endTraceHook(core);
}
