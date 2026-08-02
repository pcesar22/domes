#pragma once

/**
 * @file otaSessionCoordinator.hpp
 * @brief Device-wide ownership guard for flash-writing OTA sessions
 */

#include <atomic>
#include <cstdint>

namespace domes {

/**
 * @brief Serializes OTA writers across UART, BLE, and HTTPS transports.
 *
 * ESP-IDF permits more than one OTA handle to target the same update partition.
 * Each OTA implementation therefore acquires this process-wide lease before it
 * calls an API that can erase or write the inactive application partition.
 */
class OtaSessionCoordinator {
public:
    static constexpr int64_t kInactivityTimeoutUs = 15'000'000;

    static bool tryAcquire(const void* owner) {
        if (owner == nullptr) {
            return false;
        }

        uintptr_t expected = 0;
        return owner_.compare_exchange_strong(expected, reinterpret_cast<uintptr_t>(owner),
                                              std::memory_order_acq_rel, std::memory_order_acquire);
    }

    static void release(const void* owner) {
        if (owner == nullptr) {
            return;
        }

        uintptr_t expected = reinterpret_cast<uintptr_t>(owner);
        owner_.compare_exchange_strong(expected, 0, std::memory_order_acq_rel,
                                       std::memory_order_acquire);
    }

    static bool isOwnedBy(const void* owner) {
        return owner != nullptr &&
               owner_.load(std::memory_order_acquire) == reinterpret_cast<uintptr_t>(owner);
    }

    static bool isBusy() { return owner_.load(std::memory_order_acquire) != 0; }

    static constexpr bool hasTimedOut(int64_t lastActivityUs, int64_t nowUs) {
        return nowUs >= lastActivityUs && nowUs - lastActivityUs >= kInactivityTimeoutUs;
    }

private:
    inline static std::atomic<uintptr_t> owner_{0};
};

}  // namespace domes
