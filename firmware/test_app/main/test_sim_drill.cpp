/**
 * @file test_sim_drill.cpp
 * @brief Integration tests for DrillOrchestrator and PerfettoExporter
 *
 * Tests full drill session lifecycle (setup, arm, touch/timeout, teardown)
 * and Perfetto trace export with flow events.
 */

#include "esp_timer.h"
#include "sim/drillOrchestrator.hpp"
#include "sim/perfettoExporter.hpp"
#include "sim/podCommandHandler.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/simOrchestrator.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include <gtest/gtest.h>

using namespace sim;
using namespace domes::game;
using namespace domes::config;

class SimDrillTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_stubs::mock_time_us.store(0);
        sim::globalTraceEvents().clear();
    }

    struct DrillEnv {
        SimOrchestrator sim;
        std::unique_ptr<SimEspNowBus> bus;
        std::vector<std::unique_ptr<PodCommandHandler>> handlers;
        std::unique_ptr<DrillOrchestrator> drill;

        DrillEnv(size_t numPods) {
            bus = std::make_unique<SimEspNowBus>(sim.log(), sim.clock());
            for (size_t i = 0; i < numPods; i++) {
                auto& pod = sim.addPod(static_cast<uint16_t>(i));
                handlers.push_back(std::make_unique<PodCommandHandler>(pod, *bus, sim.log()));
                auto* handler = handlers.back().get();
                bus->registerPod(pod.podId(), i == 0 ? kMasterPeerRole : kSlavePeerRole,
                                 [handler](const SimMessage& msg) { handler->onMessage(msg); });
            }
            drill = std::make_unique<DrillOrchestrator>(sim, *bus, sim.log());
        }
    };
};

// =============================================================================
// DrillResult Statistics Tests
// =============================================================================

TEST_F(SimDrillTest, DrillResult_Statistics) {
    DrillResult result;
    result.rounds.push_back({1, true, 100000, 0});
    result.rounds.push_back({2, true, 200000, 1});
    result.rounds.push_back({1, false, 0, 0});
    result.rounds.push_back({2, true, 300000, 0});
    result.rounds.push_back({1, false, 0, 0});
    result.totalTimeUs = 5000000;

    EXPECT_EQ(result.hitCount(), 3u);
    EXPECT_EQ(result.missCount(), 2u);
    EXPECT_EQ(result.avgReactionUs(), 200000u);  // (100000+200000+300000)/3
}

TEST_F(SimDrillTest, DrillResult_AllMisses) {
    DrillResult result;
    result.rounds.push_back({1, false, 0, 0});
    result.rounds.push_back({2, false, 0, 0});

    EXPECT_EQ(result.hitCount(), 0u);
    EXPECT_EQ(result.missCount(), 2u);
    EXPECT_EQ(result.avgReactionUs(), 0u);
}

// =============================================================================
// Three-Pod Drill Tests
// =============================================================================

TEST_F(SimDrillTest, ThreePod_AllHits) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;

    uint16_t targets[] = {0, 1, 2, 0, 1};
    for (int i = 0; i < 5; i++) {
        steps.push_back({targets[i], 50, 3000, 0x03, domes::Color::rgb(0, 255, 0)});
        touches.push_back({targets[i], 100, 0});
    }

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 5u);
    EXPECT_EQ(result.hitCount(), 5u);
    EXPECT_EQ(result.missCount(), 0u);

    for (const auto& round : result.rounds) {
        EXPECT_TRUE(round.hit);
        EXPECT_NEAR(round.reactionTimeUs, 100000u, 1000u);
    }

    EXPECT_GT(result.totalTimeUs, 0u);
}

TEST_F(SimDrillTest, ThreePod_WithMisses) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;

    // 5 rounds: hits on rounds 0,2,4; misses on rounds 1,3
    uint16_t targets[] = {1, 2, 1, 2, 1};
    uint32_t touchTimes[] = {100, 0, 150, 0, 200};

    for (int i = 0; i < 5; i++) {
        steps.push_back({targets[i], 50, 3000, 0x03, domes::Color::rgb(255, 0, 0)});
        touches.push_back({targets[i], touchTimes[i], 0});
    }

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 5u);
    EXPECT_EQ(result.hitCount(), 3u);
    EXPECT_EQ(result.missCount(), 2u);

    EXPECT_TRUE(result.rounds[0].hit);
    EXPECT_FALSE(result.rounds[1].hit);
    EXPECT_TRUE(result.rounds[2].hit);
    EXPECT_FALSE(result.rounds[3].hit);
    EXPECT_TRUE(result.rounds[4].hit);
}

