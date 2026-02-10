/**
 * @file espNowService.cpp
 * @brief ESP-NOW game service implementation
 *
 * Three-phase lifecycle:
 *   1. Discovery: broadcast beacons, find peer, measure RTT
 *   2. Role assignment: lower MAC = master
 *   3. Game loop: master orchestrates drill, slave responds
 */

#include "espNowService.hpp"
#include "espNowProtocol.hpp"
#include "game/gameEngine.hpp"
#include "services/ledService.hpp"
#include "config/modeManager.hpp"
#include "infra/logging.hpp"

#include "esp_log.h"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kEspNow;

// Discovery timing
static constexpr uint32_t kBeaconIntervalMs = 2000;
static constexpr uint32_t kReceiveTimeoutMs = 500;
static constexpr uint32_t kPingDelayMs = 3000;
static constexpr uint32_t kPingCount = 10;
static constexpr uint32_t kPingIntervalMs = 500;
static constexpr uint32_t kPongTimeoutMs = 2000;  // Give up waiting for PONG after 2s

// Game timing
static constexpr uint32_t kDrillRounds = 10;
static constexpr uint32_t kArmTimeoutMs = 3000;
static constexpr uint32_t kInterRoundDelayMs = 1000;
static constexpr uint32_t kJoinGameSettleMs = 2000;
static constexpr uint32_t kEventWaitTimeoutMs = kArmTimeoutMs + 2000;  // arm timeout + margin

