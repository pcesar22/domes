#pragma once

#include "sim/podInstance.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/simProtocol.hpp"

namespace sim {

class PodCommandHandler {
public:
    PodCommandHandler(PodInstance& pod, SimEspNowBus& bus, SimLog& log)
        : pod_(pod), bus_(bus), log_(log) {}

    void onMessage(const SimMessage& message) {
        if (!domes::espnow::hasValidFields(message) || !domes::espnow::hasValidRole(message)) {
            log_.log(pod_.podId(), "cmd", "invalid peer message rejected");
            return;
        }
        switch (message.which_payload) {
            case domes_peer_PeerMessage_set_color_tag:
                dispatchSetColor(message);
                break;
            case domes_peer_PeerMessage_arm_touch_tag:
                dispatchArmTouch(message);
                break;
            case domes_peer_PeerMessage_stop_all_tag:
                dispatchStopAll();
                break;
            case domes_peer_PeerMessage_join_game_tag:
                dispatchJoinGame();
                break;
            case domes_peer_PeerMessage_touch_event_tag:
            case domes_peer_PeerMessage_timeout_event_tag:
                break;
            default:
                log_.log(pod_.podId(), "cmd", "state-invalid peer message rejected");
                break;
        }
    }

private:
    void dispatchSetColor(const SimMessage& message) {
        const auto& color = message.payload.set_color;
        pod_.led().setAll(domes::Color::rgb(color.r, color.g, color.b));
        pod_.led().refresh();
        log_.log(pod_.podId(), "cmd", "SET_COLOR applied");
    }

    void dispatchArmTouch(const SimMessage& message) {
        const auto& command = message.payload.arm_touch;
        const uint32_t masterPodId = message.header.src_pod_id;
        const uint32_t roundToken = command.round_token;
        domes::game::ArmConfig config{.timeoutMs = command.timeout_ms,
                                      .feedbackMode = static_cast<uint8_t>(command.feedback_mode)};
        if (!pod_.engine().arm(config)) {
            log_.log(pod_.podId(), "cmd", "ARM_TOUCH rejected");
            return;
        }
        pod_.setEventCallback([this, masterPodId, roundToken](const domes::game::GameEvent& event) {
            if (event.type == domes::game::GameEvent::Type::kHit) {
                bus_.send(makeTouchEvent(pod_.podId(), masterPodId, event.reactionTimeUs,
                                         event.padIndex, roundToken));
            } else {
                bus_.send(makeTimeoutEvent(pod_.podId(), masterPodId, roundToken));
            }
        });
        log_.log(pod_.podId(), "cmd", "ARM_TOUCH timeout=" + std::to_string(command.timeout_ms));
    }

    void dispatchStopAll() {
        pod_.engine().disarm();
        if (pod_.mode().currentMode() == domes::config::SystemMode::kGame)
            pod_.mode().transitionTo(domes::config::SystemMode::kConnected);
        log_.log(pod_.podId(), "cmd", "STOP_ALL");
    }

    void dispatchJoinGame() {
        auto& mode = pod_.mode();
        if (mode.currentMode() == domes::config::SystemMode::kBooting)
            mode.transitionTo(domes::config::SystemMode::kIdle);
        if (mode.currentMode() == domes::config::SystemMode::kIdle)
            mode.transitionTo(domes::config::SystemMode::kConnected);
        if (mode.currentMode() == domes::config::SystemMode::kConnected)
            mode.transitionTo(domes::config::SystemMode::kGame);
        log_.log(pod_.podId(), "cmd", "JOIN_GAME -> GAME mode");
    }

    PodInstance& pod_;
    SimEspNowBus& bus_;
    SimLog& log_;
};

}  // namespace sim
