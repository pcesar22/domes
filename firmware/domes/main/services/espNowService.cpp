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

#include "config/modeManager.hpp"
#include "drivers/injectableTouchDriver.hpp"
#include "espNowProtocol.hpp"
#include "esp_log.h"
#include "game/gameEngine.hpp"
#include "infra/logging.hpp"
#include "services/ledService.hpp"

#include <cstring>

static constexpr const char* kTag = domes::infra::tag::kEspNow;

// Discovery timing
static constexpr uint32_t kBeaconIntervalMs = 2000;
static constexpr uint32_t kReceiveTimeoutMs = 500;
static constexpr uint32_t kPingDelayMs = 1000;
static constexpr uint32_t kPingCount = 3;
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

EspNowService::EspNowService(EspNowTransport& transport, config::FeatureManager& features,
                             IPlatformIdentity& identity, IRandomSource& random)
    : transport_(transport), features_(features), identity_(identity), random_(random) {}

esp_err_t EspNowService::init() {
    if (platformInputsReady_) {
        return ESP_ERR_INVALID_STATE;
    }

    PlatformIdentity identity = {};
    esp_err_t err = identity_.read(identity);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Cannot resolve ESP-NOW platform identity: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t randomSeed = 0;
    err = random_.nextU32(randomSeed);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Cannot resolve ESP-NOW random seed: %s", esp_err_to_name(err));
        return err;
    }

    std::memcpy(ourMac_, identity.data(), identity.size());
    roundTokens_.reset(randomSeed);
    platformInputsReady_ = true;
    ESP_LOGI(kTag, "EspNowService: our MAC = %02X:%02X:%02X:%02X:%02X:%02X", ourMac_[0], ourMac_[1],
             ourMac_[2], ourMac_[3], ourMac_[4], ourMac_[5]);
    return ESP_OK;
}

// ============================================================================
// ITaskRunner::run  —  three-phase lifecycle
// ============================================================================

bool EspNowService::startBenchmark(uint32_t rounds) {
    if (!isFeatureEnabled()) {
        ESP_LOGW(kTag, "Cannot start benchmark: ESP-NOW feature is disabled");
        return false;
    }
    if (!peerFound_.load(std::memory_order_acquire)) {
        ESP_LOGW(kTag, "Cannot start benchmark: no peer discovered");
        return false;
    }
    if (!gameLoopActive_.load(std::memory_order_acquire)) {
        ESP_LOGW(kTag, "Cannot start benchmark: ESP-NOW lifecycle is not ready");
        return false;
    }
    if (!transport_.isConnected()) {
        ESP_LOGW(kTag, "Cannot start benchmark: ESP-NOW transport requires recovery");
        return false;
    }

    uint32_t expected = 0;
    if (!benchmarkStartLock_.compare_exchange_strong(expected, 1, std::memory_order_acquire,
                                                     std::memory_order_relaxed)) {
        ESP_LOGW(kTag, "Benchmark start already being prepared");
        return false;
    }

    if (benchmarkRequested_.load(std::memory_order_acquire)) {
        benchmarkStartLock_.store(0, std::memory_order_release);
        ESP_LOGW(kTag, "Benchmark already in progress");
        return false;
    }
    if (!isFeatureEnabled() || !peerFound_.load(std::memory_order_acquire) ||
        !gameLoopActive_.load(std::memory_order_acquire) || !transport_.isConnected()) {
        benchmarkStartLock_.store(0, std::memory_order_release);
        ESP_LOGW(kTag, "Cannot start benchmark: ESP-NOW lifecycle changed");
        return false;
    }
    if (rounds == 0 || rounds > kBenchMaxRounds) {
        rounds = 100;
    }
    benchmarkRounds_ = rounds;
    benchmarkDone_.store(false, std::memory_order_relaxed);
    benchmarkCancelRequested_.store(false, std::memory_order_relaxed);
    benchmarkRequested_.store(true, std::memory_order_release);
    benchmarkStartLock_.store(0, std::memory_order_release);
    return true;
}

bool EspNowService::takeBenchmarkResult(BenchmarkResult& result) {
    for (;;) {
        uint32_t expected = 0;
        if (benchmarkStartLock_.compare_exchange_weak(expected, 1, std::memory_order_acquire,
                                                      std::memory_order_relaxed)) {
            break;
        }
        taskYIELD();
    }

    if (!benchmarkDone_.load(std::memory_order_acquire)) {
        benchmarkStartLock_.store(0, std::memory_order_release);
        return false;
    }

    result = benchmarkResult_;
    benchmarkRequested_.store(false, std::memory_order_relaxed);
    benchmarkDone_.store(false, std::memory_order_release);
    benchmarkStartLock_.store(0, std::memory_order_release);
    return true;
}