namespace domes {

// ============================================================================
// Construction
// ============================================================================

EspNowService::EspNowService(EspNowTransport& transport)
    : transport_(transport) {
    esp_wifi_get_mac(WIFI_IF_STA, ourMac_);
    ESP_LOGI(kTag, "EspNowService: our MAC = %02X:%02X:%02X:%02X:%02X:%02X",
             ourMac_[0], ourMac_[1], ourMac_[2],
             ourMac_[3], ourMac_[4], ourMac_[5]);
}

// ============================================================================
// ITaskRunner::run  —  three-phase lifecycle
// ============================================================================

void EspNowService::run() {
    while (running_) {
        ESP_LOGI(kTag, "ESP-NOW service task started");
        TRACE_INSTANT(TRACE_ID("EspNow.DiscoveryStart"), trace::Category::kEspNow);

        // Reset state for fresh lifecycle
        peerFound_ = false;
        peerCount_ = 0;
        joinGameReceived_ = false;
        stopAllReceived_ = false;
        slaveEventPending_ = false;
        eventReceived_ = false;
        std::memset(peers_.data(), 0, sizeof(peers_));

        // Phase 1: Discovery (blocking until peer found + ping-pong done)
        runDiscovery();
        if (!running_) break;

        // Phase 2: Role assignment
        if (joinGameReceived_) {
            // JOIN_GAME received during discovery — we are the slave
            isMaster_ = false;
            ESP_LOGI(kTag, "=== Phase 2: Role = SLAVE (JOIN_GAME received during discovery) ===");
        } else {
            assignRole();
        }
        if (!running_) break;

        // Phase 3: Game loop (returns when drill completes or STOP_ALL received)
        if (isMaster_) {
            runMaster();
        } else {
            runSlave();
        }

        if (!running_) break;

        // Brief pause before restarting discovery
        ESP_LOGI(kTag, "Game loop ended, restarting discovery in 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    ESP_LOGI(kTag, "ESP-NOW service task exiting");
}

// ============================================================================
// Phase 1: Discovery
// ============================================================================

void EspNowService::runDiscovery() {
    ESP_LOGI(kTag, "=== Phase 1: Discovery ===");

    int64_t lastBeaconUs = 0;
    int64_t pingStartUs = 0;
    uint32_t pingsSent = 0;
    bool pingPhase = false;
    bool pingsDone = false;

    while (running_ && !pingsDone && !joinGameReceived_) {
        int64_t nowUs = esp_timer_get_time();

        // Send beacon periodically
        if ((nowUs - lastBeaconUs) >= static_cast<int64_t>(kBeaconIntervalMs) * 1000) {
            sendBeacon();
            lastBeaconUs = nowUs;
        }

        // Start ping phase after discovering a peer
        if (!pingPhase && peerCount_.load() > 0) {
            if (pingStartUs == 0) {
                pingStartUs = nowUs;
            } else if ((nowUs - pingStartUs) >= static_cast<int64_t>(kPingDelayMs) * 1000) {
                pingPhase = true;
                pingsSent = 0;
                ESP_LOGI(kTag, "=== Starting ping-pong latency test ===");
                TRACE_INSTANT(TRACE_ID("EspNow.PingTestStart"), trace::Category::kEspNow);
            }
        }

        // Send pings during ping phase (use mutable peer pointer via peers_ array)
        if (pingPhase && pingsSent < kPingCount && peerCount_.load() > 0) {
            auto& peer = peers_[0];
            if (!peer.pingSent) {
                sendPing(peer.mac);
                pingsSent++;
            } else {
                // Check for PONG timeout — don't get stuck forever
                int64_t waitedUs = nowUs - peer.pingSentAtUs;
                if (waitedUs > static_cast<int64_t>(kPongTimeoutMs) * 1000) {
                    ESP_LOGW(kTag, "PONG timeout after %ldms, skipping",
                             static_cast<long>(waitedUs / 1000));
                    peer.pingSent = false;
                }
            }
        }

        // Check if ping phase is done
        if (pingPhase && pingsSent >= kPingCount && peerCount_.load() > 0) {
            auto& peer = peers_[0];
            if (!peer.pingSent) {
                ESP_LOGI(kTag, "=== Ping-pong test complete: %lu pings, last RTT = %luus ===",
                         static_cast<unsigned long>(kPingCount),
                         static_cast<unsigned long>(peer.lastRttUs));
                TRACE_INSTANT(TRACE_ID("EspNow.PingTestDone"), trace::Category::kEspNow);
                pingsDone = true;
            } else {
                // Final ping waiting for PONG — also apply timeout
                int64_t waitedUs = nowUs - peer.pingSentAtUs;
                if (waitedUs > static_cast<int64_t>(kPongTimeoutMs) * 1000) {
                    ESP_LOGW(kTag, "Final PONG timeout, completing ping test anyway");
                    peer.pingSent = false;
                    ESP_LOGI(kTag, "=== Ping-pong test complete: %lu pings, last RTT = %luus ===",
                             static_cast<unsigned long>(kPingCount),
                             static_cast<unsigned long>(peer.lastRttUs));
                    TRACE_INSTANT(TRACE_ID("EspNow.PingTestDone"), trace::Category::kEspNow);
                    pingsDone = true;
                }
            }
        }

        // Try to receive
        uint8_t rxBuf[kEspNowMaxPayload];
        size_t rxLen = sizeof(rxBuf);
        TransportError err = transport_.receive(rxBuf, &rxLen, kReceiveTimeoutMs);
        if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
            handleReceived(rxBuf, rxLen);
        }
    }

    // peerMac_ and peerFound_ are set in findOrAddPeer() on first discovery

    if (joinGameReceived_) {
        ESP_LOGI(kTag, "Discovery interrupted by JOIN_GAME, skipping to game phase");
    }
}

void EspNowService::sendBeacon() {
    TRACE_SCOPE(TRACE_ID("EspNow.SendBeacon"), trace::Category::kEspNow);

    espnow::MsgHeader msg = {};
    fillHeader(msg, espnow::MsgType::kBeacon);
    sendMsg(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
}

void EspNowService::sendPing(const uint8_t* peerMac) {
    TRACE_SCOPE(TRACE_ID("EspNow.SendPing"), trace::Category::kEspNow);

    espnow::MsgHeader msg = {};
    fillHeader(msg, espnow::MsgType::kPing);

    auto* peer = findOrAddPeer(peerMac);
    if (peer) {
        peer->pingSent = true;
        peer->pingSentAtUs = esp_timer_get_time();
    }

    // Unicast ping to specific peer (gets MAC-level ACK, more reliable than broadcast)
    sendMsgTo(peerMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));

    ESP_LOGI(kTag, "PING -> %02X:%02X:%02X:%02X:%02X:%02X",
             peerMac[0], peerMac[1], peerMac[2],
             peerMac[3], peerMac[4], peerMac[5]);
}

void EspNowService::handleBeacon(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxBeacon"), trace::Category::kEspNow);

    auto* peer = findOrAddPeer(hdr->senderMac);
    if (peer) {
        peer->lastSeenUs = esp_timer_get_time();
        peer->beaconCount++;
        if (peer->beaconCount == 1) {
            ESP_LOGI(kTag, "*** NEW PEER: %02X:%02X:%02X:%02X:%02X:%02X ***",
                     hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2],
                     hdr->senderMac[3], hdr->senderMac[4], hdr->senderMac[5]);
            TRACE_INSTANT(TRACE_ID("EspNow.PeerDiscovered"), trace::Category::kEspNow);
        }
        if (peer->beaconCount <= 3 || peer->beaconCount % 10 == 0) {
            ESP_LOGI(kTag, "BEACON from %02X:%02X:%02X:%02X:%02X:%02X (count=%lu)",
                     hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2],
                     hdr->senderMac[3], hdr->senderMac[4], hdr->senderMac[5],
                     static_cast<unsigned long>(peer->beaconCount));
        }
    }
}

