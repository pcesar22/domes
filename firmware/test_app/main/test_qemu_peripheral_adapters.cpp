#include "platform/qemu/qemuPeripheralAdapters.hpp"

#include <gtest/gtest.h>

namespace {

TEST(QemuHapticDriverTest, EnforcesPhysicalEffectAndIntensityBounds) {
    domes::platform::QemuAdapterEvidence evidence;
    domes::platform::QemuHapticDriver haptic(evidence);

    EXPECT_EQ(haptic.playEffect(1), ESP_ERR_INVALID_STATE);
    ASSERT_EQ(haptic.init(), ESP_OK);
    haptic.setIntensity(255);
    EXPECT_EQ(haptic.getIntensity(), 100);
    EXPECT_EQ(haptic.playEffect(0), ESP_ERR_INVALID_ARG);
    EXPECT_EQ(haptic.playEffect(124), ESP_ERR_INVALID_ARG);
    EXPECT_EQ(haptic.playEffect(1), ESP_OK);
    EXPECT_EQ(haptic.playEffect(123), ESP_OK);

    const uint8_t invalidSequence[] = {1, 124};
    EXPECT_EQ(haptic.playSequence(invalidSequence, 2), ESP_ERR_INVALID_ARG);
}

}  // namespace
