#pragma once

#include "game/gameEngine.hpp"
#include "sim/podCommandHandler.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/simOrchestrator.hpp"
#include "sim/simProtocol.hpp"

#include <algorithm>
#include <vector>

namespace sim {

struct DrillStep {
    uint16_t targetPodId;
    uint32_t delayBeforeMs;
    uint32_t timeoutMs;
    uint8_t feedbackMode;
    domes::Color color;
};

struct TouchScenario {
    uint16_t podId;
    uint32_t touchAfterMs;  // 0 = miss (no touch)
    uint8_t padIndex;
};

struct RoundResult {
    uint16_t targetPodId;
    bool hit;
    uint32_t reactionTimeUs;
    uint8_t padIndex;
};

struct DrillResult {
    std::vector<RoundResult> rounds;
    uint64_t totalTimeUs = 0;
    domes::peer_drill::CodecError codecError = domes::peer_drill::CodecError::kOk;

    bool succeeded() const { return codecError == domes::peer_drill::CodecError::kOk; }

    size_t hitCount() const {
        size_t count = 0;
        for (const auto& r : rounds) {
            if (r.hit)
                count++;
        }
        return count;
    }

    size_t missCount() const { return rounds.size() - hitCount(); }

    uint32_t avgReactionUs() const {
        uint64_t sum = 0;
        size_t hits = 0;
        for (const auto& r : rounds) {
            if (r.hit) {
                sum += r.reactionTimeUs;
                hits++;
            }
        }
        return hits > 0 ? static_cast<uint32_t>(sum / hits) : 0;
    }
};

class DrillOrchestrator {
public:
    DrillOrchestrator(SimOrchestrator& sim, SimEspNowBus& bus, SimLog& log)
        : sim_(sim), bus_(bus), log_(log) {}

