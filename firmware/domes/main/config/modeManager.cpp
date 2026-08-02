#include "modeManager.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <array>

namespace {
constexpr const char* kTag = "mode_mgr";

// Feature bit helper: bit N = Feature ID N
constexpr uint32_t bit(uint8_t n) {
    return 1u << n;
}

// Feature masks per mode (bit N corresponds to Feature enum value N).
// WiFi client connectivity is user/build policy and is deliberately preserved
// across mode transitions; the remaining features are mode-controlled.
constexpr uint32_t kBootingMask = 0;                                          // All off
constexpr uint32_t kIdleMask = bit(1) | bit(2);                               // LED + BLE
constexpr uint32_t kTriageMask = bit(1) | bit(2) | bit(5) | bit(6) | bit(7);  // Feedback/input
constexpr uint32_t kConnectedMask = bit(1) | bit(2) | bit(4) | bit(5) | bit(6) | bit(7);
constexpr uint32_t kGameMask =
    bit(1) | bit(2) | bit(4) | bit(5) | bit(6) | bit(7);  // Same as Connected
constexpr uint32_t kErrorMask = bit(1) | bit(2);          // LED + BLE only
constexpr uint32_t kModeControlledMask = bit(1) | bit(2) | bit(4) | bit(5) | bit(6) | bit(7);

constexpr uint32_t kModeMasks[] = {
    kBootingMask,    // kBooting (0)
    kIdleMask,       // kIdle (1)
    kTriageMask,     // kTriage (2)
    kConnectedMask,  // kConnected (3)
    kGameMask,       // kGame (4)
    kErrorMask,      // kError (5)
};

static_assert(sizeof(kModeMasks) / sizeof(kModeMasks[0]) == 6, "Mode mask count mismatch");

}  // anonymous namespace

