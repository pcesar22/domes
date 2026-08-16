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
        const auto& semantic = message.semantic;
        switch (semantic.which_payload) {
            case domes_peer_drill_PeerMessage_set_color_tag:
                dispatchSetColor(semantic.payload.set_color);
                break;
            case domes_peer_drill_PeerMessage_arm_touch_tag:
                dispatchArmTouch(message.srcPodId, semantic.payload.arm_touch);
                break;
            case domes_peer_drill_PeerMessage_stop_all_tag:
                dispatchStopAll();
                break;
            case domes_peer_drill_PeerMessage_join_game_tag:
                dispatchJoinGame();
                break;
            default:
                break;
        }
    }

private:
    void dispatchSetColor(const domes_peer_drill_SetColor& command) {
        const auto color = domes::Color::rgb(static_cast<uint8_t>(command.red),
                                             static_cast<uint8_t>(command.green),
                                             static_cast<uint8_t>(command.blue));
        pod_.led().setAll(color);
        pod_.led().refresh();
        log_.log(pod_.podId(), "cmd", "SET_COLOR applied");
    }

    void dispatchArmTouch(uint16_t masterPodId, const domes_peer_drill_ArmTouch& command) {
        const uint32_t roundToken = command.round_token;
        domes::game::ArmConfig config{
            .timeoutMs = command.timeout_ms,
            .feedbackMode = static_cast<uint8_t>(command.feedback_mode),
        };
        if (!pod_.engine().arm(config)) {
            log_.log(pod_.podId(), "cmd", "ARM_TOUCH rejected");
            return;
        }

        pod_.setEventCallback([this, masterPodId, roundToken](const domes::game::GameEvent& event) {
            if (event.type == domes::game::GameEvent::Type::kHit) {
                auto message = makeMessage(pod_.podId(), masterPodId,
                                           domes_peer_drill_PeerMessage_touch_event_tag);
                message.semantic.payload.touch_event.reaction_time_us = event.reactionTimeUs;
                message.semantic.payload.touch_event.pad_index = event.padIndex;
                message.semantic.payload.touch_event.round_token = roundToken;
                bus_.send(message);
            } else {
                auto message = makeMessage(pod_.podId(), masterPodId,
                                           domes_peer_drill_PeerMessage_timeout_event_tag);
                message.semantic.payload.timeout_event.round_token = roundToken;
                bus_.send(message);
            }
        });
        log_.log(pod_.podId(), "cmd", "ARM_TOUCH timeout=" + std::to_string(command.timeout_ms));
    }

    void dispatchStopAll() {
        pod_.engine().disarm();
        if (pod_.mode().currentMode() == domes::config::SystemMode::kGame) {
            pod_.mode().transitionTo(domes::config::SystemMode::kConnected);
        }
        log_.log(pod_.podId(), "cmd", "STOP_ALL");
    }

    void dispatchJoinGame() {
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

    PodInstance& pod_;
    SimEspNowBus& bus_;
    SimLog& log_;
};

}  // namespace sim
