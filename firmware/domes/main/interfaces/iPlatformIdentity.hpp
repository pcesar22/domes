#pragma once

#include "esp_err.h"

#include <array>
#include <cstdint>

namespace domes {

using PlatformIdentity = std::array<uint8_t, 6>;

/** Provides the immutable six-byte identity used by pod-to-pod protocols. */
class IPlatformIdentity {
public:
    virtual ~IPlatformIdentity() = default;

    /** Read the platform identity, failing when no authoritative identity is available. */
    virtual esp_err_t read(PlatformIdentity& identity) const = 0;
};

}  // namespace domes
