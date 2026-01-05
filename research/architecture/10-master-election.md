# 10 - Master Election Protocol

## AI Agent Instructions

Load this file when:
- Implementing master/follower roles
- Handling master failover
- Debugging network topology issues
- Testing multi-pod scenarios

Prerequisites: `04-communication.md`

---

## Overview

The DOMES network uses a single **master pod** that:
- Bridges BLE (phone) to ESP-NOW (pod network)
- Coordinates game timing across all pods
- Broadcasts clock synchronization messages
- Receives and aggregates touch events

Any pod can become master. The election is **automatic** with a **manual override hook** for testing.

---

## Election Modes

### Auto-Elect Mode (Default)

Pods automatically elect a master based on priority scoring. Used in production.

### Force-Master Mode (Testing)

A specific pod can be forced to become master via:
- NVS configuration (`config/is_master = 1`)
- BLE command from phone app
- Serial console command

```cpp
// Force master mode via NVS
namespace config {
    constexpr const char* kKeyForceMaster = "force_master";  // u8: 0=auto, 1=force
}

// Check at startup
uint8_t forceMaster = 0;
nvs_get_u8(handle, config::kKeyForceMaster, &forceMaster);
if (forceMaster) {
    electionState_ = MasterElectionState::kForcedMaster;
}
```

---

## Election Algorithm

### Priority Score Calculation

Each pod calculates a priority score used for election:

```cpp
struct ElectionPriority {
    uint8_t batteryPercent;     // 0-100: Higher battery = higher priority
    uint8_t uptimeMinutes;      // Capped at 255: Longer uptime = more stable
    uint8_t signalStrength;     // Average RSSI to peers (inverted, higher = better)
    uint32_t randomTieBreaker;  // Random value to break ties
};

uint32_t calculatePriorityScore(const ElectionPriority& p) {
    // Weighted score: battery is most important, then stability, then signal
    return (static_cast<uint32_t>(p.batteryPercent) << 24) |
           (static_cast<uint32_t>(p.uptimeMinutes) << 16) |
           (static_cast<uint32_t>(p.signalStrength) << 8) |
           (p.randomTieBreaker & 0xFF);
}
```

### Election States

```cpp
enum class MasterElectionState : uint8_t {
    kIdle,              // No election in progress
    kDiscovering,       // Discovering peers
    kCampaigning,       // Broadcasting candidacy
    kWaitingForVotes,   // Collecting responses
    kMaster,            // I am the elected master
    kFollower,          // Another pod is master
    kForcedMaster,      // Forced master mode (testing)
};
```

### State Machine

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                  ELECTION STATE MACHINE                  │
                    ├─────────────────────────────────────────────────────────┤
                    │                                                          │
   Boot             │    ┌──────────┐                                          │
    │               │    │   Idle   │◄─────────────────────────────────┐       │
    ▼               │    └────┬─────┘                                  │       │
┌────────┐          │         │ no master heartbeat                    │       │
│ Check  │──force───│────►┌───┴────────┐                               │       │
│  NVS   │          │     │ Discovering │ (broadcast DISCOVER, 500ms)  │       │
└───┬────┘          │     └─────┬──────┘                               │       │
    │ auto          │           │ peers found                          │       │
    ▼               │           ▼                                      │       │
┌──────────┐        │    ┌─────────────┐                               │       │
│  Idle    │────────│────│ Campaigning │ (broadcast CANDIDATE, 200ms)  │       │
└──────────┘        │    └──────┬──────┘                               │       │
                    │           │ all votes received OR timeout        │       │
                    │           ▼                                      │       │
                    │    ┌────────────────┐                            │       │
                    │    │WaitingForVotes │ (collect VOTE msgs, 300ms) │       │
                    │    └───────┬────────┘                            │       │
                    │            │                                     │       │
                    │    ┌───────┴───────┐                             │       │
                    │    │               │                             │       │
                    │    ▼               ▼                             │       │
                    │ I have          Another pod has                  │       │
                    │ highest         higher priority                  │       │
                    │ priority                                         │       │
                    │    │               │                             │       │
                    │    ▼               ▼                             │       │
                    │ ┌────────┐    ┌──────────┐                       │       │
                    │ │ Master │    │ Follower │───master dies────────►│       │
                    │ └────┬───┘    └──────────┘                       │       │
                    │      │                                           │       │
                    │      │ broadcast MASTER_ANNOUNCE every 1s        │       │
                    │      │                                           │       │
                    │      └───────────────────────────────────────────┘       │
                    │                                                          │
                    └─────────────────────────────────────────────────────────┘