void EspNowService::run() {
    if (!platformInputsReady_) {
        ESP_LOGE(kTag, "ESP-NOW service started before platform inputs were initialized");
        running_ = false;
        return;
    }

    while (running_) {
        // Complete pending benchmark requests even when a mode transition has
        // just disabled ESP-NOW; runBenchmark() records an immediate cancel.
        if (serviceBenchmarkRequest()) {
            continue;
        }

        // Pause when ESP-NOW is disabled by the shared feature mask.
        if (!isFeatureEnabled()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;  // Keep polling until re-enabled
        }

        lifecycleActive_.store(true, std::memory_order_release);
        ESP_LOGI(kTag, "ESP-NOW service task started");
        TRACE_INSTANT(TRACE_ID("EspNow.DiscoveryStart"), trace::Category::kEspNow);

        // Reset state for a fresh lifecycle and remove stale unicast registrations.
        clearPeers();
        if (!ensureTransportReady()) {
            lifecycleActive_.store(false, std::memory_order_release);
            vTaskDelay(pdMS_TO_TICKS(kBeaconIntervalMs));
            continue;
        }
        joinGameReceived_ = false;
        stopAllReceived_ = false;
        slaveEventPending_ = false;
        eventReceived_ = false;
        expectedPeerRoundToken_ = 0;
        activeSlaveRoundToken_ = 0;

        // Phase 1: Discovery (blocking until peer found + ping-pong done)
        runDiscovery();
        if (!running_)
            break;
        if (!isFeatureEnabled()) {
            ESP_LOGI(kTag, "Discovery stopped because ESP-NOW was disabled");
            lifecycleActive_.store(false, std::memory_order_release);
            continue;
        }
        if (!transport_.isConnected()) {
            ESP_LOGW(kTag, "Discovery stopped for ESP-NOW transport recovery");
            lifecycleActive_.store(false, std::memory_order_release);
            continue;
        }
        if (!joinGameReceived_.load(std::memory_order_acquire) &&
            !peerFound_.load(std::memory_order_acquire)) {
            ESP_LOGW(kTag, "Discovery ended without a peer; restarting");
            vTaskDelay(pdMS_TO_TICKS(kBeaconIntervalMs));
            lifecycleActive_.store(false, std::memory_order_release);
            continue;
        }

        // Phase 2: Role assignment
        if (joinGameReceived_) {
            // JOIN_GAME received during discovery — we are the slave
            isMaster_ = false;
            ESP_LOGI(kTag, "=== Phase 2: Role = SLAVE (JOIN_GAME received during discovery) ===");
        } else {
            assignRole();
        }
        if (!running_)
            break;

        // Phase 3: Game loop (returns when drill completes or STOP_ALL received).
        if (isMaster_.load(std::memory_order_relaxed)) {
            runMaster();
        } else {
            runSlave();
        }
        gameLoopActive_.store(false, std::memory_order_release);

        if (!running_)
            break;

        if (!transport_.isConnected()) {
            ESP_LOGW(kTag, "Game loop ended for ESP-NOW transport recovery");
            lifecycleActive_.store(false, std::memory_order_release);
            continue;
        }

        if (!isFeatureEnabled()) {
            ESP_LOGI(kTag, "Game loop stopped because ESP-NOW was disabled");
            lifecycleActive_.store(false, std::memory_order_release);
            continue;
        }

        // Brief pause before restarting discovery
        ESP_LOGI(kTag, "Game loop ended, restarting discovery in 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        lifecycleActive_.store(false, std::memory_order_release);
    }

    gameLoopActive_.store(false, std::memory_order_release);
    lifecycleActive_.store(false, std::memory_order_release);
    clearPeers();
    ESP_LOGI(kTag, "ESP-NOW service task exiting");
}

// ============================================================================
// Phase 1: Discovery
// ============================================================================

void EspNowService::runDiscovery() {
    ESP_LOGI(kTag, "=== Phase 1: Discovery ===");

    // Drain stale RX frames from previous session to avoid processing
    // old game messages (TOUCH_EVENT, TIMEOUT_EVENT, etc.) during discovery
    {
        uint8_t drainBuf[kEspNowMaxPayload];
        size_t drainLen = sizeof(drainBuf);
        uint32_t drained = 0;
        while (isOk(transport_.receive(drainBuf, &drainLen, 0))) {
            drainLen = sizeof(drainBuf);
            drained++;
        }
        if (drained > 0) {
            ESP_LOGI(kTag, "Drained %lu stale RX frames", static_cast<unsigned long>(drained));
        }
    }

    int64_t lastBeaconUs = 0;
    int64_t pingStartUs = 0;
    uint32_t pingsSent = 0;
    bool pingPhase = false;
    bool pingsDone = false;

    while (running_ && !pingsDone && !joinGameReceived_ && isFeatureEnabled() &&
           transport_.isConnected()) {
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
                    peer.pendingPingTimestampUs = 0;
                    publishPeerSnapshot();
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
                    peer.pendingPingTimestampUs = 0;
                    publishPeerSnapshot();
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
    fillHeader(msg, espnow::kBeacon);
    sendMsg(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
}

void EspNowService::sendPing(const uint8_t* peerMac) {
    TRACE_SCOPE(TRACE_ID("EspNow.SendPing"), trace::Category::kEspNow);

    espnow::MsgHeader msg = {};
    fillHeader(msg, espnow::kPing);

    auto* peer = findOrAddPeer(peerMac);
    if (!peer) {
        ESP_LOGW(kTag, "Cannot send PING: peer registration failed");
        return;
    }

    peer->pingSent = true;
    peer->pingSentAtUs = esp_timer_get_time();
    peer->pendingPingTimestampUs = msg.timestampUs;
    publishPeerSnapshot();

    // Unicast ping to specific peer (gets MAC-level ACK, more reliable than broadcast)
    if (!sendMsgTo(peerMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(msg))) {
        peer->pingSent = false;
        peer->pendingPingTimestampUs = 0;
        publishPeerSnapshot();
        return;
    }

    ESP_LOGI(kTag, "PING -> %02X:%02X:%02X:%02X:%02X:%02X", peerMac[0], peerMac[1], peerMac[2],
             peerMac[3], peerMac[4], peerMac[5]);
}

void EspNowService::handleBeacon(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxBeacon"), trace::Category::kEspNow);

    auto* peer = findOrAddPeer(hdr->senderMac);
    if (peer) {
        int8_t rssi = 0;
        if (transport_.lastReceivedRssi(rssi)) {
            peer->rssi = rssi;
            peer->hasRssi = true;
        }
        peer->lastSeenUs = esp_timer_get_time();
        peer->beaconCount++;
        if (peer->beaconCount == 1) {
            ESP_LOGI(kTag, "*** NEW PEER: %02X:%02X:%02X:%02X:%02X:%02X ***", hdr->senderMac[0],
                     hdr->senderMac[1], hdr->senderMac[2], hdr->senderMac[3], hdr->senderMac[4],
                     hdr->senderMac[5]);
            TRACE_INSTANT(TRACE_ID("EspNow.PeerDiscovered"), trace::Category::kEspNow);
        }
        if (peer->beaconCount <= 3 || peer->beaconCount % 10 == 0) {
            ESP_LOGI(kTag, "BEACON from %02X:%02X:%02X:%02X:%02X:%02X (count=%lu)",
                     hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2], hdr->senderMac[3],
                     hdr->senderMac[4], hdr->senderMac[5],
                     static_cast<unsigned long>(peer->beaconCount));
        }
        publishPeerSnapshot();
    }
}

void EspNowService::handlePing(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxPing"), trace::Category::kEspNow);

    ESP_LOGI(kTag, "PING from %02X:%02X:%02X:%02X:%02X:%02X -> sending PONG", hdr->senderMac[0],
             hdr->senderMac[1], hdr->senderMac[2], hdr->senderMac[3], hdr->senderMac[4],
             hdr->senderMac[5]);

    if (!findOrAddPeer(hdr->senderMac)) {
        ESP_LOGW(kTag, "Cannot answer PING: peer registration failed");
        return;
    }

    espnow::MsgHeader pong = {};
    fillHeader(pong, espnow::kPong);
    pong.timestampUs = hdr->timestampUs;  // Echo original timestamp

    // Unicast PONG back to the sender (gets MAC-level ACK)
    sendMsgTo(hdr->senderMac, reinterpret_cast<const uint8_t*>(&pong), sizeof(pong));
    TRACE_INSTANT(TRACE_ID("EspNow.SendPong"), trace::Category::kEspNow);
}

void EspNowService::handlePong(const espnow::MsgHeader* hdr) {
    TRACE_INSTANT(TRACE_ID("EspNow.RxPong"), trace::Category::kEspNow);

    auto* peer = findOrAddPeer(hdr->senderMac);
    if (peer && peer->pingSent && hdr->timestampUs == peer->pendingPingTimestampUs) {
        int64_t nowUs = esp_timer_get_time();
        uint32_t rttUs = static_cast<uint32_t>(nowUs - peer->pingSentAtUs);
        peer->lastRttUs = rttUs;
        peer->pingSent = false;
        peer->pendingPingTimestampUs = 0;

        ESP_LOGI(kTag, "PONG from %02X:%02X:%02X:%02X:%02X:%02X RTT = %luus (%.2fms)",
                 hdr->senderMac[0], hdr->senderMac[1], hdr->senderMac[2], hdr->senderMac[3],
                 hdr->senderMac[4], hdr->senderMac[5], static_cast<unsigned long>(rttUs),
                 static_cast<float>(rttUs) / 1000.0f);

        TRACE_COUNTER(TRACE_ID("EspNow.RttUs"), rttUs, trace::Category::kEspNow);
        publishPeerSnapshot();
    } else if (peer && peer->pingSent) {
        ESP_LOGW(kTag, "Ignoring PONG with stale correlation timestamp 0x%08lX",
                 static_cast<unsigned long>(hdr->timestampUs));
    }
}

DiscoveredPeer* EspNowService::findOrAddPeer(const uint8_t* mac) {
    uint8_t count = peerCount_.load(std::memory_order_relaxed);

    for (uint8_t i = 0; i < count; i++) {
        if (std::memcmp(peers_[i].mac, mac, ESP_NOW_ETH_ALEN) == 0) {
            return &peers_[i];
        }
    }

    if (count >= kMaxDiscoveredPeers) {
        return nullptr;
    }

    TransportError err = transport_.addPeer(mac);
    if (!isOk(err)) {
        ESP_LOGW(kTag, "Cannot register peer: %s", transportErrorToString(err));
        return nullptr;
    }

    auto& peer = peers_[count];
    peer = {};
    std::memcpy(peer.mac, mac, ESP_NOW_ETH_ALEN);
    peer.firstSeenUs = esp_timer_get_time();
    peer.lastSeenUs = peer.firstSeenUs;
    peerCount_.store(static_cast<uint8_t>(count + 1), std::memory_order_release);

    // Select the first successfully registered peer for this lifecycle.
    if (count == 0) {
        std::memcpy(peerMac_, mac, ESP_NOW_ETH_ALEN);
        peerFound_.store(true, std::memory_order_release);
    }

    publishPeerSnapshot();
    return &peer;
}

const DiscoveredPeer* EspNowService::getPeer(uint8_t index) const {
    if (index < peerCount_.load(std::memory_order_relaxed)) {
        return &peers_[index];
    }
    return nullptr;
}

void EspNowService::clearPeers() {
    const uint8_t count = peerCount_.load(std::memory_order_acquire);
    for (uint8_t i = 0; i < count; ++i) {
        TransportError err = transport_.removePeer(peers_[i].mac);
        if (!isOk(err) && err != TransportError::kNotInitialized) {
            ESP_LOGW(kTag, "Failed to remove stale peer: %s", transportErrorToString(err));
        }
    }

    peers_.fill({});
    peerCount_.store(0, std::memory_order_release);
    std::memset(peerMac_, 0, sizeof(peerMac_));
    peerFound_.store(false, std::memory_order_release);
    publishPeerSnapshot();
}

void EspNowService::publishPeerSnapshot() {
    utils::MutexGuard guard(peerSnapshotMutex_);
    peerSnapshotCount_ = peerCount_.load(std::memory_order_acquire);
    for (uint8_t i = 0; i < peerSnapshotCount_; ++i) {
        peerSnapshot_[i] = peers_[i];
    }
    for (uint8_t i = peerSnapshotCount_; i < kMaxDiscoveredPeers; ++i) {
        peerSnapshot_[i] = {};
    }
}

// ============================================================================
// Phase 2: Role Assignment
// ============================================================================

void EspNowService::assignRole() {
    ESP_LOGI(kTag, "=== Phase 2: Role Assignment ===");

    if (!peerFound_.load(std::memory_order_acquire)) {
        ESP_LOGW(kTag, "No peer found, cannot assign role");
        return;
    }

    // Lower MAC = master
    isMaster_ = (std::memcmp(ourMac_, peerMac_, ESP_NOW_ETH_ALEN) < 0);

    const bool isMaster = isMaster_.load(std::memory_order_relaxed);
    ESP_LOGI(kTag, "Role: %s (%s MAC)", isMaster ? "MASTER" : "SLAVE",
             isMaster ? "lower" : "higher");
    logMac("  Our MAC", ourMac_);
    logMac("  Peer MAC", peerMac_);
}

// ============================================================================
// Phase 3a: Master Game Loop
// ============================================================================

void EspNowService::runMaster() {
    ESP_LOGI(kTag, "=== Phase 3: Master Game Loop ===");
    TRACE_INSTANT(TRACE_ID("EspNow.DrillStart"), trace::Category::kEspNow);

    if (!enterPeerGameMode()) {
        return;
    }
    // Publish role readiness only after the service can process benchmark traffic.
    gameLoopActive_.store(true, std::memory_order_release);

    // Send JOIN_GAME as unicast to peer (reliable — unicast gets ACK, broadcast doesn't)
    espnow::JoinGameMsg joinMsg = {};
    fillHeader(joinMsg.header, espnow::kJoinGame);
    ESP_LOGI(kTag, "Sending JOIN_GAME to peer");
    TRACE_INSTANT(TRACE_ID("EspNow.SendJoinGame"), trace::Category::kEspNow);
    sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&joinMsg), sizeof(joinMsg));

    // Keep serving diagnostics while the slave enters game mode. Role assignment
    // is already visible to the host, and the benchmark PONG timeout is the same
    // length as this settle window, so a blind delay can lose the first round.
    const int64_t settleDeadlineUs =
        esp_timer_get_time() + static_cast<int64_t>(kJoinGameSettleMs) * 1000;
    while (running_ && isFeatureEnabled() && transport_.isConnected() &&
           esp_timer_get_time() < settleDeadlineUs) {
        if (!serviceBenchmarkRequest()) {
            receiveAndDispatchBenchmarkTraffic(50);
        }
    }

    // Wire up game event callback to receive local hit/miss events.
    // Uses member variables (eventReceived_ is atomic) instead of capturing
    // stack locals by reference — safe for cross-core callback from game_tick.
    if (gameEngine_) {
        gameEngine_->setEventCallback([this](const game::GameEvent& event) {
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

    for (uint32_t round = 0;
         round < kDrillRounds && running_ && isFeatureEnabled() && transport_.isConnected();
         round++) {
        TRACE_SCOPE(TRACE_ID("EspNow.DrillRound"), trace::Category::kEspNow);

        serviceBenchmarkRequest();

        bool targetSelf = (round % 2 == 0);
        bool hit = false;
        uint32_t reactionUs = 0;
        expectedPeerRoundToken_ = 0;

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

                // Sim mode: auto-inject touch on self after delay
                if (simMode_.load(std::memory_order_acquire) && injectableTouch_) {
                    uint32_t delayMs = simDelayMs_.load(std::memory_order_relaxed);
                    uint8_t pad = simPadIndex_.load(std::memory_order_relaxed);
                    if (delayMs > 0) {
                        vTaskDelay(pdMS_TO_TICKS(delayMs));
                        injectableTouch_->injectTouch(pad);
                        ESP_LOGI(kTag, "SIM: injected local touch pad=%u after %lums", pad,
                                 static_cast<unsigned long>(delayMs));
                    }
                }

                // Wait for local event
                int64_t armStartUs = esp_timer_get_time();
                while (!eventReceived_.load(std::memory_order_acquire) && running_ &&
                       isFeatureEnabled() && transport_.isConnected()) {
                    serviceBenchmarkRequest();
                    receiveAndDispatch(10);

                    int64_t elapsed = esp_timer_get_time() - armStartUs;
                    if (elapsed > static_cast<int64_t>(kEventWaitTimeoutMs) * 1000) {
                        break;
                    }
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
                     static_cast<unsigned long>(round + 1), peerMac_[0], peerMac_[1], peerMac_[2],
                     peerMac_[3], peerMac_[4], peerMac_[5]);

            // Send SetColor green to peer
            espnow::SetColorMsg colorMsg = {};
            fillHeader(colorMsg.header, espnow::kSetColor);
            colorMsg.r = 0;
            colorMsg.g = 255;
            colorMsg.b = 0;
            sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&colorMsg), sizeof(colorMsg));

            // Send ArmTouch to peer
            const uint32_t roundToken = allocateRoundToken();
            espnow::ArmTouchMsg armMsg = {};
            fillHeader(armMsg.header, espnow::kArmTouch);
            armMsg.roundToken = roundToken;
            armMsg.timeoutMs = kArmTimeoutMs;
            armMsg.feedbackMode = 0x03;
            eventReceived_.store(false, std::memory_order_relaxed);
            expectedPeerRoundToken_ = roundToken;
            const bool armSent =
                sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&armMsg), sizeof(armMsg));

            TRACE_INSTANT(TRACE_ID("EspNow.SendArm"), trace::Category::kEspNow);

            // Sim mode: send SimulateTouch to peer after delay
            if (armSent && simMode_.load(std::memory_order_acquire)) {
                uint32_t delayMs = simDelayMs_.load(std::memory_order_relaxed);
                uint8_t pad = simPadIndex_.load(std::memory_order_relaxed);
                if (delayMs > 0) {
                    vTaskDelay(pdMS_TO_TICKS(delayMs));
                    espnow::SimulateTouchMsg simMsg = {};
                    fillHeader(simMsg.header, espnow::kSimulateTouch);
                    simMsg.roundToken = roundToken;
                    simMsg.padIndex = pad;
                    sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&simMsg), sizeof(simMsg));
                    ESP_LOGI(kTag, "SIM: sent SIMULATE_TOUCH pad=%u to peer after %lums", pad,
                             static_cast<unsigned long>(delayMs));
                }
            }

            // Wait for TouchEvent or TimeoutEvent from peer
            int64_t armStartUs = esp_timer_get_time();

            while (armSent && !eventReceived_.load(std::memory_order_acquire) && running_ &&
                   isFeatureEnabled() && transport_.isConnected()) {
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
            expectedPeerRoundToken_ = 0;
        }

        // Log result
        if (hit) {
            totalHits++;
            totalReactionUs += reactionUs;
            ESP_LOGI(kTag, "Round %lu: HIT pad=%u reaction=%luus",
                     static_cast<unsigned long>(round + 1), lastPadIndex_,
                     static_cast<unsigned long>(reactionUs));
        } else {
            ESP_LOGI(kTag, "Round %lu: MISS (timeout)", static_cast<unsigned long>(round + 1));
        }

        // Keep serving peer pings and host benchmark requests between rounds.
        int64_t delayStartUs = esp_timer_get_time();
        while (running_ && isFeatureEnabled() && transport_.isConnected() &&
               esp_timer_get_time() - delayStartUs <
                   static_cast<int64_t>(kInterRoundDelayMs) * 1000) {
            serviceBenchmarkRequest();
            receiveAndDispatch(50);
        }
    }

    // Send StopAll
    espnow::StopAllMsg stopMsg = {};
    fillHeader(stopMsg.header, espnow::kStopAll);
    sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&stopMsg), sizeof(stopMsg));

    // Log summary
    uint32_t avgMs = (totalHits > 0) ? (totalReactionUs / totalHits / 1000) : 0;
    ESP_LOGI(kTag, "=== DRILL COMPLETE: %lu/%lu hits, avg=%lums ===",
             static_cast<unsigned long>(totalHits), static_cast<unsigned long>(kDrillRounds),
             static_cast<unsigned long>(avgMs));
    TRACE_INSTANT(TRACE_ID("EspNow.DrillComplete"), trace::Category::kEspNow);

    // Transition back to IDLE
    expectedPeerRoundToken_ = 0;
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
    if (!enterPeerGameMode()) {
        return;
    }
    gameLoopActive_.store(true, std::memory_order_release);

    // Heartbeat: track last message from master. If nothing arrives for
    // kSlaveHeartbeatTimeoutMs, assume master is dead and restart discovery.
    static constexpr uint32_t kSlaveHeartbeatTimeoutMs = 15000;
    int64_t lastMasterMsgUs = esp_timer_get_time();

    while (running_ && !stopAllReceived_ && isFeatureEnabled() && transport_.isConnected()) {
        if (serviceBenchmarkRequest()) {
            lastMasterMsgUs = esp_timer_get_time();
            continue;
        }

        // Check if game engine fired an event (flag set by game_tick callback).
        // We send the ESP-NOW response HERE on the service task (Core 0, large stack)
        // instead of from the callback on game_tick (Core 1, small stack).
        // Acquire-load ensures we see the data written before the release-store.
        if (slaveEventPending_.load(std::memory_order_acquire)) {
            slaveEventPending_.store(false, std::memory_order_relaxed);
            const uint32_t roundToken = slaveEventRoundToken_;
            if (!espnow::matchesActiveRound(activeSlaveRoundToken_, roundToken)) {
                ESP_LOGW(kTag, "Ignoring event for stale round token 0x%08lX",
                         static_cast<unsigned long>(roundToken));
                continue;
            }

            if (slaveEventWasHit_) {
                ESP_LOGI(kTag, "Touch detected pad=%u, sending TOUCH_EVENT (reaction=%luus)",
                         slavePadIndex_, static_cast<unsigned long>(slaveReactionTimeUs_));

                espnow::TouchEventMsg touchMsg = {};
                fillHeader(touchMsg.header, espnow::kTouchEvent);
                touchMsg.roundToken = roundToken;
                touchMsg.reactionTimeUs = slaveReactionTimeUs_;
                touchMsg.padIndex = slavePadIndex_;
                sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&touchMsg), sizeof(touchMsg));
                TRACE_INSTANT(TRACE_ID("EspNow.SendTouchEvent"), trace::Category::kEspNow);
            } else {
                ESP_LOGI(kTag, "Timeout, sending TIMEOUT_EVENT");

                espnow::TimeoutEventMsg timeoutMsg = {};
                fillHeader(timeoutMsg.header, espnow::kTimeoutEvent);
                timeoutMsg.roundToken = roundToken;
                sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&timeoutMsg),
                          sizeof(timeoutMsg));
                TRACE_INSTANT(TRACE_ID("EspNow.SendTimeoutEvent"), trace::Category::kEspNow);
            }
            activeSlaveRoundToken_ = 0;
        }

        // Receive and dispatch incoming messages (shorter timeout for faster flag checking)
        uint8_t rxBuf[kEspNowMaxPayload];
        size_t rxLen = sizeof(rxBuf);
        TransportError err = transport_.receive(rxBuf, &rxLen, 100);
        if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
            if (handleReceived(rxBuf, rxLen)) {
                lastMasterMsgUs = esp_timer_get_time();
            }
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
    activeSlaveRoundToken_ = 0;
    slaveEventPending_.store(false, std::memory_order_relaxed);
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

    // JOIN_GAME can arrive before this pod observes the master's discovery
    // beacon. Record and register the actual sender before the slave loop uses
    // peerMac_ for unicast game events.
    if (!findOrAddPeer(hdr->senderMac)) {
        ESP_LOGE(kTag, "Cannot join game: peer registration failed");
        return;
    }
    std::memcpy(peerMac_, hdr->senderMac, ESP_NOW_ETH_ALEN);
    peerFound_ = true;
    isMaster_ = false;

    // Signal discovery loop to exit early
    joinGameReceived_ = true;

    // GAME mode is required for game_tick to tick the engine.
    if (!enterPeerGameMode()) {
        joinGameReceived_ = false;
    }
}

