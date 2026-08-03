#pragma once

#include <cstddef>

namespace domes {

inline constexpr size_t kSha256HexLength = 64;

bool isSha256Hex(const char* value);
bool formatOtaAssetName(const char* tag, char* output, size_t outputSize);
bool parseReleaseAssetSize(double value, size_t& output);

}  // namespace domes
