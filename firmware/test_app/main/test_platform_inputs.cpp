#include "platform/qemu/deterministicPlatformInputs.hpp"
#include "services/roundTokenSequence.hpp"

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

TEST(PlatformInputsTest, FixedIdentityReturnsExactSixBytes) {
    constexpr domes::PlatformIdentity kExpected = {0x02, 0x44, 0x4f, 0x4d, 0x45, 0x53};
    domes::platform::FixedPlatformIdentity source(kExpected);
    domes::PlatformIdentity actual = {};

    ASSERT_EQ(source.read(actual), ESP_OK);
    EXPECT_EQ(actual, kExpected);
}

TEST(PlatformInputsTest, RecordedRandomSourceConsumesFiniteSequenceAndFailsClosed) {
    constexpr std::array<uint32_t, 3> kValues = {0U, 0x12345678U, UINT32_MAX};
    domes::platform::RecordedRandomSource source(kValues);
    uint32_t value = 0;

    ASSERT_EQ(source.nextU32(value), ESP_OK);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(source.nextU32(value), ESP_OK);
    EXPECT_EQ(value, 0x12345678U);
    ASSERT_EQ(source.nextU32(value), ESP_OK);
    EXPECT_EQ(value, UINT32_MAX);
    EXPECT_EQ(source.consumed(), kValues.size());
    EXPECT_EQ(source.remaining(), 0U);
    EXPECT_EQ(source.nextU32(value), ESP_ERR_INVALID_STATE);
}

TEST(PlatformInputsTest, RoundTokenSequencePreservesSeedPlusOneAndSkipsZero) {
    domes::RoundTokenSequence sequence;
    sequence.reset(41U);
    EXPECT_EQ(sequence.next(), 42U);

    sequence.reset(UINT32_MAX);
    EXPECT_EQ(sequence.next(), 1U);
    EXPECT_EQ(sequence.next(), 2U);
}

}  // namespace
