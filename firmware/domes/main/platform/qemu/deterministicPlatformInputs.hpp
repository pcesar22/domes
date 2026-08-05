#pragma once

#include "interfaces/iPlatformIdentity.hpp"
#include "interfaces/iRandomSource.hpp"

#include <cstddef>
#include <span>

namespace domes::platform {

class FixedPlatformIdentity final : public IPlatformIdentity {
public:
    explicit constexpr FixedPlatformIdentity(PlatformIdentity identity) : identity_(identity) {}

    esp_err_t read(PlatformIdentity& identity) const override;

private:
    PlatformIdentity identity_;
};

/** Finite recorded input. Exhaustion is an evidence failure, never implicit PRNG fallback. */
class RecordedRandomSource final : public IRandomSource {
public:
    explicit constexpr RecordedRandomSource(std::span<const uint32_t> values) : values_(values) {}

    esp_err_t nextU32(uint32_t& value) override;

    constexpr size_t consumed() const { return index_; }
    constexpr size_t remaining() const { return values_.size() - index_; }

private:
    std::span<const uint32_t> values_;
    size_t index_ = 0;
};

}  // namespace domes::platform
