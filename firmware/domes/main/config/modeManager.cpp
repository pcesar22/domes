#include "modeManager.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace {
constexpr const char* kTag = "mode_mgr";

// Feature bit helper: bit N = Feature ID N
constexpr uint32_t bit(uint8_t n) { return 1u << n; }

// Feature masks per mode (bit N corresponds to Feature enum value N)
// Feature IDs: 1=LED, 2=BLE, 3=WiFi, 4=ESP-NOW, 5=Touch, 6=Haptic, 7=Audio
constexpr uint32_t kBootingMask   = 0;                                              // All off
constexpr uint32_t kIdleMask      = bit(1) | bit(2);                                // LED + BLE
constexpr uint32_t kTriageMask    = bit(1) | bit(2) | bit(3) | bit(5) | bit(6) | bit(7); // All except ESP-NOW
constexpr uint32_t kConnectedMask = bit(1) | bit(2) | bit(4) | bit(5) | bit(6) | bit(7); // All except WiFi
constexpr uint32_t kGameMask      = bit(1) | bit(2) | bit(4) | bit(5) | bit(6) | bit(7); // Same as Connected
constexpr uint32_t kErrorMask     = bit(1) | bit(2);                                // LED + BLE only

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
        case SystemMode::kBooting:   return "BOOTING";
        case SystemMode::kIdle:      return "IDLE";
        case SystemMode::kTriage:    return "TRIAGE";
        case SystemMode::kConnected: return "CONNECTED";
        case SystemMode::kGame:      return "GAME";
        case SystemMode::kError:     return "ERROR";
        default:                     return "UNKNOWN";
    }
}

ModeManager::ModeManager(FeatureManager& features)
    : features_(features)
    , currentMode_(static_cast<uint8_t>(SystemMode::kBooting))
    , modeEnteredAt_(esp_timer_get_time())
    , lastActivityAt_(esp_timer_get_time()) {
}

SystemMode ModeManager::currentMode() const {
    return static_cast<SystemMode>(currentMode_.load(std::memory_order_acquire));
}

bool ModeManager::transitionTo(SystemMode newMode) {
    SystemMode oldMode = currentMode();

    if (oldMode == newMode) {
        return true;  // Already in target mode
    }

    if (!isValidTransition(oldMode, newMode)) {
        ESP_LOGW(kTag, "Invalid transition: %s -> %s",
                 systemModeToString(oldMode), systemModeToString(newMode));
        return false;
    }

    // Apply feature mask first, then update mode
    applyFeatureMask(newMode);

    int64_t now = esp_timer_get_time();
    currentMode_.store(static_cast<uint8_t>(newMode), std::memory_order_release);
    modeEnteredAt_.store(now, std::memory_order_release);
    lastActivityAt_.store(now, std::memory_order_release);

    ESP_LOGI(kTag, "Mode: %s -> %s (mask=0x%08lx)",
             systemModeToString(oldMode),
             systemModeToString(newMode),
             static_cast<unsigned long>(featureMaskForMode(newMode)));

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

void ModeManager::tick() {
    SystemMode mode = currentMode();
    int64_t now = esp_timer_get_time();

    switch (mode) {
        case SystemMode::kTriage: {
            int64_t lastActivity = lastActivityAt_.load(std::memory_order_acquire);
            if ((now - lastActivity) > kTriageTimeoutUs) {
                ESP_LOGI(kTag, "Triage timeout (30s idle), returning to IDLE");
                transitionTo(SystemMode::kIdle);
            }
            break;
        }

        case SystemMode::kError: {
            int64_t entered = modeEnteredAt_.load(std::memory_order_acquire);
            if ((now - entered) > kErrorRecoveryUs) {
                ESP_LOGI(kTag, "Error recovery timeout (10s), returning to IDLE");
                transitionTo(SystemMode::kIdle);
            }
            break;
        }

        case SystemMode::kGame: {
            int64_t entered = modeEnteredAt_.load(std::memory_order_acquire);
            if ((now - entered) > kGameTimeoutUs) {
                ESP_LOGW(kTag, "Game timeout (5min), returning to CONNECTED");
                transitionTo(SystemMode::kConnected);
            }
            break;
        }

        default:
            break;  // No timeout for BOOTING, IDLE, CONNECTED
    }
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
    if (to == SystemMode::kError) return true;

    // Any mode can transition to IDLE (for recovery/reset)
    if (to == SystemMode::kIdle) return true;

    switch (from) {
        case SystemMode::kBooting:
            return to == SystemMode::kIdle;

        case SystemMode::kIdle:
            return to == SystemMode::kTriage || to == SystemMode::kConnected;

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
    uint32_t mask = featureMaskForMode(mode);
    features_.setMask(mask);
}

}  // namespace domes::config
