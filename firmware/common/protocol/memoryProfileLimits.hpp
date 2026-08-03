#pragma once

#include "config.pb.h"

#include "protocol/frameCodec.hpp"

#include <cstddef>
#include <type_traits>

namespace domes::memory_profile {

/// Status byte prepended to config response protobufs.
constexpr size_t kStatusSize = 1;

/// Generated nanopb capacity for historical heap samples.
constexpr size_t kMaxSamples =
    std::extent_v<decltype(domes_config_GetMemoryProfileResponse::samples)>;

/// Worst-case config payload: status byte followed by the encoded response.
constexpr size_t kMaxPayloadSize = kStatusSize + domes_config_GetMemoryProfileResponse_size;

static_assert(kMaxPayloadSize <= domes::kMaxPayloadSize,
              "Memory profile response exceeds the shared frame payload limit");

}  // namespace domes::memory_profile
