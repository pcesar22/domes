#include "traceBuffer.hpp"

namespace domes::trace {

bool TraceBuffer::tryClaimDumpSnapshot(const void* owner) {
    if (!initialized_.load(std::memory_order_acquire) || owner == nullptr) {
        return false;
    }

    const uintptr_t requestedOwner = reinterpret_cast<uintptr_t>(owner);
    uintptr_t currentOwner = dumpOwner_.load(std::memory_order_acquire);
    if (currentOwner == requestedOwner) {
        return true;
    }

    currentOwner = 0;
    return dumpOwner_.compare_exchange_strong(currentOwner, requestedOwner,
                                              std::memory_order_acq_rel, std::memory_order_acquire);
}

size_t TraceBuffer::captureDumpSnapshot(const void* owner) {
    if (dumpOwner_.load(std::memory_order_acquire) != reinterpret_cast<uintptr_t>(owner)) {
        return 0;
    }
    if (dumpSnapshotCount_ != 0) {
        return dumpSnapshotCount_;
    }

    while (dumpSnapshotCount_ < dumpSnapshot_.size()) {
        const TraceEvent* event = acquire(0);
        if (event == nullptr) {
            break;
        }
        dumpSnapshot_[dumpSnapshotCount_++] = event;
    }
    return dumpSnapshotCount_;
}

size_t TraceBuffer::dumpSnapshotCount(const void* owner) const {
    if (dumpOwner_.load(std::memory_order_acquire) != reinterpret_cast<uintptr_t>(owner)) {
        return 0;
    }
    return dumpSnapshotCount_;
}

const TraceEvent* TraceBuffer::dumpSnapshotEvent(const void* owner, size_t index) const {
    if (dumpOwner_.load(std::memory_order_acquire) != reinterpret_cast<uintptr_t>(owner) ||
        index >= dumpSnapshotCount_) {
        return nullptr;
    }
    return dumpSnapshot_[index];
}

void TraceBuffer::releaseDumpSnapshotEvents(const void* owner) {
    if (dumpOwner_.load(std::memory_order_acquire) != reinterpret_cast<uintptr_t>(owner)) {
        return;
    }

    for (size_t i = 0; i < dumpSnapshotCount_; ++i) {
        release(dumpSnapshot_[i]);
        dumpSnapshot_[i] = nullptr;
    }
    dumpSnapshotCount_ = 0;
}

bool TraceBuffer::completeDumpSnapshot(const void* owner) {
    const uintptr_t requestedOwner = reinterpret_cast<uintptr_t>(owner);
    if (owner == nullptr || dumpOwner_.load(std::memory_order_acquire) != requestedOwner) {
        return false;
    }

    releaseDumpSnapshotEvents(owner);
    dumpOwner_.store(0, std::memory_order_release);
    return true;
}

bool TraceBuffer::clearDumpSnapshot(const void* owner) {
    const uintptr_t requestedOwner = reinterpret_cast<uintptr_t>(owner);
    if (owner == nullptr || dumpOwner_.load(std::memory_order_acquire) != requestedOwner) {
        return false;
    }

    releaseDumpSnapshotEvents(owner);
    clear();
    dumpOwner_.store(0, std::memory_order_release);
    return true;
}

}  // namespace domes::trace