void EspNowService::handlePing(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxPing"), trace::Category::kEspNow);

    ESP_LOGI(kTag, "PING from %02X:%02X:%02X:%02X:%02X:%02X -> sending PONG",
             hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2],
             hdr->senderMac[3], hdr->senderMac[4], hdr->senderMac[5]);

    espnow::MsgHeader pong = {};
    fillHeader(pong, espnow::MsgType::kPong);
    pong.timestampUs = hdr->timestampUs;  // Echo original timestamp

    // Unicast PONG back to the sender (gets MAC-level ACK)
    sendMsgTo(hdr->senderMac, reinterpret_cast<const uint8_t*>(&pong), sizeof(pong));
    TRACE_INSTANT(TRACE_ID("EspNow.SendPong"), trace::Category::kEspNow);
}

void EspNowService::handlePong(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxPong"), trace::Category::kEspNow);

    auto* peer = findOrAddPeer(hdr->senderMac);
    if (peer && peer->pingSent) {
        int64_t nowUs = esp_timer_get_time();
        uint32_t rttUs = static_cast<uint32_t>(nowUs - peer->pingSentAtUs);
        peer->lastRttUs = rttUs;
        peer->pingSent = false;

        ESP_LOGI(kTag, "PONG from %02X:%02X:%02X:%02X:%02X:%02X RTT = %luus (%.2fms)",
                 hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2],
                 hdr->senderMac[3], hdr->senderMac[4], hdr->senderMac[5],
                 static_cast<unsigned long>(rttUs),
                 static_cast<float>(rttUs) / 1000.0f);

        TRACE_COUNTER(TRACE_ID("EspNow.RttUs"), rttUs, trace::Category::kEspNow);
    }
}