```

---

## Protocol Messages

### Message Types

```cpp
enum class ElectionMessageType : uint8_t {
    kDiscover       = 0x30,  // "Who's out there?"
    kDiscoverAck    = 0x31,  // "I'm here" (includes priority)
    kCandidate      = 0x32,  // "I want to be master" (includes priority)
    kVote           = 0x33,  // "I vote for you" or "I have higher priority"
    kMasterAnnounce = 0x34,  // "I am the master" (heartbeat)
    kMasterResign   = 0x35,  // "I'm stepping down" (graceful handoff)
};
```

### Message Structures

```cpp
#pragma pack(push, 1)

struct DiscoverMsg {
    MessageHeader header;
    uint8_t protocolVersion;   // For compatibility
};

struct DiscoverAckMsg {
    MessageHeader header;
    uint8_t podId;
    uint32_t priorityScore;
    uint8_t currentMasterMac[6];  // 00:00:00:00:00:00 if none known
};

struct CandidateMsg {
    MessageHeader header;
    uint8_t podId;
    uint32_t priorityScore;
    uint8_t electionRound;      // Incremented each election
};

struct VoteMsg {
    MessageHeader header;
    uint8_t voterPodId;
    uint8_t candidateMac[6];    // Who I'm voting for
    uint32_t voterPriority;     // My priority (for comparison)
    bool iAcceptYou;            // true = you win, false = I have higher priority
};

struct MasterAnnounceMsg {
    MessageHeader header;
    uint8_t masterPodId;
    uint8_t peerCount;          // How many pods in network
    uint32_t uptimeSeconds;     // Master's uptime
    uint8_t batteryPercent;     // Master's battery
};

struct MasterResignMsg {
    MessageHeader header;
    uint8_t reason;             // 0=low battery, 1=user request, 2=shutting down
    uint8_t suggestedSuccessorMac[6];  // Hint for next master
};