TEST_F(SimDrillTest, InvalidSlaveFeedbackModeFailsWithoutOrdinaryRoundResult) {
    DrillEnv env(2);
    std::vector<DrillStep> steps = {
        {1, 0, 3000, 4, domes::Color::rgb(255, 0, 0)},
    };
    std::vector<TouchScenario> touches = {{1, 100, 0}};

    DrillResult result = env.drill->execute(steps, touches);

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.codecError, domes::peer_drill::CodecError::kBadEnum);
    EXPECT_TRUE(result.rounds.empty());
    ASSERT_TRUE(env.bus->codecFailure().has_value());
    EXPECT_EQ(*env.bus->codecFailure(), domes::peer_drill::CodecError::kBadEnum);
}

TEST_F(SimDrillTest, ThreePod_MasterAsTarget) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;

    // All 3 rounds target pod 0 (master)
    for (int i = 0; i < 3; i++) {
        steps.push_back({0, 50, 3000, 0x03, domes::Color::rgb(0, 0, 255)});
        touches.push_back({0, 100, 0});
    }

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 3u);
    EXPECT_EQ(result.hitCount(), 3u);

    // Check SimLog for "ARM master" entries
    auto drillEntries = env.sim.log().filter("drill");
    size_t masterArmCount = 0;
    for (const auto& e : drillEntries) {
        if (e.message.find("ARM master") != std::string::npos) {
            masterArmCount++;
        }
    }
    EXPECT_EQ(masterArmCount, 3u);
}

TEST_F(SimDrillTest, MasterTargetRestoresEventCallbackAfterExecute) {
    DrillEnv env(1);
    size_t restoredCallbackEvents = 0;
    env.sim.pod(0).setEventCallback(
        [&restoredCallbackEvents](const GameEvent&) { restoredCallbackEvents++; });

    const std::vector<DrillStep> steps = {
        {0, 0, 3000, 0x03, domes::Color::rgb(0, 0, 255)},
    };
    const std::vector<TouchScenario> touches = {{0, 100, 0}};

    const DrillResult result = env.drill->execute(steps, touches);
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.rounds.size(), 1u);
    EXPECT_EQ(restoredCallbackEvents, 0u);

    ASSERT_TRUE(env.sim.pod(0).engine().arm({.timeoutMs = 3000}));
    env.sim.advanceTimeMs(100);
    env.sim.pod(0).touch().setTouched(0, true);
    env.sim.pod(0).tick();
    env.sim.pod(0).touch().clearAll();

    EXPECT_EQ(restoredCallbackEvents, 1u);
}