void EspNowService::handleArmTouch(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::ArmTouchMsg))
        return;

    const auto* msg = reinterpret_cast<const espnow::ArmTouchMsg*>(data);
    if (msg->roundToken == 0) {
        ESP_LOGW(kTag, "Ignoring ARM with invalid round token");
        return;
    }

    ESP_LOGI(kTag, "ARM received: token=0x%08lX timeout=%lums, feedbackMode=0x%02X",
             static_cast<unsigned long>(msg->roundToken),
             static_cast<unsigned long>(msg->timeoutMs), msg->feedbackMode);
    TRACE_INSTANT(TRACE_ID("EspNow.RxArm"), trace::Category::kEspNow);

    if (!gameEngine_) {
        ESP_LOGW(kTag, "ARM received but no game engine wired");
        return;
    }

    if (!enterPeerGameMode()) {
        return;
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
    const uint32_t roundToken = msg->roundToken;
    gameEngine_->setEventCallback([this, roundToken](const game::GameEvent& event) {
        // Write data fields first, then release-store the flag so the
        // service task (Core 0) sees consistent data after acquire-load.
        slaveEventWasHit_ = (event.type == game::GameEvent::Type::kHit);
        slaveReactionTimeUs_ = event.reactionTimeUs;
        slavePadIndex_ = event.padIndex;
        slaveEventRoundToken_ = roundToken;
        slaveEventPending_.store(true, std::memory_order_release);
    });
    slaveEventPending_.store(false, std::memory_order_relaxed);
    activeSlaveRoundToken_ = roundToken;

    // Arm the game engine
    game::ArmConfig cfg;
    cfg.timeoutMs = msg->timeoutMs;
    cfg.feedbackMode = msg->feedbackMode;
    if (!gameEngine_->arm(cfg)) {
        activeSlaveRoundToken_ = 0;
        ESP_LOGE(kTag, "arm() failed after disarm — state=%s",
                 game::gameStateToString(gameEngine_->currentState()));
    }
}

