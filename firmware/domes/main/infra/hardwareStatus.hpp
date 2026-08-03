#pragma once

#include <atomic>
#include <cstdint>

namespace domes::infra {

enum class HardwareSubsystem : uint8_t {
    kLed = 0,
    kImu,
    kHaptic,
    kAudio,
    kTouch,
};

class HardwareStatus {
public:
    static void markReady(HardwareSubsystem subsystem) {
        readyMask_.fetch_or(1U << static_cast<uint8_t>(subsystem), std::memory_order_release);
    }

    static bool isReady(HardwareSubsystem subsystem) {
        return (readyMask_.load(std::memory_order_acquire) &
                (1U << static_cast<uint8_t>(subsystem))) != 0;
    }

private:
    inline static std::atomic<uint32_t> readyMask_{0};
};

}  // namespace domes::infra
