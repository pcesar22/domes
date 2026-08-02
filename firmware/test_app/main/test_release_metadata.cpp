#include "services/releaseMetadata.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include <gtest/gtest.h>

namespace domes::test {

TEST(ReleaseMetadata, AcceptsExactly64HexCharacters) {
    const std::string lower(kSha256HexLength, 'a');
    const std::string upper(kSha256HexLength, 'F');

    EXPECT_TRUE(isSha256Hex(lower.c_str()));
    EXPECT_TRUE(isSha256Hex(upper.c_str()));
}

TEST(ReleaseMetadata, RejectsMissingMalformedOrWrongLengthDigest) {
    std::string shortDigest(kSha256HexLength - 1, '0');
    std::string longDigest(kSha256HexLength + 1, '0');
    std::string malformed(kSha256HexLength, '0');
    malformed[17] = 'x';

    EXPECT_FALSE(isSha256Hex(nullptr));
    EXPECT_FALSE(isSha256Hex(shortDigest.c_str()));
    EXPECT_FALSE(isSha256Hex(longDigest.c_str()));
    EXPECT_FALSE(isSha256Hex(malformed.c_str()));
}

TEST(ReleaseMetadata, FormatsTheExactVersionedOtaAssetName) {
    std::array<char, 64> output{};

    ASSERT_TRUE(formatOtaAssetName("v1.2.3", output.data(), output.size()));
    EXPECT_STREQ(output.data(), "domes-v1.2.3.bin");
}

TEST(ReleaseMetadata, RejectsInvalidTagAndInsufficientOutputBuffer) {
    std::array<char, 64> output{};
    std::array<char, 8> tooSmall{};

    EXPECT_FALSE(formatOtaAssetName(nullptr, output.data(), output.size()));
    EXPECT_FALSE(formatOtaAssetName("v1.2.3-rc.1", output.data(), output.size()));
    EXPECT_FALSE(formatOtaAssetName("v1.2.3", nullptr, output.size()));
    EXPECT_FALSE(formatOtaAssetName("v1.2.3", output.data(), 0));
    EXPECT_FALSE(formatOtaAssetName("v1.2.3", tooSmall.data(), tooSmall.size()));
}

TEST(ReleaseMetadata, AcceptsPositiveIntegralAssetSize) {
    size_t output = 0;

    EXPECT_TRUE(parseReleaseAssetSize(1.0, output));
    EXPECT_EQ(1u, output);
    EXPECT_TRUE(parseReleaseAssetSize(123456.0, output));
    EXPECT_EQ(123456u, output);
}

TEST(ReleaseMetadata, RejectsMalformedAssetSizes) {
    size_t output = 77;

    EXPECT_FALSE(parseReleaseAssetSize(0.0, output));
    EXPECT_FALSE(parseReleaseAssetSize(-1.0, output));
    EXPECT_FALSE(parseReleaseAssetSize(1.5, output));
    EXPECT_FALSE(parseReleaseAssetSize(std::numeric_limits<double>::infinity(), output));
    EXPECT_FALSE(parseReleaseAssetSize(std::numeric_limits<double>::quiet_NaN(), output));
}

TEST(ReleaseMetadata, RejectsAssetSizeOutsideSizeT) {
    size_t output = 0;
    const double exclusiveUpperBound = std::ldexp(1.0, std::numeric_limits<size_t>::digits);

    EXPECT_FALSE(parseReleaseAssetSize(exclusiveUpperBound, output));
}

}  // namespace domes::test