void EspNowService::handleSetColor(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::SetColorMsg))
        return;

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
    activeSlaveRoundToken_ = 0;

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
// Sim Touch Injection (Slave Side)
// ============================================================================

void EspNowService::handleSimulateTouch(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::SimulateTouchMsg))
        return;

    const auto* msg = reinterpret_cast<const espnow::SimulateTouchMsg*>(data);
    if (!espnow::matchesActiveRound(activeSlaveRoundToken_, msg->roundToken)) {
        ESP_LOGW(kTag, "Ignoring SIMULATE_TOUCH for stale round token 0x%08lX",
                 static_cast<unsigned long>(msg->roundToken));
        return;
    }

    ESP_LOGI(kTag, "SIMULATE_TOUCH received: token=0x%08lX pad=%u",
             static_cast<unsigned long>(msg->roundToken), msg->padIndex);
    TRACE_INSTANT(TRACE_ID("EspNow.RxSimTouch"), trace::Category::kEspNow);

    if (injectableTouch_) {
        if (msg->padIndex < injectableTouch_->getPadCount()) {
            injectableTouch_->injectTouch(msg->padIndex);
        } else {
            ESP_LOGW(kTag, "Ignoring invalid simulated touch pad: %u", msg->padIndex);
        }
    }
}

// ============================================================================
// Game Event Handlers (Master Side)
// ============================================================================

