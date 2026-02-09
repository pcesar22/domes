/**
 * @file test_multi_pod_sim.cpp
 * @brief Unit tests for multi-pod simulation framework
 *
 * Tests SimOrchestrator, PodInstance, SimLog, and mock drivers
 * working together with real GameEngine/FeatureManager/ModeManager.
 */

#include <gtest/gtest.h>
#include "esp_timer.h"
#include "sim/simOrchestrator.hpp"

using namespace sim;
using namespace domes::game;
using namespace domes::config;

class MultiPodSimTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_stubs::mock_time_us.store(0);
    }

    // Helper: transition a pod through BOOTING->IDLE->CONNECTED->GAME
    void transitionToGame(PodInstance& pod) {
        pod.mode().transitionTo(SystemMode::kIdle);
        pod.mode().transitionTo(SystemMode::kConnected);
        pod.mode().transitionTo(SystemMode::kGame);
    }

    // Helper: arm a pod and trigger a hit
    void armAndHit(SimOrchestrator& orch, PodInstance& pod, uint8_t padIndex = 0) {
        pod.engine().arm({.timeoutMs = 3000});
        orch.advanceTimeMs(100);
        pod.touch().setTouched(padIndex, true);
        pod.tick();
        pod.touch().clearAll();
    }
};

// =============================================================================
// Single Pod Tests
// =============================================================================

TEST_F(MultiPodSimTest, SinglePod_ArmHitCycle) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    std::vector<GameEvent> events;
    pod.setEventCallback([&](const GameEvent& e) { events.push_back(e); });

    pod.engine().arm({.timeoutMs = 3000});
    orch.advanceTimeMs(100);
    pod.touch().setTouched(0, true);
    pod.tick();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, GameEvent::Type::kHit);
    EXPECT_EQ(events[0].reactionTimeUs, 100'000u);
    EXPECT_EQ(events[0].padIndex, 0);
}

TEST_F(MultiPodSimTest, SinglePod_ArmMissCycle) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    std::vector<GameEvent> events;
    pod.setEventCallback([&](const GameEvent& e) { events.push_back(e); });

    pod.engine().arm({.timeoutMs = 500});
    orch.advanceTimeMs(600);
    pod.tick();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, GameEvent::Type::kMiss);
    EXPECT_EQ(events[0].reactionTimeUs, 0u);
}

// =============================================================================
// Multi-Pod Tests
// =============================================================================

TEST_F(MultiPodSimTest, MultiPod_IndependentArming) {
    SimOrchestrator orch;
    auto& pod0 = orch.addPod(1);
    auto& pod1 = orch.addPod(2);
    auto& pod2 = orch.addPod(3);
    transitionToGame(pod0);
    transitionToGame(pod1);
    transitionToGame(pod2);

    // Only arm pod 0
    pod0.engine().arm({.timeoutMs = 3000});

    EXPECT_EQ(pod0.engine().currentState(), GameState::kArmed);
    EXPECT_EQ(pod1.engine().currentState(), GameState::kReady);
    EXPECT_EQ(pod2.engine().currentState(), GameState::kReady);
}

// =============================================================================
// SimLog Tests
// =============================================================================

TEST_F(MultiPodSimTest, SimLog_CapturesLedEvents) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    armAndHit(orch, pod);

    auto ledEntries = orch.log().filter("led");
    EXPECT_GE(ledEntries.size(), 1u);

    // A hit triggers flashWhite -> setAll(white) + refresh
    bool hasSetAll = false;
    for (const auto& e : ledEntries) {
        if (e.message.find("setAll") != std::string::npos) {
            hasSetAll = true;
        }
    }
    EXPECT_TRUE(hasSetAll);
}

TEST_F(MultiPodSimTest, SimLog_CapturesAudioEvents) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    armAndHit(orch, pod);

    auto audioEntries = orch.log().filter("audio");
    EXPECT_GE(audioEntries.size(), 1u);

    bool hasStart = false;
    for (const auto& e : audioEntries) {
        if (e.message == "start") hasStart = true;
    }
    EXPECT_TRUE(hasStart);
}

TEST_F(MultiPodSimTest, SimLog_FilterByCategory) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    armAndHit(orch, pod);

    auto feedbackEntries = orch.log().filter("feedback");
    auto ledEntries = orch.log().filter("led");

    // Feedback entries should only have "feedback" category
    for (const auto& e : feedbackEntries) {
        EXPECT_EQ(e.category, "feedback");
    }
    // LED entries should only have "led" category
    for (const auto& e : ledEntries) {
        EXPECT_EQ(e.category, "led");
    }

    EXPECT_GE(feedbackEntries.size(), 1u);
    EXPECT_GE(ledEntries.size(), 1u);
}

