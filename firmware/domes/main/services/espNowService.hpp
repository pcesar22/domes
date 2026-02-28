#pragma once

/**
 * @file espNowService.hpp
 * @brief ESP-NOW game service: discovery, role negotiation, and drill orchestration
 *
 * Replaces EspNowDiscovery. Single ITaskRunner with 3 phases:
 *   Phase 1: Peer discovery via beacons + ping-pong RTT
 *   Phase 2: Role assignment (lower MAC = master)
 *   Phase 3: Game loop (master orchestrates drill, slave responds)
 */

#include "espNowProtocol.hpp"
#include "interfaces/iTaskRunner.hpp"
#include "transport/espNowTransport.hpp"
#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

// Forward declarations to avoid pulling in full headers
namespace domes::game {
    class GameEngine;
    struct ArmConfig;
    struct GameEvent;
}

namespace domes {

class LedService;
class InjectableTouchDriver;

namespace config {
    class ModeManager;
}

/// Info about a discovered peer
struct DiscoveredPeer {
    uint8_t mac[ESP_NOW_ETH_ALEN] = {};
    int64_t firstSeenUs = 0;
    int64_t lastSeenUs = 0;
    uint32_t beaconCount = 0;
    uint32_t lastRttUs = 0;
    bool pingSent = false;
    int64_t pingSentAtUs = 0;
};

/// Maximum number of discovered peers
static constexpr size_t kMaxDiscoveredPeers = 8;

/// Maximum rounds for latency benchmark
static constexpr uint32_t kBenchMaxRounds = 1000;

/// Benchmark results
struct BenchmarkResult {
    uint32_t roundsCompleted = 0;
    uint32_t roundsFailed = 0;
    uint32_t minRttUs = 0;
    uint32_t maxRttUs = 0;
    uint32_t meanRttUs = 0;
    uint32_t p50RttUs = 0;
    uint32_t p95RttUs = 0;
    uint32_t p99RttUs = 0;
};

/**
 * @brief ESP-NOW game service
 *
 * Discovery -> Role assignment -> Game loop.
 * After discovery and role assignment, master runs a drill that alternates
 * arming self and peer. Slave responds to commands from master.
 */
class EspNowService : public ITaskRunner {
public:
    explicit EspNowService(EspNowTransport& transport);

    /// Wire dependencies (set before task starts)
    void setGameEngine(game::GameEngine* engine) { gameEngine_ = engine; }
    void setLedService(LedService* led) { ledService_ = led; }
    void setModeManager(config::ModeManager* modes) { modeManager_ = modes; }
    void setInjectableTouchDriver(InjectableTouchDriver* driver) { injectableTouch_ = driver; }
    /// Enable/disable the service (called by CLI feature toggle, NOT mode manager)
    void setFeatureEnabled(bool enabled) {
        featurePaused_.store(!enabled, std::memory_order_relaxed);
        if (enabled) {
            ESP_LOGI("espnow", "ESP-NOW feature re-enabled");
        } else {
            ESP_LOGI("espnow", "ESP-NOW feature disabled by user");
        }
    }

    /// Configure sim drill mode (auto-inject touches during drills)
    void setSimMode(bool enabled, uint32_t delayMs = 500, uint8_t padIndex = 0) {
        simDelayMs_.store(delayMs, std::memory_order_relaxed);
        simPadIndex_.store(padIndex, std::memory_order_relaxed);
        simMode_.store(enabled, std::memory_order_release);  // release fence after data
    }

    /// Check if sim mode is active
    bool isSimMode() const { return simMode_.load(std::memory_order_acquire); }
    uint32_t simDelayMs() const { return simDelayMs_.load(std::memory_order_relaxed); }
    uint8_t simPadIndex() const { return simPadIndex_.load(std::memory_order_relaxed); }

    /// ITaskRunner interface
    void run() override;
    esp_err_t requestStop() override { running_ = false; return ESP_OK; }
    bool shouldRun() const override { return running_; }

    /// Get number of discovered peers
    uint8_t peerCount() const { return peerCount_.load(std::memory_order_relaxed); }

    /// Get last measured RTT in microseconds (from ping-pong)
    uint32_t lastRttUs() const {
        if (peerCount_.load(std::memory_order_relaxed) == 0) return 0;
        return peers_[0].lastRttUs;
    }

    /// Get discovery state as string
    const char* discoveryState() const {
        if (!running_.load(std::memory_order_relaxed)) return "stopped";
        if (featurePaused_.load(std::memory_order_relaxed)) return "disabled";
        if (peerFound_) return isMaster_ ? "master" : "slave";
        if (peerCount_.load(std::memory_order_relaxed) > 0) return "found-peer";
        return "searching";
    }

    /// Start latency benchmark (non-blocking, results polled via isBenchmarkDone)
    /// Returns false if no peer or benchmark already running
    bool startBenchmark(uint32_t rounds);

    /// Check if benchmark is complete
    bool isBenchmarkDone() const { return benchmarkDone_.load(std::memory_order_acquire); }