DiscoveredPeer* EspNowService::findOrAddPeer(const uint8_t* mac) {
    uint8_t count = peerCount_.load(std::memory_order_relaxed);

    for (uint8_t i = 0; i < count; i++) {
        if (std::memcmp(peers_[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
            return &peers_[i];
        }
    }

    if (count < kMaxDiscoveredPeers) {
        auto& peer = peers_[count];
        std::memcpy(peer.mac, mac, ESP_NOW_ETH_ALEN);
        peer.firstSeenUs = esp_timer_get_time();
        peer.lastSeenUs = peer.firstSeenUs;
        peer.beaconCount = 0;
        peer.lastRttUs = 0;
        peer.pingSent = false;
        peerCount_.fetch_add(1, std::memory_order_relaxed);

        transport_.addPeer(mac);

        // Set peerMac_ on first peer discovery so game commands work immediately
        if (count == 0) {
            std::memcpy(peerMac_, mac, ESP_NOW_ETH_ALEN);
            peerFound_ = true;
        }

        return &peer;
    }

    return nullptr;
}

const DiscoveredPeer* EspNowService::getPeer(uint8_t index) const {
    if (index < peerCount_.load(std::memory_order_relaxed)) {
        return &peers_[index];
    }
    return nullptr;
}

// ============================================================================
// Phase 2: Role Assignment
// ============================================================================

void EspNowService::assignRole() {
    ESP_LOGI(kTag, "=== Phase 2: Role Assignment ===");

    if (!peerFound_) {
        ESP_LOGW(kTag, "No peer found, cannot assign role");
        return;
    }

    // Lower MAC = master
    isMaster_ = (std::memcmp(ourMac_, peerMac_, ESP_NOW_ETH_ALEN) < 0);

    ESP_LOGI(kTag, "Role: %s (%s MAC)", isMaster_ ? "MASTER" : "SLAVE",
             isMaster_ ? "lower" : "higher");
    logMac("  Our MAC", ourMac_);
    logMac("  Peer MAC", peerMac_);
}

// ============================================================================
// Phase 3a: Master Game Loop
// ============================================================================

void EspNowService::runMaster() {
    ESP_LOGI(kTag, "=== Phase 3: Master Game Loop ===");
    TRACE_INSTANT(TRACE_ID("EspNow.DrillStart"), trace::Category::kEspNow);

    // Transition to GAME mode
    if (modeManager_) {
        auto mode = modeManager_->currentMode();
        if (mode == config::SystemMode::kIdle) {
            modeManager_->transitionTo(config::SystemMode::kGame);
        }
    }

    // Send JOIN_GAME as unicast to peer (reliable — unicast gets ACK, broadcast doesn't)
    espnow::JoinGameMsg joinMsg = {};
    fillHeader(joinMsg.header, espnow::MsgType::kJoinGame);
    ESP_LOGI(kTag, "Sending JOIN_GAME to peer");
    TRACE_INSTANT(TRACE_ID("EspNow.SendJoinGame"), trace::Category::kEspNow);
    sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&joinMsg), sizeof(joinMsg));

    // Wait for slave to be ready
    vTaskDelay(pdMS_TO_TICKS(kJoinGameSettleMs));

    // Drain any pending RX messages
    {
        uint8_t rxBuf[kEspNowMaxPayload];
        size_t rxLen = sizeof(rxBuf);
        while (isOk(transport_.receive(rxBuf, &rxLen, 0))) {
            rxLen = sizeof(rxBuf);
        }
    }

    // Wire up game event callback to receive local hit/miss events.
    // Uses member variables (eventReceived_ is atomic) instead of capturing
    // stack locals by reference — safe for cross-core callback from game_tick.
    if (gameEngine_) {
        gameEngine_->setEventCallback(
            [this](const game::GameEvent& event) {
                lastEventWasHit_ = (event.type == game::GameEvent::Type::kHit);
                lastReactionTimeUs_ = event.reactionTimeUs;
                lastPadIndex_ = event.padIndex;
                eventReceived_.store(true, std::memory_order_release);
            });
    }

    // Run drill rounds
    uint32_t totalHits = 0;
    uint32_t totalReactionUs = 0;

    ESP_LOGI(kTag, "=== DRILL START (%lu rounds) ===", static_cast<unsigned long>(kDrillRounds));

    for (uint32_t round = 0; round < kDrillRounds && running_; round++) {
        TRACE_SCOPE(TRACE_ID("EspNow.DrillRound"), trace::Category::kEspNow);

        bool targetSelf = (round % 2 == 0);
        bool hit = false;
        uint32_t reactionUs = 0;

        if (targetSelf) {
            // --- Arm self ---
            ESP_LOGI(kTag, "Round %lu: ARM self (timeout=%lums)",
                     static_cast<unsigned long>(round + 1),
                     static_cast<unsigned long>(kArmTimeoutMs));

            // Set LED green to indicate armed
            if (ledService_) {
                ledService_->setSolidColor(Color::green());
            }

            if (gameEngine_) {
                eventReceived_.store(false, std::memory_order_relaxed);
                game::ArmConfig cfg;
                cfg.timeoutMs = kArmTimeoutMs;
                cfg.feedbackMode = 0x03;
                gameEngine_->arm(cfg);

                // Wait for local event
                int64_t armStartUs = esp_timer_get_time();
                while (!eventReceived_.load(std::memory_order_acquire) && running_) {
                    int64_t elapsed = esp_timer_get_time() - armStartUs;
                    if (elapsed > static_cast<int64_t>(kEventWaitTimeoutMs) * 1000) {
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                if (eventReceived_.load(std::memory_order_acquire)) {
                    hit = lastEventWasHit_;
                    reactionUs = lastReactionTimeUs_;
                }
            }

            // Clear LED
            if (ledService_) {
                ledService_->setOff();
            }
        } else {
            // --- Arm peer ---
            ESP_LOGI(kTag, "Round %lu: ARM peer %02X:%02X:%02X:%02X:%02X:%02X",
                     static_cast<unsigned long>(round + 1),
                     peerMac_[0], peerMac_[1], peerMac_[2],
                     peerMac_[3], peerMac_[4], peerMac_[5]);

            // Send SetColor green to peer
            espnow::SetColorMsg colorMsg = {};
            fillHeader(colorMsg.header, espnow::MsgType::kSetColor);
            colorMsg.r = 0;
            colorMsg.g = 255;
            colorMsg.b = 0;
            sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&colorMsg), sizeof(colorMsg));

            // Send ArmTouch to peer
            espnow::ArmTouchMsg armMsg = {};
            fillHeader(armMsg.header, espnow::MsgType::kArmTouch);
            armMsg.timeoutMs = kArmTimeoutMs;
            armMsg.feedbackMode = 0x03;
            sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&armMsg), sizeof(armMsg));

            TRACE_INSTANT(TRACE_ID("EspNow.SendArm"), trace::Category::kEspNow);

            // Wait for TouchEvent or TimeoutEvent from peer
            eventReceived_.store(false, std::memory_order_relaxed);
            int64_t armStartUs = esp_timer_get_time();

            while (!eventReceived_.load(std::memory_order_acquire) && running_) {
                uint8_t rxBuf[kEspNowMaxPayload];
                size_t rxLen = sizeof(rxBuf);
                TransportError err = transport_.receive(rxBuf, &rxLen, 100);
                if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
                    handleReceived(rxBuf, rxLen);
                }

                int64_t elapsed = esp_timer_get_time() - armStartUs;
                if (elapsed > static_cast<int64_t>(kEventWaitTimeoutMs) * 1000) {
                    ESP_LOGW(kTag, "Round %lu: No event from peer (timeout)",
                             static_cast<unsigned long>(round + 1));
                    break;
                }
            }

            if (eventReceived_.load(std::memory_order_acquire)) {
                hit = lastEventWasHit_;
                reactionUs = lastReactionTimeUs_;
            }
        }

        // Log result
        if (hit) {
            totalHits++;
            totalReactionUs += reactionUs;
            ESP_LOGI(kTag, "Round %lu: HIT pad=%u reaction=%luus",
                     static_cast<unsigned long>(round + 1),
                     lastPadIndex_,
                     static_cast<unsigned long>(reactionUs));
        } else {
            ESP_LOGI(kTag, "Round %lu: MISS (timeout)",
                     static_cast<unsigned long>(round + 1));
        }

        // Inter-round delay
        vTaskDelay(pdMS_TO_TICKS(kInterRoundDelayMs));
    }

    // Send StopAll
    espnow::StopAllMsg stopMsg = {};
    fillHeader(stopMsg.header, espnow::MsgType::kStopAll);
    sendMsg(reinterpret_cast<const uint8_t*>(&stopMsg), sizeof(stopMsg));

    // Log summary
    uint32_t avgMs = (totalHits > 0) ? (totalReactionUs / totalHits / 1000) : 0;
    ESP_LOGI(kTag, "=== DRILL COMPLETE: %lu/%lu hits, avg=%lums ===",
             static_cast<unsigned long>(totalHits),
             static_cast<unsigned long>(kDrillRounds),
             static_cast<unsigned long>(avgMs));
    TRACE_INSTANT(TRACE_ID("EspNow.DrillComplete"), trace::Category::kEspNow);

    // Transition back to IDLE
    if (modeManager_) {
        modeManager_->transitionTo(config::SystemMode::kIdle);
    }

    // Clear game event callback
    if (gameEngine_) {
        gameEngine_->setEventCallback(nullptr);
    }

    // runMaster returns → run() loop handles restart
}

