#pragma once

#include "esp_err.h"

#include <cstdint>

namespace domes {

/** Supplies bounded platform randomness without owning a clock or scheduler. */
class IRandomSource {
public:
    virtual ~IRandomSource() = default;

    /** Return the next value, or fail when a deterministic source is exhausted. */
    virtual esp_err_t nextU32(uint32_t& value) = 0;
};

}  // namespace domes
