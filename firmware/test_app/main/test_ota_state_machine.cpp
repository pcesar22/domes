#include "services/otaStateMachine.hpp"

#include <array>
#include <atomic>

#include <gtest/gtest.h>

namespace domes {
namespace {

TEST(OtaStateMachineTest, StartsFromIdle) {
    std::atomic<OtaState> state{OtaState::kIdle};

    EXPECT_TRUE(tryBeginOtaOperation(state, OtaState::kCheckingVersion));
    EXPECT_EQ(state.load(), OtaState::kCheckingVersion);
}

TEST(OtaStateMachineTest, ErrorStateAllowsRetryWithoutReboot) {
    std::atomic<OtaState> state{OtaState::kError};

    EXPECT_TRUE(tryBeginOtaOperation(state, OtaState::kDownloading));
    EXPECT_EQ(state.load(), OtaState::kDownloading);
}

TEST(OtaStateMachineTest, ActiveOperationCannotBeReplaced) {
    constexpr std::array activeStates{
        OtaState::kCheckingVersion, OtaState::kDownloading, OtaState::kVerifying,
        OtaState::kInstalling,      OtaState::kRebooting,
    };

    for (OtaState activeState : activeStates) {
        std::atomic<OtaState> state{activeState};
        EXPECT_FALSE(tryBeginOtaOperation(state, OtaState::kDownloading));
        EXPECT_EQ(state.load(), activeState);
    }
}

}  // namespace
}  // namespace domes