    /// Get benchmark results (only valid after isBenchmarkDone() returns true)
    BenchmarkResult getBenchmarkResult() const { return benchmarkResult_; }

    /// Get peer info for observability (returns count of peers copied)
    uint8_t getPeers(DiscoveredPeer* out, uint8_t maxPeers) const {
        uint8_t count = peerCount_.load(std::memory_order_relaxed);
        if (count > maxPeers) count = maxPeers;
        for (uint8_t i = 0; i < count; ++i) {
            out[i] = peers_[i];
        }
        return count;
    }

private:
    // =========================================================================
    // Phase 1: Discovery
    // =========================================================================
    void runDiscovery();
    void sendBeacon();
    void sendPing(const uint8_t* peerMac);

    // Discovery message handlers
    void handleBeacon(const espnow::MsgHeader* hdr);
    void handlePing(const espnow::MsgHeader* hdr);
    void handlePong(const espnow::MsgHeader* hdr);

    // Peer tracking
    DiscoveredPeer* findOrAddPeer(const uint8_t* mac);
    const DiscoveredPeer* getPeer(uint8_t index) const;

    // =========================================================================
    // Phase 2: Role assignment
    // =========================================================================
    void assignRole();

    // =========================================================================
    // Phase 3: Game loop
    // =========================================================================
    void runMaster();
    void runSlave();

    // Game command handlers (slave side)
    void handleJoinGame(const espnow::MsgHeader* hdr);
    void handleArmTouch(const uint8_t* data, size_t len);
    void handleSetColor(const uint8_t* data, size_t len);
    void handleStopAll(const espnow::MsgHeader* hdr);
    void handleSimulateTouch(const uint8_t* data, size_t len);

    // Game event handlers (master side)
    void handleTouchEvent(const uint8_t* data, size_t len);
    void handleTimeoutEvent(const uint8_t* data, size_t len);

    // =========================================================================
    // Benchmark
    // =========================================================================
    void runBenchmark();

    // =========================================================================
    // Shared helpers
    // =========================================================================
    void handleReceived(const uint8_t* data, size_t len);
    void fillHeader(espnow::MsgHeader& hdr, espnow::MsgType type);
    void sendMsg(const uint8_t* data, size_t len);
    void sendMsgTo(const uint8_t* mac, const uint8_t* data, size_t len);
    void logMac(const char* prefix, const uint8_t* mac);

    // =========================================================================
    // State
    // =========================================================================
    EspNowTransport& transport_;
    game::GameEngine* gameEngine_ = nullptr;
    LedService* ledService_ = nullptr;
    config::ModeManager* modeManager_ = nullptr;
    InjectableTouchDriver* injectableTouch_ = nullptr;
    std::atomic<bool> running_{true};
    std::atomic<bool> featurePaused_{true};   // start paused; setFeatureEnabled(true) unpauses

    // Sim drill mode state
    std::atomic<bool> simMode_{false};
    std::atomic<uint32_t> simDelayMs_{500};   // 0 = miss (no injection)
    std::atomic<uint8_t> simPadIndex_{0};

    // Identity
    uint8_t ourMac_[ESP_NOW_ETH_ALEN] = {};
    uint8_t peerMac_[ESP_NOW_ETH_ALEN] = {};
    bool peerFound_ = false;
    bool isMaster_ = false;

    // Peer tracking
    std::array<DiscoveredPeer, kMaxDiscoveredPeers> peers_ = {};
    std::atomic<uint8_t> peerCount_{0};

    // Join game flag (set when slave receives JOIN_GAME during discovery)
    std::atomic<bool> joinGameReceived_{false};

    // Master drill state — written by master's local game event callback (Core 1),
    // read by runMaster loop (Core 0). eventReceived_ acts as the release/acquire
    // fence: writer sets data fields first, then stores eventReceived_ with release;
    // reader loads eventReceived_ with acquire, then reads data fields.
    std::atomic<bool> eventReceived_{false};
    bool lastEventWasHit_ = false;
    uint32_t lastReactionTimeUs_ = 0;
    uint8_t lastPadIndex_ = 0;

    // Slave event state — written by game_tick callback (Core 1), read by
    // runSlave loop (Core 0). slaveEventPending_ is the release/acquire fence.
    // We use flags instead of sending directly from the callback because
    // the callback fires on the game_tick task (Core 1, small stack) and
    // sendMsgTo() is a blocking call that shouldn't run there.
    std::atomic<bool> slaveEventPending_{false};
    bool slaveEventWasHit_ = false;
    uint32_t slaveReactionTimeUs_ = 0;
    uint8_t slavePadIndex_ = 0;

    // Flag to break slave out of runSlave() loop on STOP_ALL
    std::atomic<bool> stopAllReceived_{false};

    // Benchmark state
    std::atomic<bool> benchmarkRequested_{false};
    std::atomic<bool> benchmarkDone_{false};
    uint32_t benchmarkRounds_ = 0;
    BenchmarkResult benchmarkResult_;
    std::array<uint32_t, kBenchMaxRounds> benchmarkRtts_ = {};
};

}  // namespace domes
