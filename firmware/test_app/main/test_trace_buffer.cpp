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

}  // namespace
}  // namespace domes::trace
