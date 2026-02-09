#pragma once

#include "sim/simLog.hpp"
#include "sim/simTouchDriver.hpp"
#include "sim/simLedDriver.hpp"
#include "sim/simAudioDriver.hpp"
#include "sim/simImuDriver.hpp"
#include "sim/simConfigStorage.hpp"
#include "config/featureManager.hpp"
#include "config/modeManager.hpp"
#include "game/gameEngine.hpp"
#include "freertos/task.h"

#include <functional>
#include <sstream>

namespace sim {

class PodInstance {
public:
    PodInstance(uint16_t podId, SimLog& log)
        : podId_(podId), log_(log),
          led_(podId, log), audio_(podId, log),
          mode_(features_), engine_(touch_) {
        engine_.setFeedbackCallbacks({
            .flashWhite = [this](uint32_t ms) {
                led_.setAll(domes::Color::white());
                led_.refresh();
                log_.log(podId_, "feedback", "flashWhite " + std::to_string(ms) + "ms");
            },
            .flashColor = [this](domes::Color c, uint32_t ms) {
                led_.setAll(c);
                led_.refresh();
                std::ostringstream oss;
                oss << "flashColor rgb(" << (int)c.r << "," << (int)c.g << "," << (int)c.b << ") " << ms << "ms";
                log_.log(podId_, "feedback", oss.str());
            },
            .playSound = [this](const char* name) {
                audio_.start();
                log_.log(podId_, "feedback", std::string("playSound ") + name);
            },
        });
    }

    uint16_t podId() const { return podId_; }
    domes::game::GameEngine& engine() { return engine_; }
    domes::config::FeatureManager& features() { return features_; }
    domes::config::ModeManager& mode() { return mode_; }
    SimTouchDriver& touch() { return touch_; }
    SimLedDriver& led() { return led_; }
    SimAudioDriver& audio() { return audio_; }

    void setEventCallback(domes::game::GameEventCallback cb) {
        engine_.setEventCallback(std::move(cb));
    }

    void tick() {
        sim_trace::currentPodId = podId_;
        engine_.tick();
    }

private:
    uint16_t podId_;
    SimLog& log_;
    SimTouchDriver touch_;
    SimLedDriver led_;
    SimAudioDriver audio_;
    SimImuDriver imu_;
    SimConfigStorage config_;
    domes::config::FeatureManager features_;
    domes::config::ModeManager mode_;
    domes::game::GameEngine engine_;
};

}  // namespace sim