void EspNowService::handleTouchEvent(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::TouchEventMsg))
        return;

    const auto* msg = reinterpret_cast<const espnow::TouchEventMsg*>(data);
    if (!espnow::matchesActiveRound(expectedPeerRoundToken_, msg->roundToken)) {
        ESP_LOGW(kTag, "Ignoring TOUCH_EVENT for stale round token 0x%08lX",
                 static_cast<unsigned long>(msg->roundToken));
        return;
    }
    ESP_LOGI(kTag, "TOUCH_EVENT from peer: pad=%u reaction=%luus", msg->padIndex,
             static_cast<unsigned long>(msg->reactionTimeUs));
    TRACE_INSTANT(TRACE_ID("EspNow.RxTouchEvent"), trace::Category::kEspNow);

    lastEventWasHit_ = true;
    lastReactionTimeUs_ = msg->reactionTimeUs;
    lastPadIndex_ = msg->padIndex;
    eventReceived_ = true;
}

void EspNowService::handleTimeoutEvent(const uint8_t* data, size_t len) {
    if (len < sizeof(espnow::TimeoutEventMsg))
        return;

    const auto* msg = reinterpret_cast<const espnow::TimeoutEventMsg*>(data);
    if (!espnow::matchesActiveRound(expectedPeerRoundToken_, msg->roundToken)) {
        ESP_LOGW(kTag, "Ignoring TIMEOUT_EVENT for stale round token 0x%08lX",
                 static_cast<unsigned long>(msg->roundToken));
        return;
    }

    ESP_LOGI(kTag, "TIMEOUT_EVENT from peer");
    TRACE_INSTANT(TRACE_ID("EspNow.RxTimeoutEvent"), trace::Category::kEspNow);

    lastEventWasHit_ = false;
    lastReactionTimeUs_ = 0;
    lastPadIndex_ = 0;
    eventReceived_ = true;
}

