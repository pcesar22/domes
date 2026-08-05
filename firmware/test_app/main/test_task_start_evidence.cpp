#include "freertos/task.h"
#include "infra/taskStartEvidence.hpp"

#include <gtest/gtest.h>

namespace {

TEST(TaskStartEvidenceTest, RecordsDuplicateEntryAndExclusiveCoreMasks) {
    using domes::infra::TaskStartEvidence;

    sim_trace::currentCoreId = 0;
    TaskStartEvidence::markStarted(0x01U);
    sim_trace::currentCoreId = 1;
    TaskStartEvidence::markStarted(0x02U);

    EXPECT_EQ(TaskStartEvidence::startedMask(), 0x03U);
    EXPECT_EQ(TaskStartEvidence::duplicateMask(), 0U);
    EXPECT_EQ(TaskStartEvidence::coreMask(0), 0x01U);
    EXPECT_EQ(TaskStartEvidence::coreMask(1), 0x02U);

    TaskStartEvidence::markStarted(0x02U);
    EXPECT_EQ(TaskStartEvidence::duplicateMask(), 0x02U);
}

}  // namespace