// ============================================================================
// Phase 3b: Slave Game Loop
// ============================================================================

void EspNowService::runSlave() {
    ESP_LOGI(kTag, "=== Phase 3: Slave Game Loop (waiting for commands) ===");

    // Ensure GAME mode so game_tick will tick the engine.
    // The slave might arrive here before receiving JOIN_GAME (e.g., if it
    // completed discovery/role-assignment before the master sent JOIN_GAME).
    if (modeManager_) {
        auto mode = modeManager_->currentMode();
        if (mode != config::SystemMode::kGame) {
            ESP_LOGI(kTag, "Transitioning to GAME mode for slave game loop");
            if (mode == config::SystemMode::kBooting) {
                modeManager_->transitionTo(config::SystemMode::kIdle);
            }
            modeManager_->transitionTo(config::SystemMode::kGame);
        }
    }

    // Heartbeat: track last message from master. If nothing arrives for
    // kSlaveHeartbeatTimeoutMs, assume master is dead and restart discovery.
    static constexpr uint32_t kSlaveHeartbeatTimeoutMs = 15000;
    int64_t lastMasterMsgUs = esp_timer_get_time();

    while (running_ && !stopAllReceived_) {
        // Check if game engine fired an event (flag set by game_tick callback).
        // We send the ESP-NOW response HERE on the service task (Core 0, large stack)
        // instead of from the callback on game_tick (Core 1, small stack).
        // Acquire-load ensures we see the data written before the release-store.
        if (slaveEventPending_.load(std::memory_order_acquire)) {
            slaveEventPending_.store(false, std::memory_order_relaxed);

            if (slaveEventWasHit_) {
                ESP_LOGI(kTag, "Touch detected pad=%u, sending TOUCH_EVENT (reaction=%luus)",
                         slavePadIndex_, static_cast<unsigned long>(slaveReactionTimeUs_));

                espnow::TouchEventMsg touchMsg = {};
                fillHeader(touchMsg.header, espnow::MsgType::kTouchEvent);
                touchMsg.reactionTimeUs = slaveReactionTimeUs_;
                touchMsg.padIndex = slavePadIndex_;
                sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&touchMsg), sizeof(touchMsg));
                TRACE_INSTANT(TRACE_ID("EspNow.SendTouchEvent"), trace::Category::kEspNow);
            } else {
                ESP_LOGI(kTag, "Timeout, sending TIMEOUT_EVENT");

                espnow::TimeoutEventMsg timeoutMsg = {};
                fillHeader(timeoutMsg.header, espnow::MsgType::kTimeoutEvent);
                sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&timeoutMsg), sizeof(timeoutMsg));
                TRACE_INSTANT(TRACE_ID("EspNow.SendTimeoutEvent"), trace::Category::kEspNow);
            }
        }

        // Receive and dispatch incoming messages (shorter timeout for faster flag checking)
        uint8_t rxBuf[kEspNowMaxPayload];
        size_t rxLen = sizeof(rxBuf);
        TransportError err = transport_.receive(rxBuf, &rxLen, 100);
        if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
            handleReceived(rxBuf, rxLen);
            lastMasterMsgUs = esp_timer_get_time();  // Any message resets heartbeat
        }

        // Heartbeat timeout — master might have crashed or disconnected
        int64_t silenceUs = esp_timer_get_time() - lastMasterMsgUs;
        if (silenceUs > static_cast<int64_t>(kSlaveHeartbeatTimeoutMs) * 1000) {
            ESP_LOGW(kTag, "No message from master for %lums, restarting discovery",
                     static_cast<unsigned long>(silenceUs / 1000));
            TRACE_INSTANT(TRACE_ID("EspNow.SlaveHeartbeatTimeout"), trace::Category::kEspNow);
            break;
        }
    }

    // Clean up game state before returning to discovery
    if (gameEngine_) {
        gameEngine_->disarm();
        gameEngine_->setEventCallback(nullptr);
    }
    if (ledService_) {
        ledService_->setOff();
    }
    if (modeManager_) {
        modeManager_->transitionTo(config::SystemMode::kIdle);
    }
}