// ============================================================================
// Benchmark
// ============================================================================

bool EspNowService::serviceBenchmarkRequest() {
    if (!benchmarkRequested_.load(std::memory_order_acquire) ||
        benchmarkDone_.load(std::memory_order_acquire)) {
        return false;
    }

    runBenchmark();
    return true;
}

void EspNowService::runBenchmark() {
    ESP_LOGI(kTag, "=== Starting latency benchmark (%lu rounds) ===",
             static_cast<unsigned long>(benchmarkRounds_));
    TRACE_SCOPE(TRACE_ID("EspNow.Benchmark"), trace::Category::kEspNow);

    BenchmarkResult result = {};
    uint32_t completed = 0;
    uint32_t failed = 0;

    static constexpr uint32_t kBenchPongTimeoutMs = 2000;
    static constexpr uint32_t kBenchInterPingMs = 10;
    static constexpr uint32_t kBenchMaxDurationMs = 45000;
    const int64_t deadlineUs =
        esp_timer_get_time() + static_cast<int64_t>(kBenchMaxDurationMs) * 1000;

    auto shouldContinue = [&]() {
        return running_.load(std::memory_order_relaxed) &&
               gameLoopActive_.load(std::memory_order_acquire) && isFeatureEnabled() &&
               transport_.isConnected() &&
               !benchmarkCancelRequested_.load(std::memory_order_acquire) &&
               esp_timer_get_time() < deadlineUs;
    };

    for (uint32_t i = 0; i < benchmarkRounds_ && shouldContinue(); ++i) {
        // Send ping
        espnow::MsgHeader ping = {};
        fillHeader(ping, espnow::kPing);

        auto& peer = peers_[0];
        peer.pingSent = true;
        peer.pingSentAtUs = esp_timer_get_time();
        peer.pendingPingTimestampUs = ping.timestampUs;
        publishPeerSnapshot();

        if (!sendMsgTo(peerMac_, reinterpret_cast<const uint8_t*>(&ping), sizeof(ping))) {
            peer.pingSent = false;
            peer.pendingPingTimestampUs = 0;
            publishPeerSnapshot();
            failed++;
            continue;
        }

        // Wait for pong
        int64_t startUs = peer.pingSentAtUs;
        bool gotPong = false;

        while (!gotPong && shouldContinue()) {
            uint8_t rxBuf[kEspNowMaxPayload];
            size_t rxLen = sizeof(rxBuf);
            TransportError err = transport_.receive(rxBuf, &rxLen, 100);
            if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
                const auto* hdr = reinterpret_cast<const espnow::MsgHeader*>(rxBuf);
                uint8_t sourceMac[ESP_NOW_ETH_ALEN] = {};
                if (validateReceivedSource(*hdr, sourceMac)) {
                    const auto type = static_cast<espnow::MsgType>(hdr->type);
                    const bool correlatedPong =
                        type == espnow::kPong && rxLen == espnow::expectedMessageSize(type) &&
                        std::memcmp(sourceMac, peerMac_, ESP_NOW_ETH_ALEN) == 0 && peer.pingSent &&
                        hdr->timestampUs == peer.pendingPingTimestampUs;
                    if (correlatedPong) {
                        int64_t nowUs = esp_timer_get_time();
                        uint32_t rttUs = static_cast<uint32_t>(nowUs - peer.pingSentAtUs);
                        peer.lastRttUs = rttUs;
                        peer.pingSent = false;
                        peer.pendingPingTimestampUs = 0;
                        publishPeerSnapshot();
                        benchmarkRtts_[completed] = rttUs;
                        completed++;
                        gotPong = true;
                    } else {
                        // Handle other valid traffic normally. Stale or foreign PONGs cannot
                        // complete this benchmark round because handlePong checks correlation.
                        handleValidatedReceived(rxBuf, rxLen, sourceMac);
                    }
                }
            }

            // Check timeout
            int64_t elapsed = esp_timer_get_time() - startUs;
            if (!gotPong && elapsed > static_cast<int64_t>(kBenchPongTimeoutMs) * 1000) {
                peer.pingSent = false;
                peer.pendingPingTimestampUs = 0;
                publishPeerSnapshot();
                failed++;
                break;
            }
        }

        if (!gotPong && !shouldContinue()) {
            peer.pingSent = false;
            peer.pendingPingTimestampUs = 0;
            publishPeerSnapshot();
        }

        // Brief delay between pings
        if (shouldContinue()) {
            vTaskDelay(pdMS_TO_TICKS(kBenchInterPingMs));
        }
    }

    // Compute stats
    result.roundsCompleted = completed;
    result.roundsFailed = failed;

    if (completed > 0) {
        // Sort for percentile computation
        std::sort(benchmarkRtts_.begin(), benchmarkRtts_.begin() + completed);

        result.minRttUs = benchmarkRtts_[0];
        result.maxRttUs = benchmarkRtts_[completed - 1];

        uint64_t sum = 0;
        for (uint32_t i = 0; i < completed; ++i) {
            sum += benchmarkRtts_[i];
        }
        result.meanRttUs = static_cast<uint32_t>(sum / completed);

        result.p50RttUs = benchmarkRtts_[completed * 50 / 100];
        result.p95RttUs = benchmarkRtts_[completed * 95 / 100];
        result.p99RttUs = benchmarkRtts_[completed * 99 / 100];
    }

    benchmarkResult_ = result;
    benchmarkDone_.store(true, std::memory_order_release);

    if (benchmarkCancelRequested_.load(std::memory_order_relaxed) || !isFeatureEnabled()) {
        ESP_LOGW(kTag, "Benchmark canceled after %lu completed and %lu failed rounds",
                 static_cast<unsigned long>(completed), static_cast<unsigned long>(failed));
    } else if (esp_timer_get_time() >= deadlineUs) {
        ESP_LOGW(kTag, "Benchmark stopped at the %lums service deadline",
                 static_cast<unsigned long>(kBenchMaxDurationMs));
    }

    ESP_LOGI(kTag, "=== Benchmark complete: %lu/%lu rounds, P50=%luus P95=%luus P99=%luus ===",
             static_cast<unsigned long>(completed), static_cast<unsigned long>(benchmarkRounds_),
             static_cast<unsigned long>(result.p50RttUs),
             static_cast<unsigned long>(result.p95RttUs),
             static_cast<unsigned long>(result.p99RttUs));
}