namespace domes::config {

const char* systemModeToString(SystemMode mode) {
    switch (mode) {
        case SystemMode::kBooting:
            return "BOOTING";
        case SystemMode::kIdle:
            return "IDLE";
        case SystemMode::kTriage:
            return "TRIAGE";
        case SystemMode::kConnected:
            return "CONNECTED";
        case SystemMode::kGame:
            return "GAME";
        case SystemMode::kError:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

ModeManager::ModeManager(FeatureManager& features)
    : features_(features),
      currentMode_(static_cast<uint8_t>(SystemMode::kBooting)),
      modeEnteredAt_(esp_timer_get_time()),
      lastActivityAt_(esp_timer_get_time()) {}

SystemMode ModeManager::currentMode() const {
    return static_cast<SystemMode>(currentMode_.load(std::memory_order_acquire));
}

bool ModeManager::transitionTo(SystemMode newMode) {
    TransitionRecord record{};
    bool changed = false;
    ModeTransitionCallback callback;
    {
        utils::MutexGuard guard(transitionMutex_);
        if (!transitionToLocked(newMode, record, changed)) {
            return false;
        }
        callback = transitionCb_;
    }

    if (changed) {
        logTransition(record);
        if (callback) {
            callback(record.from, record.to);
        }
    }
    return true;
}

bool ModeManager::transitionToPeerGame() {
    std::array<TransitionRecord, 3> records{};
    size_t recordCount = 0;
    ModeTransitionCallback callback;
    {
        utils::MutexGuard guard(transitionMutex_);
        SystemMode mode = static_cast<SystemMode>(currentMode_.load(std::memory_order_relaxed));
        if (mode == SystemMode::kGame) {
            return true;
        }

        const auto apply = [&](SystemMode target) {
            bool changed = false;
            if (!transitionToLocked(target, records[recordCount], changed)) {
                return false;
            }
            if (changed) {
                ++recordCount;
            }
            return true;
        };

        if (mode == SystemMode::kBooting || mode == SystemMode::kError) {
            if (!apply(SystemMode::kIdle)) {
                return false;
            }
            mode = SystemMode::kIdle;
        }

        if (mode == SystemMode::kIdle || mode == SystemMode::kTriage) {
            if (!apply(SystemMode::kConnected)) {
                return false;
            }
            mode = SystemMode::kConnected;
        }

        if (mode != SystemMode::kConnected || !apply(SystemMode::kGame)) {
            return false;
        }
        callback = transitionCb_;
    }

    for (size_t i = 0; i < recordCount; ++i) {
        logTransition(records[i]);
        if (callback) {
            callback(records[i].from, records[i].to);
        }
    }
    return true;
}

uint32_t ModeManager::timeInModeMs() const {
    int64_t now = esp_timer_get_time();
    int64_t entered = modeEnteredAt_.load(std::memory_order_acquire);
    return static_cast<uint32_t>((now - entered) / 1000);  // us -> ms
}

void ModeManager::resetActivityTimer() {
    lastActivityAt_.store(esp_timer_get_time(), std::memory_order_release);
}

void ModeManager::onTransition(ModeTransitionCallback callback) {
    utils::MutexGuard guard(transitionMutex_);
    transitionCb_ = std::move(callback);
}

void ModeManager::tick() {
    TransitionRecord record{};
    bool changed = false;
    ModeTransitionCallback callback;
    {
        utils::MutexGuard guard(transitionMutex_);
        const SystemMode mode =
            static_cast<SystemMode>(currentMode_.load(std::memory_order_relaxed));
        const int64_t now = esp_timer_get_time();
        SystemMode target = mode;

        if (mode == SystemMode::kTriage &&
            (now - lastActivityAt_.load(std::memory_order_relaxed)) > kTriageTimeoutUs) {
            ESP_LOGI(kTag, "Triage timeout (30s idle), returning to IDLE");
            target = SystemMode::kIdle;
        } else if (mode == SystemMode::kError &&
                   (now - modeEnteredAt_.load(std::memory_order_relaxed)) > kErrorRecoveryUs) {
            ESP_LOGI(kTag, "Error recovery timeout (10s), returning to IDLE");
            target = SystemMode::kIdle;
        } else if (mode == SystemMode::kGame &&
                   (now - modeEnteredAt_.load(std::memory_order_relaxed)) > kGameTimeoutUs) {
            target = static_cast<SystemMode>(gameEnteredFrom_.load(std::memory_order_relaxed));
            ESP_LOGW(kTag, "Game timeout (5min), returning to %s", systemModeToString(target));
        }

        if (target == mode) {
            return;
        }
        if (!transitionToLocked(target, record, changed)) {
            return;
        }
        callback = transitionCb_;
    }

    if (changed) {
        logTransition(record);
        if (callback) {
            callback(record.from, record.to);
        }
    }
}

bool ModeManager::transitionToLocked(SystemMode newMode, TransitionRecord& record, bool& changed) {
    const SystemMode oldMode =
        static_cast<SystemMode>(currentMode_.load(std::memory_order_relaxed));
    record = {oldMode, newMode};
    changed = oldMode != newMode;
    if (!changed) {
        return true;
    }

    if (!isValidTransition(oldMode, newMode)) {
        ESP_LOGW(kTag, "Invalid transition: %s -> %s", systemModeToString(oldMode),
                 systemModeToString(newMode));
        return false;
    }

    if (newMode == SystemMode::kGame) {
        gameEnteredFrom_.store(static_cast<uint8_t>(oldMode), std::memory_order_relaxed);
    }
    applyFeatureMask(newMode);

    const int64_t now = esp_timer_get_time();
    currentMode_.store(static_cast<uint8_t>(newMode), std::memory_order_release);
    modeEnteredAt_.store(now, std::memory_order_release);
    lastActivityAt_.store(now, std::memory_order_release);
    return true;
}

void ModeManager::logTransition(const TransitionRecord& record) {
    ESP_LOGI(kTag, "Mode: %s -> %s (mask=0x%08lx)", systemModeToString(record.from),
             systemModeToString(record.to),
             static_cast<unsigned long>(featureMaskForMode(record.to)));
}

uint32_t ModeManager::featureMaskForMode(SystemMode mode) {
    auto idx = static_cast<size_t>(mode);
    if (idx < sizeof(kModeMasks) / sizeof(kModeMasks[0])) {
        return kModeMasks[idx];
    }
    return 0;
}

bool ModeManager::isValidTransition(SystemMode from, SystemMode to) const {
    // Any mode can transition to ERROR
    if (to == SystemMode::kError)
        return true;

    // Any mode can transition to IDLE (for recovery/reset)
    if (to == SystemMode::kIdle)
        return true;

    switch (from) {
        case SystemMode::kBooting:
            return to == SystemMode::kIdle;

        case SystemMode::kIdle:
            return to == SystemMode::kTriage || to == SystemMode::kConnected ||
                   to == SystemMode::kGame;  // solo drill

        case SystemMode::kTriage:
            return to == SystemMode::kConnected;

        case SystemMode::kConnected:
            return to == SystemMode::kTriage || to == SystemMode::kGame;

        case SystemMode::kGame:
            return to == SystemMode::kConnected;

        case SystemMode::kError:
            return to == SystemMode::kIdle;

        default:
            return false;
    }
}

void ModeManager::applyFeatureMask(SystemMode mode) {
    const uint32_t current = features_.getMask();
    const uint32_t modeMask = featureMaskForMode(mode);
    features_.setMask((current & ~kModeControlledMask) | (modeMask & kModeControlledMask));
}

}  // namespace domes::config
