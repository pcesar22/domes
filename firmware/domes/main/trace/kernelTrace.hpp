#pragma once

#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "traceEvent.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace domes::trace {

class TraceBuffer;

constexpr uint32_t kTraceEventFormatVersion = 1;
constexpr size_t kMaxTraceObjects = 8;
constexpr size_t kMaxTraceObjectNameLength = 16;

enum class ObjectKind : uint8_t {
    kUnknown = domes_trace_ObjectKind_OBJECT_KIND_UNKNOWN,
    kQueue = domes_trace_ObjectKind_OBJECT_KIND_QUEUE,
    kSemaphore = domes_trace_ObjectKind_OBJECT_KIND_SEMAPHORE,
    kInterrupt = domes_trace_ObjectKind_OBJECT_KIND_INTERRUPT,
    kCallback = domes_trace_ObjectKind_OBJECT_KIND_CALLBACK,
    kAction = domes_trace_ObjectKind_OBJECT_KIND_ACTION,
    kMutex = domes_trace_ObjectKind_OBJECT_KIND_MUTEX,
    kTimeout = domes_trace_ObjectKind_OBJECT_KIND_TIMEOUT,
};

struct ObjectNameEntry {
    const void* handle = nullptr;
    uint32_t objectId = 0;
    ObjectKind kind = ObjectKind::kUnknown;
    char name[kMaxTraceObjectNameLength] = {};
    bool valid = false;
};

class KernelTrace {
public:
    static constexpr size_t kCoreCount = 2;
    static constexpr size_t kEventsPerCore = 512;
    static constexpr size_t kCaptureCapacityBytes =
        kCoreCount * kEventsPerCore * sizeof(TraceEvent);

    static void start();
    static void enable();
    static void stopAndFlush(TraceBuffer& destination);
    static void clear();
    static bool IRAM_ATTR isEnabled();

    static bool record(const TraceEvent& event);
    static bool IRAM_ATTR recordFromIsr(const TraceEvent& event);
    static bool recordPreamble(const TraceEvent& event);

    static bool registerTaskHandle(const void* handle, uint16_t taskId);
    static uint16_t IRAM_ATTR taskId(const volatile void* handle);
    static uint16_t IRAM_ATTR unregisterTaskHandle(const volatile void* handle);
    static void clearTaskHandles();
    static void IRAM_ATTR setTaskActive(uint16_t taskId, bool active);
    static bool isTaskActive(uint16_t taskId);
    static void beginTaskSnapshot();
    static void endTaskSnapshot();

    static bool registerObject(const void* handle, uint32_t objectId, ObjectKind kind,
                               const char* name);
    static bool unregisterObject(const void* handle);
    static void clearObjects();
    static const std::array<ObjectNameEntry, kMaxTraceObjects>& objects();
    static uint32_t IRAM_ATTR objectId(const void* handle);
    static ObjectKind IRAM_ATTR objectKind(const void* handle);
    static bool IRAM_ATTR isRegisteredInterruptId(uint32_t objectId);

    static uint32_t eventCount();
    static uint32_t droppedCount();
    static uint32_t discontinuityCount();
    static void IRAM_ATTR noteDiscontinuity();

    static uint8_t IRAM_ATTR contextFlags(bool isr, bool callback = false);
    static TraceEvent IRAM_ATTR makeKernelEvent(EventType type, uint16_t taskId, uint32_t arg1 = 0,
                                                uint32_t arg2 = 0, bool isr = false,
                                                bool callback = false);

private:
    static bool IRAM_ATTR recordOnCurrentCore(const TraceEvent& event);
};

}  // namespace domes::trace
