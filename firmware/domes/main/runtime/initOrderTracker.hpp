#pragma once

#include "infra/taskTopology.hpp"

#include <cstddef>
#include <cstring>

namespace domes::runtime {

/** Fail-closed consumer of the generated runtime-profile initialization order. */
class InitOrderTracker {
public:
    bool advance(const char* stage) {
        if (!stage || next_ >= runtime_profile::kInitOrder.size() ||
            std::strcmp(stage, runtime_profile::kInitOrder[next_]) != 0) {
            return false;
        }
        ++next_;
        return true;
    }

    bool complete() const { return next_ == runtime_profile::kInitOrder.size(); }

    const char* expected() const {
        return next_ < runtime_profile::kInitOrder.size() ? runtime_profile::kInitOrder[next_]
                                                          : nullptr;
    }

private:
    size_t next_ = 0;
};

}  // namespace domes::runtime