#pragma pack(pop)
```

---

## Election Process

### Phase 1: Discovery (500ms)

```cpp
void ElectionService::startDiscovery() {
    state_ = MasterElectionState::kDiscovering;
    peers_.clear();

    // Broadcast discover message
    DiscoverMsg msg;
    msg.header = makeHeader(MessageType::kDiscover, nextSeq());
    msg.protocolVersion = kProtocolVersion;

    comm_.broadcast(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    // Wait for responses
    discoveryTimer_ = esp_timer_get_time();
    discoveryTimeoutMs_ = 500;
}

void ElectionService::onDiscoverAck(const DiscoverAckMsg& msg, const uint8_t* srcMac) {
    PeerInfo peer;
    memcpy(peer.mac, srcMac, 6);
    peer.podId = msg.podId;
    peer.priorityScore = msg.priorityScore;
    peer.lastSeenUs = esp_timer_get_time();

    peers_.push_back(peer);

    // If they know a master, check if it's still alive
    if (!isZeroMac(msg.currentMasterMac)) {
        knownMasterMac_ = msg.currentMasterMac;
    }
}
```

### Phase 2: Campaigning (200ms)

```cpp
void ElectionService::startCampaigning() {
    state_ = MasterElectionState::kCampaigning;

    // Calculate my priority
    myPriority_ = calculatePriorityScore({
        .batteryPercent = power_.getBatteryPercent(),
        .uptimeMinutes = static_cast<uint8_t>(std::min(255UL, uptimeSeconds_ / 60)),
        .signalStrength = getAverageRssi(),
        .randomTieBreaker = esp_random(),
    });

    // Broadcast candidacy
    CandidateMsg msg;
    msg.header = makeHeader(MessageType::kCandidate, nextSeq());
    msg.podId = myPodId_;
    msg.priorityScore = myPriority_;
    msg.electionRound = electionRound_;

    comm_.broadcast(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    campaignTimer_ = esp_timer_get_time();
    campaignTimeoutMs_ = 200;
}
```

### Phase 3: Vote Collection (300ms)

```cpp
void ElectionService::onCandidate(const CandidateMsg& msg, const uint8_t* srcMac) {
    // Compare priorities
    bool theyWin = msg.priorityScore > myPriority_;

    // Send vote
    VoteMsg vote;
    vote.header = makeHeader(MessageType::kVote, nextSeq());
    vote.voterPodId = myPodId_;
    memcpy(vote.candidateMac, srcMac, 6);
    vote.voterPriority = myPriority_;
    vote.iAcceptYou = theyWin;

    comm_.send(srcMac, reinterpret_cast<uint8_t*>(&vote), sizeof(vote));

    if (theyWin) {
        // I should become follower
        state_ = MasterElectionState::kFollower;
        memcpy(masterMac_, srcMac, 6);
    }
}

void ElectionService::onVote(const VoteMsg& vote, const uint8_t* srcMac) {
    if (vote.iAcceptYou) {
        votesReceived_++;
    } else if (vote.voterPriority > myPriority_) {
        // They have higher priority, I should yield
        state_ = MasterElectionState::kFollower;
        memcpy(masterMac_, srcMac, 6);
    }
}
```

### Phase 4: Master Announcement

```cpp
void ElectionService::declareVictory() {
    state_ = MasterElectionState::kMaster;

    ESP_LOGI(kTag, "I am now master (pod %d, priority %lu)",
             myPodId_, myPriority_);

    // Start heartbeat
    startMasterHeartbeat();

    // Notify application layer
    if (onMasterElected_) {
        onMasterElected_(true);  // true = I am master
    }
}

void ElectionService::startMasterHeartbeat() {
    // Create repeating timer for announcements
    esp_timer_create_args_t args = {
        .callback = masterHeartbeatCallback,
        .arg = this,
        .name = "master_hb",
    };
    esp_timer_create(&args, &heartbeatTimer_);
    esp_timer_start_periodic(heartbeatTimer_, kHeartbeatIntervalUs);
}

void ElectionService::sendMasterAnnounce() {
    MasterAnnounceMsg msg;
    msg.header = makeHeader(MessageType::kMasterAnnounce, nextSeq());
    msg.masterPodId = myPodId_;
    msg.peerCount = peers_.size();
    msg.uptimeSeconds = uptimeSeconds_;
    msg.batteryPercent = power_.getBatteryPercent();

    comm_.broadcast(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
}
```

---

## Failover Handling

### Master Death Detection

Followers monitor master heartbeats:

```cpp
void ElectionService::onMasterAnnounce(const MasterAnnounceMsg& msg, const uint8_t* srcMac) {
    if (state_ == MasterElectionState::kFollower) {
        lastMasterHeartbeatUs_ = esp_timer_get_time();
        masterBatteryPercent_ = msg.batteryPercent;
    }
}

void ElectionService::tick() {
    if (state_ == MasterElectionState::kFollower) {
        uint32_t silenceMs = (esp_timer_get_time() - lastMasterHeartbeatUs_) / 1000;

        if (silenceMs > kMasterTimeoutMs) {  // 3000ms
            ESP_LOGW(kTag, "Master lost, starting new election");
            electionRound_++;
            startDiscovery();
        }
    }
}
```

### Graceful Handoff

When master needs to step down (low battery, shutdown):

```cpp
void ElectionService::resignAsMaster(uint8_t reason) {
    if (state_ != MasterElectionState::kMaster) return;

    // Find best successor
    PeerInfo* best = nullptr;
    uint32_t bestPriority = 0;
    for (auto& peer : peers_) {
        if (peer.priorityScore > bestPriority) {
            bestPriority = peer.priorityScore;
            best = &peer;
        }
    }

    // Announce resignation
    MasterResignMsg msg;
    msg.header = makeHeader(MessageType::kMasterResign, nextSeq());
    msg.reason = reason;
    if (best) {
        memcpy(msg.suggestedSuccessorMac, best->mac, 6);
    } else {
        memset(msg.suggestedSuccessorMac, 0, 6);
    }

    comm_.broadcast(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    // Transition to idle
    state_ = MasterElectionState::kIdle;
    esp_timer_stop(heartbeatTimer_);
}
```

### Handling Resign Message

```cpp
void ElectionService::onMasterResign(const MasterResignMsg& msg, const uint8_t* srcMac) {
    ESP_LOGI(kTag, "Master resigned (reason=%d)", msg.reason);

    // Check if I'm the suggested successor
    uint8_t myMac[6];
    comm_.getOwnMac(myMac);

    if (memcmp(msg.suggestedSuccessorMac, myMac, 6) == 0) {
        // I'm the suggested successor, fast-track to master
        ESP_LOGI(kTag, "I am the suggested successor");
        declareVictory();
    } else {
        // Start normal election
        electionRound_++;
        startDiscovery();
    }
}
```

---

## Edge Cases

### Split Brain Prevention

If two pods both think they're master (network partition healed):

```cpp
void ElectionService::onMasterAnnounce(const MasterAnnounceMsg& msg, const uint8_t* srcMac) {
    if (state_ == MasterElectionState::kMaster) {
        // Conflict! Another master exists
        ESP_LOGW(kTag, "Master conflict detected!");

        // Compare priorities - lower priority yields
        uint32_t theirPriority = calculatePriorityFromAnnounce(msg);
        if (theirPriority > myPriority_) {
            ESP_LOGI(kTag, "Yielding to higher priority master");
            state_ = MasterElectionState::kFollower;
            memcpy(masterMac_, srcMac, 6);
            esp_timer_stop(heartbeatTimer_);
        }
        // If I have higher priority, ignore their announcement
        // They will see mine and yield
    }
}
```

### Late Joiner

Pod joins network where master already exists:

```cpp
void ElectionService::startDiscovery() {
    // ... normal discovery ...
}

void ElectionService::onDiscoverAck(const DiscoverAckMsg& msg, const uint8_t* srcMac) {
    // If a master is known, skip election
    if (!isZeroMac(msg.currentMasterMac)) {
        ESP_LOGI(kTag, "Existing master found, becoming follower");
        state_ = MasterElectionState::kFollower;
        memcpy(masterMac_, msg.currentMasterMac, 6);
        discoveryComplete_ = true;
    }
}
```

### No Other Pods

Single pod in network:

```cpp
void ElectionService::discoveryTimeout() {
    if (peers_.empty()) {
        ESP_LOGI(kTag, "No peers found, becoming standalone master");
        state_ = MasterElectionState::kMaster;
        startMasterHeartbeat();  // Still heartbeat in case pods appear
    } else {
        startCampaigning();
    }
}
```

---

## Timing Constants

```cpp
namespace election {
    constexpr uint32_t kDiscoveryTimeoutMs = 500;    // Wait for peer responses
    constexpr uint32_t kCampaignTimeoutMs = 200;     // Broadcast candidacy
    constexpr uint32_t kVoteTimeoutMs = 300;         // Collect votes
    constexpr uint32_t kHeartbeatIntervalMs = 1000;  // Master announcement
    constexpr uint32_t kMasterTimeoutMs = 3000;      // Miss 3 heartbeats = dead
    constexpr uint32_t kRandomDelayMaxMs = 100;      // Jitter to avoid collision
    constexpr uint8_t kMaxElectionRetries = 3;       // Before giving up
}
```

---

## Testing Hooks

### Force Master via Serial

```cpp
void handleSerialCommand(const char* cmd) {
    if (strcmp(cmd, "force_master") == 0) {
        election_.forceBecomeMaster();
    } else if (strcmp(cmd, "resign_master") == 0) {
        election_.resignAsMaster(1);  // User request
    } else if (strcmp(cmd, "election_status") == 0) {
        ESP_LOGI(kTag, "State: %s, Priority: %lu, Peers: %d",
                 stateToString(election_.state()),
                 election_.myPriority(),
                 election_.peerCount());
    }
}
```

### Force Master via BLE

```cpp
// In BLE write handler
case kCharForceElection:
    if (data[0] == 0x01) {
        election_.forceBecomeMaster();
    } else if (data[0] == 0x02) {
        election_.resignAsMaster(1);
    }
    break;
```

### Simulation Mode

For unit testing without hardware:

```cpp
class MockElectionTransport : public IElectionTransport {
public:
    std::vector<std::pair<uint8_t[6], std::vector<uint8_t>>> sentMessages;
    std::queue<std::pair<uint8_t[6], std::vector<uint8_t>>> incomingMessages;

    esp_err_t send(const uint8_t* mac, const uint8_t* data, size_t len) override {
        // Capture for verification
        sentMessages.push_back({mac, std::vector<uint8_t>(data, data + len)});
        return ESP_OK;
    }

    void injectMessage(const uint8_t* srcMac, const uint8_t* data, size_t len) {
        incomingMessages.push({srcMac, std::vector<uint8_t>(data, data + len)});
    }
};
```

---

## Integration with Application

### Callbacks

```cpp
class ElectionService {
public:
    using MasterElectedCallback = std::function<void(bool iAmMaster)>;
    using MasterChangedCallback = std::function<void(const uint8_t* newMasterMac)>;

    void onMasterElected(MasterElectedCallback cb) { onMasterElected_ = cb; }
    void onMasterChanged(MasterChangedCallback cb) { onMasterChanged_ = cb; }

    bool isMaster() const { return state_ == MasterElectionState::kMaster; }
    void getMasterMac(uint8_t* out) const { memcpy(out, masterMac_, 6); }
};
```

### Usage in GameEngine

```cpp
void GameEngine::init() {
    election_.onMasterElected([this](bool iAmMaster) {
        if (iAmMaster) {
            // Start BLE advertising
            ble_.startAdvertising();
            // Initialize game coordination
            initMasterRole();
        } else {
            // Stop BLE advertising (only master needs it)
            ble_.stopAdvertising();
            // Wait for commands from master
            initFollowerRole();
        }
    });

    election_.onMasterChanged([this](const uint8_t* newMaster) {
        // Resync timing with new master
        timing_.resetSync();
    });
}
```

---

## Verification Checklist

| Test Case | Expected Result |
|-----------|-----------------|
| Single pod boots | Becomes master after discovery timeout |
| Two pods boot simultaneously | Higher priority becomes master |
| Pod joins existing network | Becomes follower, knows master |
| Master battery drops to 15% | Master resigns, new election |
| Master loses power suddenly | Followers detect timeout, elect new |
| Network partition heals | Lower priority master yields |
| Force master via NVS | Pod becomes master regardless of priority |
| Force master via BLE | Pod becomes master immediately |

---

*Prerequisites: 04-communication.md*
*Related: 11-clock-sync.md, 05-game-engine.md*
