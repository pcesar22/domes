#include "trace/kernelTrace.hpp"
#include "trace/traceApi.hpp"
#include "trace/traceBuffer.hpp"
#include "trace/traceRecorder.hpp"

#include <gtest/gtest.h>

namespace domes::trace {
namespace {

TraceEvent eventAt(uint32_t timestamp) {
    TraceEvent event{};
    event.timestamp = timestamp;
    return event;
}

TEST(TraceBufferTest, AcquiredEventsRemainCountedUntilReleased) {
    TraceBuffer buffer;
    ASSERT_EQ(buffer.init(), ESP_OK);
    ASSERT_TRUE(buffer.record(eventAt(10)));
    ASSERT_TRUE(buffer.record(eventAt(20)));

    const TraceEvent* first = buffer.acquire();
    const TraceEvent* second = buffer.acquire();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->timestamp, 10U);
    EXPECT_EQ(second->timestamp, 20U);
    EXPECT_EQ(buffer.count(), 2U);
    EXPECT_EQ(buffer.acquire(), nullptr);

    buffer.release(first);
    EXPECT_EQ(buffer.count(), 1U);
    buffer.release(second);
    EXPECT_EQ(buffer.count(), 0U);
}

TEST(TraceBufferTest, PendingAcquisitionSurvivesAdditionalRecording) {
    TraceBuffer buffer;
    ASSERT_EQ(buffer.init(), ESP_OK);
    ASSERT_TRUE(buffer.record(eventAt(100)));

    const TraceEvent* pending = buffer.acquire();
    ASSERT_NE(pending, nullptr);
    ASSERT_TRUE(buffer.record(eventAt(200)));

    EXPECT_EQ(pending->timestamp, 100U);
    buffer.release(pending);

    TraceEvent remaining{};
    ASSERT_TRUE(buffer.read(&remaining));
    EXPECT_EQ(remaining.timestamp, 200U);
    EXPECT_EQ(buffer.count(), 0U);
}

TEST(TraceBufferTest, RetainedDumpSnapshotHasSingleOwnerAcrossTransports) {
    TraceBuffer buffer;
    ASSERT_EQ(buffer.init(), ESP_OK);

    TraceEvent first{};
    first.timestamp = 10;
    TraceEvent second{};
    second.timestamp = 20;
    ASSERT_TRUE(buffer.record(first));
    ASSERT_TRUE(buffer.record(second));

    int serialOwner = 0;
    int bleOwner = 0;
    ASSERT_TRUE(buffer.tryClaimDumpSnapshot(&serialOwner));
    buffer.pause();
    ASSERT_EQ(buffer.captureDumpSnapshot(&serialOwner), 2u);
    buffer.resume();

    EXPECT_FALSE(buffer.tryClaimDumpSnapshot(&bleOwner));
    EXPECT_EQ(buffer.dumpSnapshotCount(&bleOwner), 0u);
    ASSERT_NE(buffer.dumpSnapshotEvent(&serialOwner, 0), nullptr);
    EXPECT_EQ(buffer.dumpSnapshotEvent(&serialOwner, 0)->timestamp, 10u);
    EXPECT_FALSE(buffer.completeDumpSnapshot(&bleOwner));
    EXPECT_EQ(buffer.count(), 2u);

    EXPECT_TRUE(buffer.completeDumpSnapshot(&serialOwner));
    EXPECT_EQ(buffer.count(), 0u);
    EXPECT_TRUE(buffer.tryClaimDumpSnapshot(&bleOwner));
    EXPECT_TRUE(buffer.completeDumpSnapshot(&bleOwner));
}

TEST(TraceBufferTest, OwningTransportCanClearRetainedSnapshotAndLaterEvents) {
    TraceBuffer buffer;
    ASSERT_EQ(buffer.init(), ESP_OK);

    TraceEvent event{};
    event.timestamp = 10;
    ASSERT_TRUE(buffer.record(event));

    int owner = 0;
    ASSERT_TRUE(buffer.tryClaimDumpSnapshot(&owner));
    buffer.pause();
    ASSERT_EQ(buffer.captureDumpSnapshot(&owner), 1u);
    buffer.resume();

    event.timestamp = 20;
    ASSERT_TRUE(buffer.record(event));
    EXPECT_EQ(buffer.count(), 2u);
    EXPECT_TRUE(buffer.clearDumpSnapshot(&owner));
    EXPECT_EQ(buffer.count(), 0u);
}

TEST(TraceRecorderTest, StableTaskIdMustFitActiveTaskBitmap) {
    Recorder::shutdown();
    ASSERT_EQ(Recorder::init(1024), ESP_OK);
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();

    EXPECT_TRUE(Recorder::registerTask(handle, "max_id", 31, 1, 0));
    EXPECT_FALSE(Recorder::registerTask(handle, "too_large", 32, 1, 0));

    Recorder::shutdown();
}

TEST(TraceRecorderTest, StableTaskIdsAndHandlesCannotAlias) {
    Recorder::shutdown();
    ASSERT_EQ(Recorder::init(1024), ESP_OK);
    int firstTask = 0;
    int secondTask = 0;
    auto first = reinterpret_cast<TaskHandle_t>(&firstTask);
    auto second = reinterpret_cast<TaskHandle_t>(&secondTask);

    EXPECT_TRUE(Recorder::registerTask(first, "first", 1, 2, 0));
    EXPECT_FALSE(Recorder::registerTask(second, "second", 1, 2, 0));
    EXPECT_FALSE(Recorder::registerTask(first, "first", 2, 2, 0));
    EXPECT_FALSE(Recorder::registerTask(second, "", 2, 2, 0));
    EXPECT_FALSE(Recorder::registerTask(second, "1234567890abcdef", 2, 2, 0));
    EXPECT_FALSE(Recorder::registerTask(second, "second", 2, 2, 2));
    EXPECT_EQ(Recorder::getRegisteredTaskCount(), 1U);

    Recorder::shutdown();
}

TEST(TraceApiTest, ApplicationEventsCarryFormatV1TaskContext) {
    Recorder::shutdown();
    ASSERT_EQ(Recorder::init(1024), ESP_OK);
    ASSERT_TRUE(Recorder::registerTask(xTaskGetCurrentTaskHandle(), "app", 7, 1, 1));
    sim_trace::currentCoreId = 1;

    const TraceEvent event = makeEvent(EventType::kInstant, Category::kGame, 42);

    EXPECT_EQ(event.taskId, 7U);
    EXPECT_EQ(event.coreCode(), 2U);
    EXPECT_EQ(event.contextCode(), 0U);
    EXPECT_EQ(event.category(), Category::kGame);

    Recorder::shutdown();
}

TEST(KernelTraceTest, TaskHandlesMapOnlyToExplicitStableIds) {
    KernelTrace::clearTaskHandles();
    int registeredTask = 0;
    int secondTask = 0;

    EXPECT_EQ(KernelTrace::taskId(&registeredTask), 0U);
    ASSERT_TRUE(KernelTrace::registerTaskHandle(&registeredTask, 7));
    EXPECT_EQ(KernelTrace::taskId(&registeredTask), 7U);
    EXPECT_FALSE(KernelTrace::registerTaskHandle(&secondTask, 7));
    EXPECT_FALSE(KernelTrace::registerTaskHandle(&registeredTask, 8));
    EXPECT_EQ(KernelTrace::taskId(&secondTask), 0U);
    EXPECT_EQ(KernelTrace::unregisterTaskHandle(&registeredTask), 7U);
    EXPECT_EQ(KernelTrace::taskId(&registeredTask), 0U);
    EXPECT_TRUE(KernelTrace::registerTaskHandle(&secondTask, 7));
    KernelTrace::clearTaskHandles();
}

TEST(KernelTraceTest, ObjectIdentityCannotChangeWhenReboundOrWhileEnabled) {
    KernelTrace::clearObjects();
    int firstQueue = 0;
    int replacementQueue = 0;
    int liveQueue = 0;

    ASSERT_TRUE(KernelTrace::registerObject(&firstQueue, 1, ObjectKind::kQueue, "queue"));
    ASSERT_TRUE(KernelTrace::unregisterObject(&firstQueue));
    EXPECT_FALSE(KernelTrace::registerObject(&replacementQueue, 1, ObjectKind::kQueue, "renamed"));
    EXPECT_TRUE(KernelTrace::registerObject(&replacementQueue, 1, ObjectKind::kQueue, "queue"));
    EXPECT_FALSE(KernelTrace::registerObject(&liveQueue, 2, ObjectKind::kQueue, ""));
    EXPECT_FALSE(
        KernelTrace::registerObject(&liveQueue, 2, ObjectKind::kQueue, "1234567890abcdef"));

    KernelTrace::start();
    KernelTrace::enable();
    EXPECT_FALSE(KernelTrace::registerObject(&liveQueue, 2, ObjectKind::kQueue, "live"));
    TraceBuffer destination(1024);
    ASSERT_EQ(destination.init(), ESP_OK);
    KernelTrace::stopAndFlush(destination);
    KernelTrace::clearObjects();
}

TEST(KernelTraceTest, FlushStablyOrdersBothCoresWithoutMergeStorage) {
    KernelTrace::start();
    KernelTrace::enable();

    sim_trace::currentCoreId = 0;
    ASSERT_TRUE(KernelTrace::record(eventAt(30)));
    TraceEvent coreZeroEqual = eventAt(10);
    coreZeroEqual.arg1 = 100;
    ASSERT_TRUE(KernelTrace::record(coreZeroEqual));
    sim_trace::currentCoreId = 1;
    TraceEvent coreOneEqual = eventAt(10);
    coreOneEqual.arg1 = 200;
    ASSERT_TRUE(KernelTrace::record(coreOneEqual));
    ASSERT_TRUE(KernelTrace::record(eventAt(20)));

    TraceBuffer destination(1024);
    ASSERT_EQ(destination.init(), ESP_OK);
    KernelTrace::stopAndFlush(destination);

    std::array<uint32_t, 4> timestamps{};
    std::array<uint32_t, 4> arguments{};
    size_t index = 0;
    for (auto& timestamp : timestamps) {
        TraceEvent event{};
        ASSERT_TRUE(destination.read(&event));
        timestamp = event.timestamp;
        arguments[index++] = event.arg1;
    }
    EXPECT_EQ((std::array<uint32_t, 4>{10, 10, 20, 30}), timestamps);
    EXPECT_EQ(100U, arguments[0]);
    EXPECT_EQ(200U, arguments[1]);
    EXPECT_EQ(destination.count(), 0U);
}

}  // namespace
}  // namespace domes::trace
