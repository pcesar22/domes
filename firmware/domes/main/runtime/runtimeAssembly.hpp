#pragma once

#include "config/featureManager.hpp"
#include "esp_err.h"

#include <cstdint>

namespace domes {

class AudioService;
class EspNowService;
class EspNowTransport;
class IAudioDriver;
class IHapticDriver;
class IImuDriver;
class ILedDriver;
class IPlatformIdentity;
class IRandomSource;
class ITouchDriver;
class ImuService;
class InjectableTouchDriver;
class LedService;
class TouchService;

namespace config {
class ModeManager;
}

namespace game {
class GameEngine;
}

namespace infra {
class TaskManager;
}

namespace runtime {

using TouchEventCallback = void (*)(void* context, uint8_t padIndex, uint64_t timestampUs);

struct RuntimeHandles {
    config::FeatureManager* features = nullptr;
    config::ModeManager* modes = nullptr;
    LedService* led = nullptr;
    ImuService* imu = nullptr;
    AudioService* audio = nullptr;
    TouchService* touch = nullptr;
    InjectableTouchDriver* injectableTouch = nullptr;
    game::GameEngine* game = nullptr;
    EspNowService* espNow = nullptr;
};

/** One-shot production service assembly used by both build-selected roots. */
class RuntimeAssembly {
public:
    esp_err_t initFeatureManager(uint32_t supportedMask,
                                 config::FeatureManager::FeatureChangeCallback callback);
    esp_err_t initModeManager();
    esp_err_t initDiagnostics();
    esp_err_t initMemoryProfiler();
    esp_err_t initLedService(ILedDriver& driver);
    esp_err_t initImuService(IImuDriver& driver);
    esp_err_t initAudioService(IAudioDriver& driver);
    esp_err_t connectImuFeedback(IHapticDriver* haptic, AudioService* audio);
    esp_err_t initTouchService(ITouchDriver& driver, TouchEventCallback callback = nullptr,
                               void* callbackContext = nullptr);
    esp_err_t initGameEngine(ITouchDriver& driver);
    esp_err_t prepareEspNowService(EspNowTransport& transport, IPlatformIdentity& identity,
                                   IRandomSource& random);
    esp_err_t startEspNowService(infra::TaskManager& taskManager);
    bool transitionToIdle();

    const RuntimeHandles& handles() const { return handles_; }

private:
    RuntimeHandles handles_;
    bool diagnosticsInitAttempted_ = false;
    bool memoryProfilerInitAttempted_ = false;
    EspNowService* preparedEspNow_ = nullptr;
    bool espNowStartAttempted_ = false;
};

}  // namespace runtime
}  // namespace domes