// ============================================================================
// Game Command Handlers (Slave Side)
// ============================================================================

void EspNowService::handleJoinGame(const espnow::MsgHeader* hdr) {
    ESP_LOGI(kTag, "JOIN_GAME received from master");
    TRACE_INSTANT(TRACE_ID("EspNow.RxJoinGame"), trace::Category::kEspNow);

    // Signal discovery loop to exit early
    joinGameReceived_ = true;

    // Transition to GAME mode (required for game_tick to tick the engine)
    if (modeManager_) {
        auto mode = modeManager_->currentMode();
        if (mode == config::SystemMode::kBooting) {
            // ESP-NOW service started before BOOTING→IDLE transition — do it now
            ESP_LOGW(kTag, "Still in BOOTING, transitioning BOOTING→IDLE→GAME");
            modeManager_->transitionTo(config::SystemMode::kIdle);
            modeManager_->transitionTo(config::SystemMode::kGame);
        } else if (mode != config::SystemMode::kGame) {
            modeManager_->transitionTo(config::SystemMode::kGame);
        }
    }
}

void EspNowService::handleArmTouch(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::ArmTouchMsg)) return;

    const auto* msg = reinterpret_cast<const espnow::ArmTouchMsg*>(data);
    ESP_LOGI(kTag, "ARM received: timeout=%lums, feedbackMode=0x%02X",
             static_cast<unsigned long>(msg->timeoutMs), msg->feedbackMode);
    TRACE_INSTANT(TRACE_ID("EspNow.RxArm"), trace::Category::kEspNow);

    if (!gameEngine_) {
        ESP_LOGW(kTag, "ARM received but no game engine wired");
        return;
    }

    // Ensure GAME mode (game_tick only ticks the engine in GAME mode)
    if (modeManager_) {
        auto mode = modeManager_->currentMode();
        if (mode != config::SystemMode::kGame) {
            ESP_LOGW(kTag, "Not in GAME mode (mode=%s), transitioning now",
                     config::systemModeToString(mode));
            if (mode == config::SystemMode::kBooting) {
                modeManager_->transitionTo(config::SystemMode::kIdle);
            }
            modeManager_->transitionTo(config::SystemMode::kGame);
        }
    }

    // Force disarm if engine is not in READY state (safety: previous round may not have finished)
    if (gameEngine_->currentState() != game::GameState::kReady) {
        ESP_LOGW(kTag, "Engine not READY (state=%s), forcing disarm before re-arm",
                 game::gameStateToString(gameEngine_->currentState()));
        gameEngine_->disarm();
    }

    // Set callback that signals the service task via flags instead of sending
    // directly. The callback fires from game_tick (Core 1, small stack) where
    // calling sendMsgTo() would block the tick loop and risk stack overflow.
    slaveEventPending_.store(false, std::memory_order_relaxed);
    gameEngine_->setEventCallback(
        [this](const game::GameEvent& event) {
            // Write data fields first, then release-store the flag so the
            // service task (Core 0) sees consistent data after acquire-load.
            slaveEventWasHit_ = (event.type == game::GameEvent::Type::kHit);
            slaveReactionTimeUs_ = event.reactionTimeUs;
            slavePadIndex_ = event.padIndex;
            slaveEventPending_.store(true, std::memory_order_release);
        });

    // Arm the game engine
    game::ArmConfig cfg;
    cfg.timeoutMs = msg->timeoutMs;
    cfg.feedbackMode = msg->feedbackMode;
    if (!gameEngine_->arm(cfg)) {
        ESP_LOGE(kTag, "arm() failed after disarm — state=%s",
                 game::gameStateToString(gameEngine_->currentState()));
    }
}