// ============================================================================
// Message Routing
// ============================================================================

bool EspNowService::receiveAndDispatch(uint32_t timeoutMs) {
    uint8_t rxBuf[kEspNowMaxPayload];
    size_t rxLen = sizeof(rxBuf);
    TransportError err = transport_.receive(rxBuf, &rxLen, timeoutMs);
    if (isOk(err) && rxLen >= sizeof(espnow::MsgHeader)) {
        return handleReceived(rxBuf, rxLen);
    }
    return false;
}

bool EspNowService::receiveAndDispatchBenchmarkTraffic(uint32_t timeoutMs) {
    uint8_t rxBuf[kEspNowMaxPayload];
    size_t rxLen = sizeof(rxBuf);
    const TransportError err = transport_.receive(rxBuf, &rxLen, timeoutMs);
    if (!isOk(err) || rxLen < sizeof(espnow::MsgHeader)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const espnow::MsgHeader*>(rxBuf);
    uint8_t sourceMac[ESP_NOW_ETH_ALEN] = {};
    if (!validateReceivedSource(*hdr, sourceMac) || !isSelectedPeer(sourceMac)) {
        return false;
    }

    const auto type = static_cast<espnow::MsgType>(hdr->type);
    if ((type != espnow::kPing && type != espnow::kPong) ||
        rxLen != espnow::expectedMessageSize(type)) {
        ESP_LOGW(kTag, "Ignoring %s during master settle window", espnow::msgTypeName(type));
        return false;
    }

    if (type == espnow::kPing) {
        handlePing(hdr);
    } else {
        handlePong(hdr);
    }
    return true;
}

bool EspNowService::handleReceived(const uint8_t* data, size_t len) {
    if (!data || len < sizeof(espnow::MsgHeader)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const espnow::MsgHeader*>(data);
    uint8_t sourceMac[ESP_NOW_ETH_ALEN] = {};
    if (!validateReceivedSource(*hdr, sourceMac)) {
        return false;
    }

    return handleValidatedReceived(data, len, sourceMac);
}

bool EspNowService::handleValidatedReceived(const uint8_t* data, size_t len,
                                            const uint8_t sourceMac[ESP_NOW_ETH_ALEN]) {
    if (!data || !sourceMac || len < sizeof(espnow::MsgHeader)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const espnow::MsgHeader*>(data);

    // Ignore our own messages (broadcast loopback)
    if (std::memcmp(sourceMac, ourMac_, ESP_NOW_ETH_ALEN) == 0) {
        return false;
    }

    espnow::Message message = domes_peer_PeerMessage_init_zero;
    if (!espnow::decodeLegacyMessage(data, len, message)) {
        ESP_LOGW(kTag, "Rejecting malformed ESP-NOW message type 0x%02X (%zu bytes)", hdr->type,
                 len);
        return false;
    }
    const auto type = espnow::messageType(message);
    message.header.sender_role = (type == espnow::kTouchEvent || type == espnow::kTimeoutEvent)
                                     ? espnow::kRoleSlave
                                     : espnow::kRoleMaster;
    if (espnow::isDiscoveryMessage(type))
        message.header.sender_role = espnow::kRoleUnspecified;
    if (!espnow::hasValidRole(message))
        return false;

    const bool controlMessage = type == espnow::kArmTouch || type == espnow::kSetColor ||
                                type == espnow::kStopAll || type == espnow::kSimulateTouch;
    const bool eventMessage = type == espnow::kTouchEvent || type == espnow::kTimeoutEvent;
    if ((controlMessage && isMaster_.load(std::memory_order_relaxed)) ||
        (eventMessage && !isMaster_.load(std::memory_order_relaxed))) {
        ESP_LOGW(kTag, "Rejecting role-invalid %s", espnow::msgTypeName(type));
        return false;
    }

    const bool receiverIsMaster = isMaster_.load(std::memory_order_relaxed);
    const auto receiverRole = receiverIsMaster ? espnow::kRoleMaster : espnow::kRoleSlave;
    auto lifecycleState = domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_DISCOVERY;
    if (gameLoopActive_.load(std::memory_order_acquire)) {
        lifecycleState = !receiverIsMaster && activeSlaveRoundToken_ != 0
                             ? domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_ARMED
                             : domes_peer_PeerLifecycleState_PEER_LIFECYCLE_STATE_READY;
    }
    if (!espnow::allowedInState(message, receiverRole, lifecycleState)) {
        ESP_LOGW(kTag, "Rejecting state-invalid %s", espnow::msgTypeName(type));
        return false;
    }

    if (!espnow::isDiscoveryMessage(type)) {
        const bool maySelectPeer =
            type == espnow::kJoinGame && !peerFound_.load(std::memory_order_acquire);
        if (!maySelectPeer && !isSelectedPeer(sourceMac)) {
            ESP_LOGW(kTag, "Ignoring %s from non-selected peer", espnow::msgTypeName(type));
            return false;
        }
    }

    switch (type) {
        // Discovery messages
        case espnow::kBeacon:
            handleBeacon(hdr);
            break;
        case espnow::kPing:
            handlePing(hdr);
            break;
        case espnow::kPong:
            handlePong(hdr);
            break;

        // Game control (slave receives)
        case espnow::kJoinGame:
            handleJoinGame(hdr);
            break;
        case espnow::kArmTouch:
            handleArmTouch(data, len);
            break;
        case espnow::kSetColor:
            handleSetColor(data, len);
            break;
        case espnow::kStopAll:
            handleStopAll(hdr);
            break;
        case espnow::kSimulateTouch:
            handleSimulateTouch(data, len);
            break;

        // Game events (master receives)
        case espnow::kTouchEvent:
            handleTouchEvent(data, len);
            break;
        case espnow::kTimeoutEvent:
            handleTimeoutEvent(data, len);
            break;

        default:
            return false;
    }
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

void EspNowService::fillHeader(espnow::MsgHeader& hdr, espnow::MsgType type) {
    hdr.type = static_cast<uint8_t>(type);
    std::memcpy(hdr.senderMac, ourMac_, ESP_NOW_ETH_ALEN);
    hdr.timestampUs = static_cast<uint32_t>(esp_timer_get_time());
}

bool EspNowService::validateReceivedSource(const espnow::MsgHeader& header,
                                           uint8_t sourceMac[ESP_NOW_ETH_ALEN]) const {
    if (!transport_.lastReceivedSource(sourceMac)) {
        ESP_LOGW(kTag, "Dropping ESP-NOW frame without radio source metadata");
        return false;
    }
    if (!espnow::senderMatchesSource(header, sourceMac)) {
        ESP_LOGW(kTag, "Dropping ESP-NOW frame with spoofed sender field");
        return false;
    }
    return true;
}

bool EspNowService::isSelectedPeer(const uint8_t mac[ESP_NOW_ETH_ALEN]) const {
    return mac && peerFound_.load(std::memory_order_acquire) &&
           std::memcmp(mac, peerMac_, ESP_NOW_ETH_ALEN) == 0;
}

uint32_t EspNowService::allocateRoundToken() {
    return roundTokens_.next();
}

bool EspNowService::enterPeerGameMode() {
    if (!modeManager_) {
        return true;
    }
    if (modeManager_->transitionToPeerGame()) {
        return true;
    }

    ESP_LOGE(kTag, "Cannot enter peer GAME mode from %s",
             config::systemModeToString(modeManager_->currentMode()));
    return false;
}

bool EspNowService::ensureTransportReady() {
    if (transport_.isConnected()) {
        return true;
    }

    ESP_LOGW(kTag, "Reinitializing ESP-NOW after a failed or ambiguous TX completion");
    transport_.disconnect();
    TransportError err = transport_.init();
    if (!isOk(err)) {
        ESP_LOGE(kTag, "ESP-NOW recovery failed: %s", transportErrorToString(err));
        return false;
    }

    TRACE_INSTANT(TRACE_ID("EspNow.TransportRecovered"), trace::Category::kEspNow);
    return true;
}

bool EspNowService::sendMsg(const uint8_t* data, size_t len) {
    TransportError err = transport_.send(data, len);
    if (!isOk(err)) {
        ESP_LOGW(kTag, "Broadcast send failed: %s", transportErrorToString(err));
        return false;
    }
    return true;
}

bool EspNowService::sendMsgTo(const uint8_t* mac, const uint8_t* data, size_t len) {
    TransportError err = transport_.sendTo(mac, data, len);
    if (!isOk(err)) {
        ESP_LOGW(kTag, "Unicast send failed: %s", transportErrorToString(err));
        return false;
    }
    return true;
}

void EspNowService::logMac(const char* prefix, const uint8_t* mac) {
    ESP_LOGI(kTag, "%s: %02X:%02X:%02X:%02X:%02X:%02X", prefix, mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
}

}  // namespace domes
