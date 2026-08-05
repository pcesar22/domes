#pragma once

#include <cstdint>

namespace domes {

/** Generates nonzero round tokens while preserving the existing seed-plus-one sequence. */
class RoundTokenSequence {
public:
    constexpr void reset(uint32_t seed) { current_ = seed; }

    constexpr uint32_t next() {
        do {
            ++current_;
        } while (current_ == 0);
        return current_;
    }

private:
    uint32_t current_ = 0;
};

}  // namespace domes