TEST_F(SimDrillTest, DelayedArmIsAppliedBeforeTouch) {
    DrillEnv env(2);
    env.bus->setDeliveryPolicy([](const DeliveryContext& context) {
        if (context.payloadTag == domes_peer_drill_PeerMessage_arm_touch_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 50'000};
        }
        if (context.payloadTag == domes_peer_drill_PeerMessage_touch_event_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 25'000};
        }
        return DeliveryDirective{};
    });

    std::vector<DrillStep> steps = {
        {1, 0, 3000, 0x03, domes::Color::rgb(0, 255, 0)},
    };
    std::vector<TouchScenario> touches = {{1, 100, 0}};

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 1u);
    EXPECT_TRUE(result.rounds[0].hit);
    EXPECT_EQ(result.rounds[0].reactionTimeUs, 50'000u);
    auto touchFlow = std::find_if(
        env.bus->flowEvents().begin(), env.bus->flowEvents().end(), [](const FlowEvent& event) {
            return event.payloadTag == domes_peer_drill_PeerMessage_touch_event_tag;
        });
    ASSERT_NE(touchFlow, env.bus->flowEvents().end());
    EXPECT_EQ(touchFlow->timestampUs, 125'000u);
}

TEST_F(SimDrillTest, DelayedArmTimeoutStartsAtDelivery) {
    DrillEnv env(2);
    env.bus->setDeliveryPolicy([](const DeliveryContext& context) {
        if (context.payloadTag == domes_peer_drill_PeerMessage_arm_touch_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 50'000};
        }
        return DeliveryDirective{};
    });

    std::vector<DrillStep> steps = {
        {1, 0, 100, 0x03, domes::Color::rgb(255, 0, 0)},
    };
    std::vector<TouchScenario> touches = {{1, 0, 0}};

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 1u);
    EXPECT_FALSE(result.rounds[0].hit);
    auto timeoutFlow = std::find_if(
        env.bus->flowEvents().begin(), env.bus->flowEvents().end(), [](const FlowEvent& event) {
            return event.payloadTag == domes_peer_drill_PeerMessage_timeout_event_tag;
        });
    ASSERT_NE(timeoutFlow, env.bus->flowEvents().end());
    EXPECT_EQ(timeoutFlow->timestampUs, 151'000u);
}

TEST_F(SimDrillTest, StaleDelayedTimeoutDoesNotMaskNextTouchResponse) {
    DrillEnv env(2);
    env.bus->setDeliveryPolicy([](const DeliveryContext& context) {
        if (context.payloadTag == domes_peer_drill_PeerMessage_timeout_event_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 300'000};
        }
        if (context.payloadTag == domes_peer_drill_PeerMessage_touch_event_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 25'000};
        }
        return DeliveryDirective{};
    });

    std::vector<DrillStep> steps = {
        {1, 0, 100, 0x03, domes::Color::rgb(255, 0, 0)},
        {1, 0, 3000, 0x03, domes::Color::rgb(0, 255, 0)},
    };
    std::vector<TouchScenario> touches = {
        {1, 0, 0},
        {1, 100, 0},
    };

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 2u);
    EXPECT_FALSE(result.rounds[0].hit);
    EXPECT_TRUE(result.rounds[1].hit);
    EXPECT_EQ(result.rounds[1].reactionTimeUs, 100'000u);
    auto timeoutFlow = std::find_if(
        env.bus->flowEvents().begin(), env.bus->flowEvents().end(), [](const FlowEvent& event) {
            return event.payloadTag == domes_peer_drill_PeerMessage_timeout_event_tag;
        });
    ASSERT_NE(timeoutFlow, env.bus->flowEvents().end());
    EXPECT_EQ(timeoutFlow->timestampUs, 401'000u);
    auto touchFlow = std::find_if(
        env.bus->flowEvents().begin(), env.bus->flowEvents().end(), [](const FlowEvent& event) {
            return event.payloadTag == domes_peer_drill_PeerMessage_touch_event_tag;
        });
    ASSERT_NE(touchFlow, env.bus->flowEvents().end());
    EXPECT_EQ(touchFlow->timestampUs, 427'000u);
}

TEST_F(SimDrillTest, DelayedResultAfterExecuteUsesRestoredMasterHandler) {
    DrillEnv env(2);
    env.bus->setDeliveryPolicy([](const DeliveryContext& context) {
        if (context.payloadTag == domes_peer_drill_PeerMessage_timeout_event_tag) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 2'000'000};
        }
        return DeliveryDirective{};
    });

    const std::vector<DrillStep> steps = {
        {1, 0, 100, 0x03, domes::Color::rgb(255, 0, 0)},
    };
    const std::vector<TouchScenario> touches = {{1, 0, 0}};

    const DrillResult result = env.drill->execute(steps, touches);

    ASSERT_TRUE(result.succeeded());
    ASSERT_GT(env.bus->pendingCount(), 0u);
    env.sim.advanceTimeUs(2'000'000);
    env.bus->deliverPending();
    EXPECT_EQ(env.bus->pendingCount(), 0u);
    EXPECT_FALSE(env.bus->codecFailure().has_value());
}

TEST_F(SimDrillTest, DelayedOldArmCannotClaimNextRoundTouch) {
    DrillEnv env(2);
    size_t armCount = 0;
    env.bus->setDeliveryPolicy([&armCount](const DeliveryContext& context) {
        if (context.payloadTag != domes_peer_drill_PeerMessage_arm_touch_tag) {
            return DeliveryDirective{};
        }
        armCount++;
        if (armCount == 1) {
            return DeliveryDirective{.action = DeliveryAction::kDeliver, .delayUs = 150'000};
        }
        return DeliveryDirective{.action = DeliveryAction::kDrop};
    });

    std::vector<DrillStep> steps = {
        {1, 0, 3000, 0x03, domes::Color::rgb(255, 0, 0)},
        {1, 0, 3000, 0x03, domes::Color::rgb(0, 255, 0)},
    };
    std::vector<TouchScenario> touches = {
        {1, 100, 0},
        {1, 100, 0},
    };

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 2u);
    EXPECT_FALSE(result.rounds[0].hit);
    EXPECT_FALSE(result.rounds[1].hit);
    auto touchFlow = std::find_if(
        env.bus->flowEvents().begin(), env.bus->flowEvents().end(), [](const FlowEvent& event) {
            return event.payloadTag == domes_peer_drill_PeerMessage_touch_event_tag;
        });
    EXPECT_NE(touchFlow, env.bus->flowEvents().end());
}

// =============================================================================
// Five-Pod Large Drill
// =============================================================================

TEST_F(SimDrillTest, FivePod_LargeDrill) {
    DrillEnv env(5);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;

    for (int i = 0; i < 10; i++) {
        uint16_t target = static_cast<uint16_t>(i % 5);
        steps.push_back({target, 30, 3000, 0x03, domes::Color::rgb(128, 128, 0)});
        // Alternate hits and misses
        uint32_t touchTime = (i % 2 == 0) ? 80 : 0;
        touches.push_back({target, touchTime, 0});
    }

    DrillResult result = env.drill->execute(steps, touches);

    ASSERT_EQ(result.rounds.size(), 10u);
    EXPECT_EQ(result.hitCount(), 5u);
    EXPECT_EQ(result.missCount(), 5u);
    EXPECT_GT(result.totalTimeUs, 0u);
}

// =============================================================================
// Determinism Test
// =============================================================================

TEST_F(SimDrillTest, Determinism_SameInputSameOutput) {
    auto runDrill = [this]() {
        test_stubs::mock_time_us.store(0);
        sim::globalTraceEvents().clear();

        DrillEnv env(3);

        std::vector<DrillStep> steps;
        std::vector<TouchScenario> touches;

        steps.push_back({1, 50, 3000, 0x03, domes::Color::rgb(255, 0, 0)});
        touches.push_back({1, 120, 0});
        steps.push_back({2, 30, 3000, 0x03, domes::Color::rgb(0, 255, 0)});
        touches.push_back({2, 0, 0});  // miss
        steps.push_back({0, 40, 3000, 0x03, domes::Color::rgb(0, 0, 255)});
        touches.push_back({0, 80, 1});

        return env.drill->execute(steps, touches);
    };

    DrillResult r1 = runDrill();
    DrillResult r2 = runDrill();

    ASSERT_EQ(r1.rounds.size(), r2.rounds.size());
    for (size_t i = 0; i < r1.rounds.size(); i++) {
        EXPECT_EQ(r1.rounds[i].hit, r2.rounds[i].hit) << "round " << i;
        EXPECT_EQ(r1.rounds[i].reactionTimeUs, r2.rounds[i].reactionTimeUs) << "round " << i;
        EXPECT_EQ(r1.rounds[i].padIndex, r2.rounds[i].padIndex) << "round " << i;
        EXPECT_EQ(r1.rounds[i].targetPodId, r2.rounds[i].targetPodId) << "round " << i;
    }
    EXPECT_EQ(r1.totalTimeUs, r2.totalTimeUs);
}

// =============================================================================
// Perfetto Export Tests
// =============================================================================

TEST_F(SimDrillTest, PerfettoExport_ProducesValidJson) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;
    steps.push_back({1, 50, 3000, 0x03, domes::Color::rgb(255, 0, 0)});
    touches.push_back({1, 100, 0});

    env.drill->execute(steps, touches);

    std::string json = PerfettoExporter::exportJson(sim::globalTraceEvents(), env.sim.log(),
                                                    env.bus->flowEvents(), env.sim.podCount());

    // Must start with {"traceEvents":[ and end with ]}
    EXPECT_EQ(json.find("{\"traceEvents\":["), 0u);
    EXPECT_EQ(json.rfind("]}"), json.size() - 2);

    // Must contain at least process metadata
    EXPECT_NE(json.find("\"ph\":\"M\""), std::string::npos);
}

