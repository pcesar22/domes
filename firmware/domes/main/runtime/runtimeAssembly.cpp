#include "runtimeAssembly.hpp"

#include "config/modeManager.hpp"
#include "drivers/injectableTouchDriver.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game/gameEngine.hpp"
#include "infra/diagnostics.hpp"
#include "infra/memoryProfiler.hpp"
#include "infra/taskStartEvidence.hpp"
#include "infra/taskTopology.hpp"
#include "interfaces/iAudioDriver.hpp"
#include "interfaces/iHapticDriver.hpp"
#include "interfaces/iImuDriver.hpp"
#include "interfaces/iLedDriver.hpp"
#include "interfaces/iTouchDriver.hpp"
#include "services/audioService.hpp"
#include "services/imuService.hpp"
#include "services/ledService.hpp"
#include "services/touchService.hpp"

#include <utility>

namespace {
constexpr const char* kTag = "runtime";

struct GameTaskContext {
    domes::game::GameEngine* game;
    domes::config::ModeManager* modes;
};
}  // namespace

namespace domes::runtime {

esp_err_t RuntimeAssembly::initFeatureManager(
    uint32_t supportedMask, config::FeatureManager::FeatureChangeCallback callback) {
    if (handles_.features) {
        return ESP_ERR_INVALID_STATE;
    }

    static config::FeatureManager features(supportedMask);
    features.onChange(std::move(callback));
    handles_.features = &features;
    ESP_LOGI(kTag,
             "Runtime profile: name=%s manifest_sha256=%s spec_sha256=%s "
             "sdkconfig_sha256=%s task_config_sha256=%s",
             runtime_profile::kProfileName, runtime_profile::kManifestSha256,
             runtime_profile::kSpecSha256, runtime_profile::kSdkconfigSha256,
             runtime_profile::kTaskConfigSha256);
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initModeManager() {
    if (!handles_.features || handles_.modes) {
        return ESP_ERR_INVALID_STATE;
    }

    static config::ModeManager modes(*handles_.features);
    const auto& task = infra::task::kModeTick;
    const BaseType_t created = xTaskCreate(
        [](void* param) {
            infra::TaskStartEvidence::markStarted(infra::task::kModeTick);
            auto* manager = static_cast<config::ModeManager*>(param);
            while (true) {
                manager->tick();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        },
        task.name, task.stackSize, &modes, task.priority, nullptr);
    if (created != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    handles_.modes = &modes;
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initDiagnostics() {
    if (diagnosticsInitAttempted_) {
        return ESP_ERR_INVALID_STATE;
    }
    diagnosticsInitAttempted_ = true;
    infra::Diagnostics::init();
    return infra::Diagnostics::startTask();
}

esp_err_t RuntimeAssembly::initMemoryProfiler() {
    if (memoryProfilerInitAttempted_) {
        return ESP_ERR_INVALID_STATE;
    }
    memoryProfilerInitAttempted_ = true;
    const esp_err_t err = infra::MemoryProfiler::init();
    return err == ESP_OK ? infra::MemoryProfiler::startTask() : err;
}

esp_err_t RuntimeAssembly::initLedService(ILedDriver& driver) {
    if (!handles_.features || handles_.led) {
        return ESP_ERR_INVALID_STATE;
    }

    static LedService service(driver, *handles_.features);
    const esp_err_t err = service.start();
    if (err != ESP_OK) {
        return err;
    }
    handles_.led = &service;
    handles_.features->setEnabled(config::Feature::kLedEffects, true);
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initImuService(IImuDriver& driver) {
    if (!handles_.features || !handles_.led || handles_.imu) {
        return ESP_ERR_INVALID_STATE;
    }

    static ImuService service(driver, *handles_.led, *handles_.features);
    const esp_err_t err = service.start();
    if (err != ESP_OK) {
        return err;
    }
    handles_.imu = &service;
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initAudioService(IAudioDriver& driver) {
    if (!handles_.features || handles_.audio) {
        return ESP_ERR_INVALID_STATE;
    }

    static AudioService service(driver, *handles_.features);
    const esp_err_t err = service.start();
    if (err != ESP_OK) {
        return err;
    }
    handles_.audio = &service;
    return ESP_OK;
}

esp_err_t RuntimeAssembly::connectImuFeedback(IHapticDriver* haptic, AudioService* audio) {
    if (!handles_.imu) {
        return ESP_ERR_INVALID_STATE;
    }
    handles_.imu->setHapticDriver(haptic);
    handles_.imu->setAudioService(audio);
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initTouchService(ITouchDriver& driver, TouchEventCallback callback,
                                            void* callbackContext) {
    if (!handles_.features || !handles_.led || handles_.touch) {
        return ESP_ERR_INVALID_STATE;
    }

    static TouchService service(driver, *handles_.led, *handles_.features);
    const esp_err_t err = service.start();
    if (err != ESP_OK) {
        return err;
    }
    if (callback) {
        service.setEventCallback(callback, callbackContext);
    }
    handles_.touch = &service;
    handles_.features->setEnabled(config::Feature::kTouch, true);
    return ESP_OK;
}

esp_err_t RuntimeAssembly::initGameEngine(ITouchDriver& driver) {
    if (!handles_.modes || !handles_.touch || handles_.game || handles_.injectableTouch) {
        return ESP_ERR_INVALID_STATE;
    }

    static InjectableTouchDriver injectableTouch(driver, false);
    static game::GameEngine engine(injectableTouch);

    game::FeedbackCallbacks callbacks;
    if (handles_.led) {
        LedService* led = handles_.led;
        callbacks.flashWhite = [led](uint32_t durationMs) {
            led->requestFlash(durationMs);
        };
        callbacks.flashColor = [led](Color color, uint32_t) {
            led->setSolidColor(color);
        };
    }
    if (handles_.audio) {
        AudioService* audio = handles_.audio;
        callbacks.playSound = [audio](const char* name) {
            audio->playAsset(name);
        };
    }
    engine.setFeedbackCallbacks(std::move(callbacks));
    engine.setEventCallback([](const game::GameEvent& event) {
        if (event.type == game::GameEvent::Type::kHit) {
            ESP_LOGI(kTag, "Game: HIT pad=%u reaction=%lu us", event.padIndex,
                     static_cast<unsigned long>(event.reactionTimeUs));
        } else {
            ESP_LOGI(kTag, "Game: MISS (timeout)");
        }
    });

    static GameTaskContext context = {
        .game = &engine,
        .modes = handles_.modes,
    };
    const auto& task = infra::task::kGameTick;
    const BaseType_t created = xTaskCreatePinnedToCore(
        [](void* param) {
            infra::TaskStartEvidence::markStarted(infra::task::kGameTick);
            auto* gameContext = static_cast<GameTaskContext*>(param);
            while (true) {
                if (gameContext->modes->currentMode() == config::SystemMode::kGame) {
                    gameContext->game->tick();
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        },
        task.name, task.stackSize, &context, task.priority, nullptr, task.coreAffinity);
    if (created != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    handles_.injectableTouch = &injectableTouch;
    handles_.game = &engine;
    return ESP_OK;
}

bool RuntimeAssembly::transitionToIdle() {
    return handles_.modes && handles_.modes->transitionTo(config::SystemMode::kIdle);
}

}  // namespace domes::runtime
