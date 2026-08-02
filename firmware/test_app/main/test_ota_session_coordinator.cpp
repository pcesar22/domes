#include "services/otaSessionCoordinator.hpp"

#include <gtest/gtest.h>

namespace {

TEST(OtaSessionCoordinator, AllowsOnlyOneOwner) {
    int uartOwner = 0;
    int bleOwner = 0;

    ASSERT_TRUE(domes::OtaSessionCoordinator::tryAcquire(&uartOwner));
    EXPECT_TRUE(domes::OtaSessionCoordinator::isBusy());
    EXPECT_TRUE(domes::OtaSessionCoordinator::isOwnedBy(&uartOwner));
    EXPECT_FALSE(domes::OtaSessionCoordinator::tryAcquire(&bleOwner));

    domes::OtaSessionCoordinator::release(&uartOwner);
    EXPECT_FALSE(domes::OtaSessionCoordinator::isBusy());
}

TEST(OtaSessionCoordinator, NonOwnerCannotReleaseLease) {
    int httpsOwner = 0;
    int otherOwner = 0;

    ASSERT_TRUE(domes::OtaSessionCoordinator::tryAcquire(&httpsOwner));
    domes::OtaSessionCoordinator::release(&otherOwner);
    EXPECT_TRUE(domes::OtaSessionCoordinator::isOwnedBy(&httpsOwner));

    domes::OtaSessionCoordinator::release(&httpsOwner);
    EXPECT_FALSE(domes::OtaSessionCoordinator::isBusy());
}

TEST(OtaSessionCoordinator, InactivityTimeoutUsesInclusiveBoundary) {
    constexpr int64_t lastActivityUs = 123;

    EXPECT_FALSE(domes::OtaSessionCoordinator::hasTimedOut(
        lastActivityUs, lastActivityUs + domes::OtaSessionCoordinator::kInactivityTimeoutUs - 1));
    EXPECT_TRUE(domes::OtaSessionCoordinator::hasTimedOut(
        lastActivityUs, lastActivityUs + domes::OtaSessionCoordinator::kInactivityTimeoutUs));
    EXPECT_FALSE(domes::OtaSessionCoordinator::hasTimedOut(lastActivityUs, lastActivityUs - 1));
}

}  // namespace