    DrillResult execute(const std::vector<DrillStep>& steps,
                        const std::vector<TouchScenario>& touches) {
        DrillResult result;
        uint64_t startTimeUs = sim_.clock().nowUs();

        // --- SETUP PHASE ---
        // Transition master (pod 0) to GAME
        setupPod(sim_.pod(0));

        // Send JoinGame broadcast to all slave pods
        auto joinCmd = makeMessage(sim_.pod(0).podId(), kBroadcastPodId,
                                   domes_peer_drill_PeerMessage_join_game_tag);
        if (bus_.send(joinCmd) != domes::peer_drill::CodecError::kOk) {
            fail(result, startTimeUs);
            return result;
        }
        bus_.deliverPending();
        if (failIfCodecError(result, startTimeUs))
            return result;

        // Register master to receive TouchEvent/TimeoutEvent from slaves
        std::vector<SimMessage> masterReceived;
        auto masterHandler = bus_.overridePodHandler(
            sim_.pod(0).podId(),
            [&masterReceived](const SimMessage& msg) { masterReceived.push_back(msg); });
        if (failIfCodecError(result, startTimeUs))
            return result;

        // --- EXECUTION PHASE ---
        for (size_t i = 0; i < steps.size(); i++) {
            const auto& step = steps[i];
            const TouchScenario* touchScenario = (i < touches.size()) ? &touches[i] : nullptr;

            // Advance time by delay
            advanceNetworkTimeMs(step.delayBeforeMs);
            sim_.tickAll();
            if (failIfCodecError(result, startTimeUs))
                return result;
            masterReceived.clear();
            const uint32_t roundToken = allocateRoundToken();

            RoundResult roundResult{step.targetPodId, false, 0, 0};

            // Find target pod
            PodInstance* targetPod = findPod(step.targetPodId);
            ScopedEventCallbackOverride masterEventCallback;

            if (step.targetPodId == sim_.pod(0).podId()) {
                // MASTER AS TARGET: call arm() directly
                domes::game::ArmConfig config{step.timeoutMs, step.feedbackMode};

                masterEventCallback.install(
                    *targetPod, [&roundResult](const domes::game::GameEvent& event) {
                        roundResult.hit = (event.type == domes::game::GameEvent::Type::kHit);
                        roundResult.reactionTimeUs = event.reactionTimeUs;
                        roundResult.padIndex = event.padIndex;
                    });

                targetPod->engine().arm(config);
                log_.log(targetPod->podId(), "drill", "ARM master directly");
            } else {
                // SLAVE TARGET: send via ESP-NOW
                auto colorCmd = makeMessage(sim_.pod(0).podId(), step.targetPodId,
                                            domes_peer_drill_PeerMessage_set_color_tag);
                colorCmd.semantic.payload.set_color.red = step.color.r;
                colorCmd.semantic.payload.set_color.green = step.color.g;
                colorCmd.semantic.payload.set_color.blue = step.color.b;
                if (bus_.send(colorCmd) != domes::peer_drill::CodecError::kOk) {
                    fail(result, startTimeUs);
                    return result;
                }

                auto armCmd = makeMessage(sim_.pod(0).podId(), step.targetPodId,
                                          domes_peer_drill_PeerMessage_arm_touch_tag);
                armCmd.semantic.payload.arm_touch.timeout_ms = step.timeoutMs;
                armCmd.semantic.payload.arm_touch.feedback_mode =
                    static_cast<domes_peer_drill_FeedbackMode>(step.feedbackMode);
                armCmd.semantic.payload.arm_touch.round_token = roundToken;
                if (bus_.send(armCmd) != domes::peer_drill::CodecError::kOk) {
                    fail(result, startTimeUs);
                    return result;
                }
                bus_.deliverPending();
                if (failIfCodecError(result, startTimeUs))
                    return result;

                log_.log(sim_.pod(0).podId(), "drill",
                         "ARM slave pod" + std::to_string(step.targetPodId));
            }

            // Simulate touch or timeout
            if (touchScenario && touchScenario->touchAfterMs > 0) {
                // HIT: advance to touch time, set touch, tick
                advanceNetworkTimeMs(touchScenario->touchAfterMs);
                targetPod->touch().setTouched(touchScenario->padIndex, true);
                sim_.tickAll();
                targetPod->touch().clearAll();
                if (failIfCodecError(result, startTimeUs))
                    return result;

                if (step.targetPodId != sim_.pod(0).podId()) {
                    // Slave: deliver TouchEvent back to master
                    bus_.deliverPending();
                    advanceUntilTouchEvent(masterReceived, step.targetPodId, roundToken);
                    if (failIfCodecError(result, startTimeUs))
                        return result;

                    // Extract result from the last TouchEvent received by master
                    for (auto it = masterReceived.rbegin(); it != masterReceived.rend(); ++it) {
                        const auto& message = *it;
                        if (message.semantic.which_payload ==
                            domes_peer_drill_PeerMessage_touch_event_tag) {
                            const auto& touch = message.semantic.payload.touch_event;
                            if (message.srcPodId != step.targetPodId ||
                                touch.round_token != roundToken) {
                                continue;
                            }
                            roundResult.hit = true;
                            roundResult.reactionTimeUs = touch.reaction_time_us;
                            roundResult.padIndex = static_cast<uint8_t>(touch.pad_index);
                            break;
                        }
                    }
                }
                // For master-as-target, roundResult was already set by event callback
            } else {
                // MISS: advance past timeout
                advanceUntilArmed(*targetPod);
                if (failIfCodecError(result, startTimeUs))
                    return result;
                advanceNetworkTimeMs(step.timeoutMs + 1);
                sim_.tickAll();
                if (failIfCodecError(result, startTimeUs))
                    return result;

                if (step.targetPodId != sim_.pod(0).podId()) {
                    bus_.deliverPending();
                    if (failIfCodecError(result, startTimeUs))
                        return result;
                }
                roundResult.hit = false;
            }

            result.rounds.push_back(roundResult);

            // Wait for feedback duration
            advanceNetworkTimeMs(domes::game::kFeedbackDurationMs + 1);
            sim_.tickAll();
            if (failIfCodecError(result, startTimeUs))
                return result;
        }

        // --- TEARDOWN PHASE ---
        auto stopCmd = makeMessage(sim_.pod(0).podId(), kBroadcastPodId,
                                   domes_peer_drill_PeerMessage_stop_all_tag);
        if (bus_.send(stopCmd) != domes::peer_drill::CodecError::kOk) {
            fail(result, startTimeUs, false);
            return result;
        }
        bus_.deliverPending();
        if (failIfCodecError(result, startTimeUs, false))
            return result;

        result.totalTimeUs = sim_.clock().nowUs() - startTimeUs;
        return result;
    }

private:
    class ScopedEventCallbackOverride {
    public:
        ScopedEventCallbackOverride() = default;
        ScopedEventCallbackOverride(const ScopedEventCallbackOverride&) = delete;
        ScopedEventCallbackOverride& operator=(const ScopedEventCallbackOverride&) = delete;

        ~ScopedEventCallbackOverride() {
            if (pod_ != nullptr) {
                (void)pod_->replaceEventCallback(std::move(previous_));
            }
        }

