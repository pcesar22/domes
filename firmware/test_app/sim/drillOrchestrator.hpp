#pragma once

#include "game/gameEngine.hpp"
#include "services/roundTokenSequence.hpp"
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
    uint32_t roundToken;
    bool hit;
    uint32_t reactionTimeUs;
    uint8_t padIndex;
};

struct DrillResult {
    std::vector<RoundResult> rounds;
    uint64_t totalTimeUs;

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

    uint32_t bestReactionUs() const {
        uint32_t best = 0;
        for (const auto& r : rounds) {
            if (r.hit && (best == 0 || r.reactionTimeUs < best)) {
                best = r.reactionTimeUs;
            }
        }
        return best;
    }

    uint32_t worstReactionUs() const {
        uint32_t worst = 0;
        for (const auto& r : rounds) {
            if (r.hit && r.reactionTimeUs > worst) {
                worst = r.reactionTimeUs;
            }
        }
        return worst;
    }
};

class DrillOrchestrator {
public:
    DrillOrchestrator(SimOrchestrator& sim, SimEspNowBus& bus, SimLog& log)
        : sim_(sim), bus_(bus), log_(log) {}

    /** Resets the production-owned token sequence to its deterministic seed. */
    void resetRoundTokens(uint32_t seed = 0) { roundTokens_.reset(seed); }

    DrillResult execute(const std::vector<DrillStep>& steps,
                        const std::vector<TouchScenario>& touches) {
        DrillResult result;
        uint64_t startTimeUs = sim_.clock().nowUs();

        // --- SETUP PHASE ---
        // Transition master (pod 0) to GAME
        setupPod(sim_.pod(0));

        // Send JoinGame broadcast to all slave pods
        bus_.send(makeMessage(domes::espnow::kJoinGame, sim_.pod(0).podId(), kBroadcastPodId));
        bus_.deliverPending();

        // Register master to receive TouchEvent/TimeoutEvent from slaves
        std::vector<SimMessage> masterReceived;
        bus_.registerPod(sim_.pod(0).podId(), [&masterReceived](const SimMessage& msg) {
            masterReceived.push_back(msg);
        });

        // --- EXECUTION PHASE ---
        for (size_t i = 0; i < steps.size(); i++) {
            const auto& step = steps[i];
            const TouchScenario* touchScenario = (i < touches.size()) ? &touches[i] : nullptr;

            // Advance time by delay
            advanceNetworkTimeMs(step.delayBeforeMs);
            sim_.tickAll();
            masterReceived.clear();
            const uint32_t roundToken = roundTokens_.next();

            RoundResult roundResult{step.targetPodId, roundToken, false, 0, 0};

            // Find target pod
            PodInstance* targetPod = findPod(step.targetPodId);

            if (step.targetPodId == sim_.pod(0).podId()) {
                // MASTER AS TARGET: call arm() directly
                domes::game::ArmConfig config{step.timeoutMs, step.feedbackMode};

                targetPod->setEventCallback([&roundResult](const domes::game::GameEvent& event) {
                    roundResult.hit = (event.type == domes::game::GameEvent::Type::kHit);
                    roundResult.reactionTimeUs = event.reactionTimeUs;
                    roundResult.padIndex = event.padIndex;
                });

                targetPod->engine().arm(config);
                log_.log(targetPod->podId(), "drill", "ARM master directly");
            } else {
                // SLAVE TARGET: send via ESP-NOW
                bus_.send(makeSetColor(sim_.pod(0).podId(), step.targetPodId, step.color.r,
                                       step.color.g, step.color.b));
                bus_.send(makeArmTouch(sim_.pod(0).podId(), step.targetPodId, step.timeoutMs,
                                       step.feedbackMode, roundToken));
                bus_.deliverPending();

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

                if (step.targetPodId != sim_.pod(0).podId()) {
                    // Slave: deliver TouchEvent back to master
                    bus_.deliverPending();
                    advanceUntilTouchEvent(masterReceived, step.targetPodId, roundToken);

                    // Extract result from the last TouchEvent received by master
                    for (auto it = masterReceived.rbegin(); it != masterReceived.rend(); ++it) {
                        if (it->which_payload == domes_peer_PeerMessage_touch_event_tag) {
                            const auto& event = it->payload.touch_event;
                            if (it->header.src_pod_id != step.targetPodId ||
                                event.round_token != roundToken) {
                                continue;
                            }
                            roundResult.hit = true;
                            roundResult.reactionTimeUs = event.reaction_time_us;
                            roundResult.padIndex = static_cast<uint8_t>(event.pad_index);
                            break;
                        }
                    }
                }
                // For master-as-target, roundResult was already set by event callback
            } else {
                // MISS: advance past timeout
                advanceUntilArmed(*targetPod);
                advanceNetworkTimeMs(step.timeoutMs + 1);
                sim_.tickAll();

                if (step.targetPodId != sim_.pod(0).podId()) {
                    bus_.deliverPending();
                }
                roundResult.hit = false;
            }

            result.rounds.push_back(roundResult);

            // Wait for feedback duration
            advanceNetworkTimeMs(domes::game::kFeedbackDurationMs + 1);
            sim_.tickAll();
        }

        // --- TEARDOWN PHASE ---
        bus_.send(makeMessage(domes::espnow::kStopAll, sim_.pod(0).podId(), kBroadcastPodId));
        bus_.deliverPending();

        result.totalTimeUs = sim_.clock().nowUs() - startTimeUs;
        return result;
    }

private:
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
                return message.which_payload == domes_peer_PeerMessage_touch_event_tag &&
                       message.header.src_pod_id == targetPodId &&
                       message.payload.touch_event.round_token == roundToken;
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

    SimOrchestrator& sim_;
    SimEspNowBus& bus_;
    SimLog& log_;
    domes::RoundTokenSequence roundTokens_;
};

}  // namespace sim
