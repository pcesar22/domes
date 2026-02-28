#pragma once

/**
 * @file injectableTouchDriver.hpp
 * @brief Decorator that adds touch injection to any ITouchDriver
 *
 * Sits between the hardware TouchDriver and GameEngine.
 * isTouched() returns (injected || real) so injected touches
 * exercise the full game pipeline identically to real touches.
 *
 * Thread safety: injection is via atomic bitmask (set from Core 0
 * config/espnow tasks, read from Core 1 game_tick).
 */

#include "interfaces/iTouchDriver.hpp"

#include <atomic>
#include <cstdint>

namespace domes {

class InjectableTouchDriver : public ITouchDriver {
public:
    explicit InjectableTouchDriver(ITouchDriver& real) : real_(real) {}

    // -- ITouchDriver interface (delegates to real driver) --

    esp_err_t init() override { return real_.init(); }
    esp_err_t update() override {
        // Tick down injection timers (CAS to avoid TOCTOU with cross-core injectTouch)
        for (uint8_t i = 0; i < kMaxPads; ++i) {
            uint8_t cur = injectedTicks_[i].load(std::memory_order_relaxed);
            while (cur > 0) {
                if (injectedTicks_[i].compare_exchange_weak(
                        cur, cur - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    break;
                }
                // cur is updated by compare_exchange_weak on failure
            }
        }
        return real_.update();
    }

    bool isTouched(uint8_t padIndex) const override {
        if (padIndex >= kMaxPads) return false;
        bool injected = injectedTicks_[padIndex].load(std::memory_order_relaxed) > 0;
        return injected || real_.isTouched(padIndex);
    }

    TouchPadState getPadState(uint8_t padIndex) const override {
        TouchPadState state = real_.getPadState(padIndex);
        if (padIndex < kMaxPads &&
            injectedTicks_[padIndex].load(std::memory_order_relaxed) > 0) {
            state.touched = true;
        }
        return state;
    }

    uint8_t getPadCount() const override { return real_.getPadCount(); }
    esp_err_t calibrate() override { return real_.calibrate(); }

    // -- Injection API --

    /**
     * @brief Inject a touch on the given pad
     *
     * The pad will report as touched for @p durationTicks calls to update().
     * At 100 Hz game tick, 10 ticks ≈ 100 ms.
     *
     * @param padIndex  Pad to inject (0-3)
     * @param durationTicks  How many update() ticks the injection lasts (default 10)
     */
    void injectTouch(uint8_t padIndex, uint8_t durationTicks = 10) {
        if (padIndex < kMaxPads) {
            injectedTicks_[padIndex].store(durationTicks, std::memory_order_relaxed);
        }
    }

    /// Clear all active injections
    void clearInjection() {
        for (auto& t : injectedTicks_) {
            t.store(0, std::memory_order_relaxed);
        }
    }

private:
    static constexpr uint8_t kMaxPads = 4;

    ITouchDriver& real_;
    std::atomic<uint8_t> injectedTicks_[kMaxPads] = {};
};

}  // namespace domes