        void install(PodInstance& pod, domes::game::GameEventCallback callback) {
            pod_ = &pod;
            previous_ = pod.replaceEventCallback(std::move(callback));
        }

    private:
        PodInstance* pod_ = nullptr;
        domes::game::GameEventCallback previous_;
    };

    void fail(DrillResult& result, uint64_t startTimeUs, bool sendStop = true) {
        result.codecError = bus_.codecFailure().value_or(domes::peer_drill::CodecError::kMalformed);
        result.totalTimeUs = sim_.clock().nowUs() - startTimeUs;
        if (!sendStop)
            return;

        auto stopCmd = makeMessage(sim_.pod(0).podId(), kBroadcastPodId,
                                   domes_peer_drill_PeerMessage_stop_all_tag);
        (void)bus_.send(stopCmd);
        bus_.deliverPending();
    }

    bool failIfCodecError(DrillResult& result, uint64_t startTimeUs, bool sendStop = true) {
        if (!bus_.codecFailure())
            return false;
        fail(result, startTimeUs, sendStop);
        return true;
    }

    void advanceNetworkTimeMs(uint64_t durationMs) { advanceNetworkTimeUs(durationMs * 1000); }

    void advanceNetworkTimeUs(uint64_t durationUs) {
        uint64_t targetUs = sim_.clock().nowUs() + durationUs;
        bus_.deliverPending();

        while (auto nextDeliveryUs = bus_.nextDeliveryTimeUs()) {
            if (*nextDeliveryUs > targetUs)
                break;
            if (*nextDeliveryUs > sim_.clock().nowUs())
                sim_.advanceTimeUs(*nextDeliveryUs - sim_.clock().nowUs());
            bus_.deliverPending();
            sim_.tickAll();
            bus_.deliverPending();
        }

        if (targetUs > sim_.clock().nowUs())
            sim_.advanceTimeUs(targetUs - sim_.clock().nowUs());
        bus_.deliverPending();
    }

    void advanceUntilArmed(PodInstance& pod) {
        while (pod.engine().currentState() != domes::game::GameState::kArmed) {
            auto nextDeliveryUs = bus_.nextDeliveryTimeUs();
            if (!nextDeliveryUs)
                return;
            uint64_t durationUs =
                *nextDeliveryUs > sim_.clock().nowUs() ? *nextDeliveryUs - sim_.clock().nowUs() : 0;
            advanceNetworkTimeUs(durationUs);
        }
    }

    static bool hasTouchEvent(const std::vector<SimMessage>& received, uint16_t targetPodId,
                              uint32_t roundToken) {
        return std::any_of(
            received.begin(), received.end(), [targetPodId, roundToken](const SimMessage& message) {
                return message.semantic.which_payload ==
                           domes_peer_drill_PeerMessage_touch_event_tag &&
                       message.srcPodId == targetPodId &&
                       message.semantic.payload.touch_event.round_token == roundToken;
            });
    }

    void advanceUntilTouchEvent(const std::vector<SimMessage>& received, uint16_t targetPodId,
                                uint32_t roundToken) {
        while (!hasTouchEvent(received, targetPodId, roundToken)) {
            auto nextDeliveryUs = bus_.nextDeliveryTimeUs();
            if (!nextDeliveryUs)
                return;
            uint64_t durationUs =
                *nextDeliveryUs > sim_.clock().nowUs() ? *nextDeliveryUs - sim_.clock().nowUs() : 0;
            advanceNetworkTimeUs(durationUs);
        }
    }

    void setupPod(PodInstance& pod) {
        auto& mode = pod.mode();
        if (mode.currentMode() == domes::config::SystemMode::kBooting)
            mode.transitionTo(domes::config::SystemMode::kIdle);
        if (mode.currentMode() == domes::config::SystemMode::kIdle)
            mode.transitionTo(domes::config::SystemMode::kConnected);
        if (mode.currentMode() == domes::config::SystemMode::kConnected)
            mode.transitionTo(domes::config::SystemMode::kGame);
    }

    PodInstance* findPod(uint16_t podId) {
        for (size_t p = 0; p < sim_.podCount(); p++) {
            if (sim_.pod(p).podId() == podId) {
                return &sim_.pod(p);
            }
        }
        return nullptr;
    }

    uint32_t allocateRoundToken() {
        const uint32_t token = nextRoundToken_++;
        if (nextRoundToken_ == 0) {
            nextRoundToken_ = 1;
        }
        return token;
    }

    SimOrchestrator& sim_;
    SimEspNowBus& bus_;
    SimLog& log_;
    uint32_t nextRoundToken_ = 1;
};

}  // namespace sim