TEST_F(MultiPodSimTest, SimLog_FilterByPod) {
    SimOrchestrator orch;
    auto& pod0 = orch.addPod(1);
    auto& pod1 = orch.addPod(2);
    transitionToGame(pod0);
    transitionToGame(pod1);

    armAndHit(orch, pod0);
    armAndHit(orch, pod1);

    auto pod0Entries = orch.log().filterByPod(1);
    auto pod1Entries = orch.log().filterByPod(2);

    // Each pod's entries should only reference that pod
    for (const auto& e : pod0Entries) {
        EXPECT_EQ(e.podId, 1);
    }
    for (const auto& e : pod1Entries) {
        EXPECT_EQ(e.podId, 2);
    }

    EXPECT_GE(pod0Entries.size(), 1u);
    EXPECT_GE(pod1Entries.size(), 1u);
}

// =============================================================================
// Mode Transition Tests
// =============================================================================

TEST_F(MultiPodSimTest, ModeTransition_BootToGame) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);

    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kBooting);

    EXPECT_TRUE(pod.mode().transitionTo(SystemMode::kIdle));
    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kIdle);

    EXPECT_TRUE(pod.mode().transitionTo(SystemMode::kConnected));
    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kConnected);

    EXPECT_TRUE(pod.mode().transitionTo(SystemMode::kGame));
    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kGame);
}

// =============================================================================
// PodInstance Feedback Integration Tests
// =============================================================================

TEST_F(MultiPodSimTest, PodInstance_FeedbackCallbacksLogged) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    armAndHit(orch, pod);

    auto feedbackEntries = orch.log().filter("feedback");

    // Hit should trigger both flashWhite and playSound in the log
    bool hasFlashWhite = false;
    bool hasPlaySound = false;
    for (const auto& e : feedbackEntries) {
        if (e.message.find("flashWhite") != std::string::npos) hasFlashWhite = true;
        if (e.message.find("playSound") != std::string::npos) hasPlaySound = true;
    }
    EXPECT_TRUE(hasFlashWhite);
    EXPECT_TRUE(hasPlaySound);
}

// =============================================================================
// Orchestrator Tests
// =============================================================================

TEST_F(MultiPodSimTest, Orchestrator_TicksAllPods) {
    SimOrchestrator orch;
    auto& pod0 = orch.addPod(1);
    auto& pod1 = orch.addPod(2);
    auto& pod2 = orch.addPod(3);
    transitionToGame(pod0);
    transitionToGame(pod1);
    transitionToGame(pod2);

    // Arm all pods
    pod0.engine().arm({.timeoutMs = 3000});
    pod1.engine().arm({.timeoutMs = 3000});
    pod2.engine().arm({.timeoutMs = 3000});

    // tickAll should tick each pod
    orch.tickAll();
    orch.tickAll();
    orch.tickAll();

    // Touch update is called during tick when armed
    EXPECT_EQ(pod0.touch().updateCount(), 3);
    EXPECT_EQ(pod1.touch().updateCount(), 3);
    EXPECT_EQ(pod2.touch().updateCount(), 3);
}

// =============================================================================
// ESP-NOW Bus Tests
// =============================================================================

#include "sim/simEspNowBus.hpp"
#include "sim/podCommandHandler.hpp"

TEST_F(MultiPodSimTest, Bus_UnicastDelivery) {
    SimLog log;
    SimEspNowBus bus(log);

    std::vector<SimMessageType> pod1Received;
    std::vector<SimMessageType> pod2Received;

    bus.registerPod(1, [&](const SimMessage& msg) {
        pod1Received.push_back(getHeader(msg).type);
    });
    bus.registerPod(2, [&](const SimMessage& msg) {
        pod2Received.push_back(getHeader(msg).type);
    });

    // Send unicast from pod 0 to pod 1
    SetColorCommand cmd;
    cmd.header.srcPodId = 0;
    cmd.header.dstPodId = 1;
    cmd.header.type = SimMessageType::kSetColor;
    cmd.r = 255;
    bus.send(cmd);
    bus.deliverPending();

    // Only pod 1 should receive
    ASSERT_EQ(pod1Received.size(), 1u);
    EXPECT_EQ(pod1Received[0], SimMessageType::kSetColor);
    EXPECT_EQ(pod2Received.size(), 0u);

    // Verify flow event recorded
    ASSERT_EQ(bus.flowEvents().size(), 1u);
    EXPECT_EQ(bus.flowEvents()[0].srcPod, 0);
    EXPECT_EQ(bus.flowEvents()[0].dstPod, 1);
}