TEST_F(SimDrillTest, PerfettoExport_ContainsFlowEvents) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;
    // Target a slave so ESP-NOW messages are sent
    steps.push_back({1, 50, 3000, 0x03, domes::Color::rgb(255, 0, 0)});
    touches.push_back({1, 100, 0});

    env.drill->execute(steps, touches);

    // There should be flow events from the bus
    ASSERT_GT(env.bus->flowEvents().size(), 0u);

    std::string json = PerfettoExporter::exportJson(sim::globalTraceEvents(), env.sim.log(),
                                                    env.bus->flowEvents(), env.sim.podCount());

    // Flow start and finish events
    EXPECT_NE(json.find("\"ph\":\"s\""), std::string::npos);
    EXPECT_NE(json.find("\"ph\":\"f\""), std::string::npos);
}

TEST_F(SimDrillTest, PerfettoExport_WritesToFile) {
    DrillEnv env(3);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;
    steps.push_back({1, 50, 3000, 0x03, domes::Color::rgb(0, 255, 0)});
    touches.push_back({1, 100, 0});

    env.drill->execute(steps, touches);

    std::string tmpPath = "/tmp/domes_test_perfetto.json";

    bool success = PerfettoExporter::exportToFile(tmpPath, sim::globalTraceEvents(), env.sim.log(),
                                                  env.bus->flowEvents(), env.sim.podCount());

    EXPECT_TRUE(success);

    // Verify file exists and is non-empty
    std::ifstream file(tmpPath);
    ASSERT_TRUE(file.is_open());
    file.seekg(0, std::ios::end);
    EXPECT_GT(file.tellg(), 0);
    file.close();

    // Clean up
    std::remove(tmpPath.c_str());
}