void EspNowService::handleSetColor(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::SetColorMsg)) return;

    const auto* msg = reinterpret_cast<const espnow::SetColorMsg*>(data);
    ESP_LOGI(kTag, "SET_COLOR received: R=%u G=%u B=%u", msg->r, msg->g, msg->b);
    TRACE_INSTANT(TRACE_ID("EspNow.RxSetColor"), trace::Category::kEspNow);

    if (ledService_) {
        ledService_->setSolidColor(Color::rgb(msg->r, msg->g, msg->b));
    }
}

void EspNowService::handleStopAll(const espnow::MsgHeader* hdr) {
    ESP_LOGI(kTag, "STOP_ALL received, returning to IDLE");
    TRACE_INSTANT(TRACE_ID("EspNow.RxStopAll"), trace::Category::kEspNow);

    // Disarm game engine and clear callback
    if (gameEngine_) {
        gameEngine_->disarm();
        gameEngine_->setEventCallback(nullptr);
    }

    // Clear any pending slave event flags
    slaveEventPending_ = false;

    // Signal runSlave() to exit so the service restarts discovery
    stopAllReceived_ = true;

    // Turn off LEDs
    if (ledService_) {
        ledService_->setOff();
    }

    // Transition back to IDLE
    if (modeManager_) {
        modeManager_->transitionTo(config::SystemMode::kIdle);
    }
}