TEST_F(MultiPodSimTest, Bus_BroadcastDelivery) {
    SimLog log;
    SimEspNowBus bus(log);

    std::vector<SimMessageType> pod0Received;
    std::vector<SimMessageType> pod1Received;
    std::vector<SimMessageType> pod2Received;

    bus.registerPod(0, [&](const SimMessage& msg) {
        pod0Received.push_back(getHeader(msg).type);
    });
    bus.registerPod(1, [&](const SimMessage& msg) {
        pod1Received.push_back(getHeader(msg).type);
    });
    bus.registerPod(2, [&](const SimMessage& msg) {
        pod2Received.push_back(getHeader(msg).type);
    });

    // Broadcast from pod 0
    JoinGameCommand cmd;
    cmd.header.srcPodId = 0;
    cmd.header.dstPodId = kBroadcastPodId;
    cmd.header.type = SimMessageType::kJoinGame;
    bus.send(cmd);
    bus.deliverPending();

    // Pod 0 (sender) should NOT receive; pods 1 and 2 should
    EXPECT_EQ(pod0Received.size(), 0u);
    ASSERT_EQ(pod1Received.size(), 1u);
    EXPECT_EQ(pod1Received[0], SimMessageType::kJoinGame);
    ASSERT_EQ(pod2Received.size(), 1u);
    EXPECT_EQ(pod2Received[0], SimMessageType::kJoinGame);

    // Two flow events (one per receiver)
    EXPECT_EQ(bus.flowEvents().size(), 2u);
}

// =============================================================================
// PodCommandHandler Tests
// =============================================================================

TEST_F(MultiPodSimTest, PodCommandHandler_JoinGame) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    SimEspNowBus bus(orch.log());

    PodCommandHandler handler(pod, bus, orch.log());

    // Pod starts in BOOTING
    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kBooting);

    // Send JoinGame command
    JoinGameCommand cmd;
    cmd.header.srcPodId = 0;
    cmd.header.dstPodId = 1;
    cmd.header.type = SimMessageType::kJoinGame;
    handler.onMessage(cmd);

    // Should have transitioned through IDLE->CONNECTED->GAME
    EXPECT_EQ(pod.mode().currentMode(), SystemMode::kGame);

    // Verify log entry
    auto cmdEntries = orch.log().filter("cmd");
    ASSERT_GE(cmdEntries.size(), 1u);
    bool hasJoinGame = false;
    for (const auto& e : cmdEntries) {
        if (e.message.find("JOIN_GAME") != std::string::npos) hasJoinGame = true;
    }
    EXPECT_TRUE(hasJoinGame);
}

TEST_F(MultiPodSimTest, PodCommandHandler_SetColor) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    SimEspNowBus bus(orch.log());
    PodCommandHandler handler(pod, bus, orch.log());

    // Send SetColor green
    SetColorCommand cmd;
    cmd.header.srcPodId = 0;
    cmd.header.dstPodId = 1;
    cmd.header.type = SimMessageType::kSetColor;
    cmd.r = 0;
    cmd.g = 255;
    cmd.b = 0;
    handler.onMessage(cmd);

    // Verify LED was set to green
    domes::Color expected = domes::Color::rgb(0, 255, 0);
    EXPECT_EQ(pod.led().lastColor().r, expected.r);
    EXPECT_EQ(pod.led().lastColor().g, expected.g);
    EXPECT_EQ(pod.led().lastColor().b, expected.b);

    // Verify refresh was called
    EXPECT_GE(pod.led().refreshCount(), 1);
}

TEST_F(MultiPodSimTest, PodCommandHandler_ArmAndTouch) {
    SimOrchestrator orch;
    auto& pod = orch.addPod(1);
    transitionToGame(pod);

    SimEspNowBus bus(orch.log());
    PodCommandHandler handler(pod, bus, orch.log());

    // Register pod 0 (master) to receive events
    std::vector<SimMessage> masterReceived;
    bus.registerPod(0, [&](const SimMessage& msg) {
        masterReceived.push_back(msg);
    });

    // Master (pod 0) sends ArmTouch to pod 1
    ArmTouchCommand cmd;
    cmd.header.srcPodId = 0;
    cmd.header.dstPodId = 1;
    cmd.header.type = SimMessageType::kArmTouch;
    cmd.timeoutMs = 3000;
    cmd.feedbackMode = 0x03;
    handler.onMessage(cmd);

    // Pod 1 should now be armed
    EXPECT_EQ(pod.engine().currentState(), GameState::kArmed);

    // Simulate touch on pad 0 after 150ms
    orch.advanceTimeMs(150);
    pod.touch().setTouched(0, true);
    pod.tick();
    pod.touch().clearAll();

    // The event callback should have queued a TouchEvent on the bus
    EXPECT_GE(bus.pendingCount(), 1u);

    // Deliver pending messages
    bus.deliverPending();

    // Master (pod 0) should have received a TouchEvent
    ASSERT_EQ(masterReceived.size(), 1u);
    auto* touchEvent = std::get_if<TouchEventMsg>(&masterReceived[0]);
    ASSERT_NE(touchEvent, nullptr);
    EXPECT_EQ(touchEvent->header.srcPodId, 1);  // From pod 1
    EXPECT_EQ(touchEvent->header.dstPodId, 0);  // To master pod 0
    EXPECT_EQ(touchEvent->reactionTimeUs, 150'000u);
    EXPECT_EQ(touchEvent->padIndex, 0);
}
