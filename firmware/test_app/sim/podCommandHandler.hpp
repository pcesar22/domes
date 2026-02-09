#pragma once

#include "sim/simProtocol.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/podInstance.hpp"

#include <variant>

namespace sim {

class PodCommandHandler {
public:
    PodCommandHandler(PodInstance& pod, SimEspNowBus& bus, SimLog& log)
        : pod_(pod), bus_(bus), log_(log) {}

    void onMessage(const SimMessage& msg) {
        std::visit([this](const auto& m) { dispatch(m); }, msg);
    }

private:
    void dispatch(const SetColorCommand& cmd) {
        domes::Color color = domes::Color::rgb(cmd.r, cmd.g, cmd.b);
        pod_.led().setAll(color);
        pod_.led().refresh();
        log_.log(pod_.podId(), "cmd", "SET_COLOR applied");
    }

    void dispatch(const ArmTouchCommand& cmd) {
        uint16_t masterPodId = cmd.header.srcPodId;

        // Set event callback to send touch/timeout events back to master
        pod_.setEventCallback([this, masterPodId](const domes::game::GameEvent& event) {
            if (event.type == domes::game::GameEvent::Type::kHit) {
                TouchEventMsg msg;
                msg.header.srcPodId = pod_.podId();
                msg.header.dstPodId = masterPodId;
                msg.header.type = SimMessageType::kTouchEvent;
                msg.reactionTimeUs = event.reactionTimeUs;
                msg.padIndex = event.padIndex;
                bus_.send(msg);
            } else {
                TimeoutEventMsg msg;
                msg.header.srcPodId = pod_.podId();
                msg.header.dstPodId = masterPodId;
                msg.header.type = SimMessageType::kTimeoutEvent;
                bus_.send(msg);
            }
        });

        domes::game::ArmConfig config{
            .timeoutMs = cmd.timeoutMs,
            .feedbackMode = cmd.feedbackMode,
        };
        pod_.engine().arm(config);
        log_.log(pod_.podId(), "cmd", "ARM_TOUCH timeout=" + std::to_string(cmd.timeoutMs));
    }

    void dispatch(const PlaySoundCommand& cmd) {
        pod_.audio().start();
        log_.log(pod_.podId(), "cmd", "PLAY_SOUND " + cmd.soundName);
    }

    void dispatch(const StopAllCommand&) {
        pod_.engine().disarm();
        if (pod_.mode().currentMode() == domes::config::SystemMode::kGame) {
            pod_.mode().transitionTo(domes::config::SystemMode::kConnected);
        }
        log_.log(pod_.podId(), "cmd", "STOP_ALL");
    }

    void dispatch(const JoinGameCommand&) {
        auto& mode = pod_.mode();
        if (mode.currentMode() == domes::config::SystemMode::kBooting) {
            mode.transitionTo(domes::config::SystemMode::kIdle);
        }
        if (mode.currentMode() == domes::config::SystemMode::kIdle) {
            mode.transitionTo(domes::config::SystemMode::kConnected);
        }
        if (mode.currentMode() == domes::config::SystemMode::kConnected) {
            mode.transitionTo(domes::config::SystemMode::kGame);
        }
        log_.log(pod_.podId(), "cmd", "JOIN_GAME -> GAME mode");
    }

    void dispatch(const TouchEventMsg&) {
        // Master receives this - handled by DrillOrchestrator, not here
    }

    void dispatch(const TimeoutEventMsg&) {
        // Master receives this - handled by DrillOrchestrator, not here
    }

    PodInstance& pod_;
    SimEspNowBus& bus_;
    SimLog& log_;
};

}  // namespace sim
