#include "releaseMetadata.hpp"

#include "firmwareVersion.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace domes {

bool isSha256Hex(const char* value) {
    if (!value || std::strlen(value) != kSha256HexLength) {
        return false;
    }

    for (size_t i = 0; i < kSha256HexLength; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

bool formatOtaAssetName(const char* tag, char* output, size_t outputSize) {
    if (!tag || !output || outputSize == 0 || !parseVersion(tag).valid) {
        return false;
    }

    const int length = std::snprintf(output, outputSize, "domes-%s.bin", tag);
    return length >= 0 && static_cast<size_t>(length) < outputSize;
}

bool parseReleaseAssetSize(double value, size_t& output) {
    if (!std::isfinite(value) || value < 1.0 || std::trunc(value) != value) {
        return false;
    }

    // Use an exclusive power-of-two bound because converting SIZE_MAX to
    // double can round it up beyond the range of size_t.
    const double exclusiveUpperBound = std::ldexp(1.0, std::numeric_limits<size_t>::digits);
    if (value >= exclusiveUpperBound) {
        return false;
    }

    output = static_cast<size_t>(value);
    return output != 0;
}

}  // namespace domes