// ============================================================================
// Game Event Handlers (Master Side)
// ============================================================================

void EspNowService::handleTouchEvent(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::TouchEventMsg)) return;

    const auto* msg = reinterpret_cast<const espnow::TouchEventMsg*>(data);
    ESP_LOGI(kTag, "TOUCH_EVENT from peer: pad=%u reaction=%luus",
             msg->padIndex, static_cast<unsigned long>(msg->reactionTimeUs));
    TRACE_INSTANT(TRACE_ID("EspNow.RxTouchEvent"), trace::Category::kEspNow);

    lastEventWasHit_ = true;
    lastReactionTimeUs_ = msg->reactionTimeUs;
    lastPadIndex_ = msg->padIndex;
    eventReceived_ = true;
}

void EspNowService::handleTimeoutEvent(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::TimeoutEventMsg)) return;

    ESP_LOGI(kTag, "TIMEOUT_EVENT from peer");
    TRACE_INSTANT(TRACE_ID("EspNow.RxTimeoutEvent"), trace::Category::kEspNow);

    lastEventWasHit_ = false;
    lastReactionTimeUs_ = 0;
    lastPadIndex_ = 0;
    eventReceived_ = true;
}

// ============================================================================
// Message Routing
// ============================================================================

void EspNowService::handleReceived(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::MsgHeader)) return;

    const auto* hdr = reinterpret_cast<const espnow::MsgHeader*>(data);

    // Ignore our own messages (broadcast loopback)
    if (std::memcmp(hdr->senderMac, ourMac_, ESP_NOW_ETH_ALEN) == 0) {
        return;
    }

    auto type = static_cast<espnow::MsgType>(hdr->type);

    switch (type) {
        // Discovery messages
        case espnow::MsgType::kBeacon:       handleBeacon(hdr); break;
        case espnow::MsgType::kPing:         handlePing(hdr); break;
        case espnow::MsgType::kPong:         handlePong(hdr); break;

        // Game control (slave receives)
        case espnow::MsgType::kJoinGame:     handleJoinGame(hdr); break;
        case espnow::MsgType::kArmTouch:     handleArmTouch(data, len); break;
        case espnow::MsgType::kSetColor:     handleSetColor(data, len); break;
        case espnow::MsgType::kStopAll:      handleStopAll(hdr); break;

        // Game events (master receives)
        case espnow::MsgType::kTouchEvent:   handleTouchEvent(data, len); break;
        case espnow::MsgType::kTimeoutEvent: handleTimeoutEvent(data, len); break;

        default:
            ESP_LOGW(kTag, "Unknown ESP-NOW msg type: 0x%02X", hdr->type);
            break;
    }
}

// ============================================================================
// Helpers
// ============================================================================

void EspNowService::fillHeader(espnow::MsgHeader& hdr, espnow::MsgType type) {
    hdr.type = static_cast<uint8_t>(type);
    std::memcpy(hdr.senderMac, ourMac_, ESP_NOW_ETH_ALEN);
    hdr.timestampUs = static_cast<uint32_t>(esp_timer_get_time());
}

void EspNowService::sendMsg(const uint8_t* data, size_t len) {
    TransportError err = transport_.send(data, len);
    if (!isOk(err)) {
        ESP_LOGW(kTag, "Broadcast send failed: %s", transportErrorToString(err));
    }
}

void EspNowService::sendMsgTo(const uint8_t* mac, const uint8_t* data, size_t len) {
    TransportError err = transport_.sendTo(mac, data, len);
    if (!isOk(err)) {
        ESP_LOGW(kTag, "Unicast send failed: %s", transportErrorToString(err));
    }
}

void EspNowService::logMac(const char* prefix, const uint8_t* mac) {
    ESP_LOGI(kTag, "%s: %02X:%02X:%02X:%02X:%02X:%02X",
             prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

}  // namespace domes
