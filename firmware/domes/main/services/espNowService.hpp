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

#include "esp_timer.h"
#include "esp_wifi.h"

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

    /// ITaskRunner interface
    void run() override;
    esp_err_t requestStop() override { running_ = false; return ESP_OK; }
    bool shouldRun() const override { return running_; }

    /// Get number of discovered peers
    uint8_t peerCount() const { return peerCount_.load(std::memory_order_relaxed); }

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

    // Game event handlers (master side)
    void handleTouchEvent(const uint8_t* data, size_t len);
    void handleTimeoutEvent(const uint8_t* data, size_t len);

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
    std::atomic<bool> running_{true};

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
};

}  // namespace domes
