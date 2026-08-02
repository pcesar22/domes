#pragma once

#include "interfaces/iOtaManager.hpp"

#include <atomic>

namespace domes {

/**
 * @brief Begin an OTA operation when the manager is idle or recovering from an error.
 *
 * `kError` records the outcome of the previous attempt, but it must not require a
 * reboot before another version check or download can be attempted.
 *
 * @param state Shared OTA state.
 * @param activeState State for the operation being started.
 * @return true when the operation acquired the state machine.
 */
inline bool tryBeginOtaOperation(std::atomic<OtaState>& state, OtaState activeState) {
    OtaState current = state.load(std::memory_order_acquire);
    while (current == OtaState::kIdle || current == OtaState::kError) {
        if (state.compare_exchange_weak(current, activeState, std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

}  // namespace domes
