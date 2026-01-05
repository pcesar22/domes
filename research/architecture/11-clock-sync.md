# 11 - Clock Synchronization

## AI Agent Instructions

Load this file when:
- Implementing timing synchronization between pods
- Debugging reaction time measurement accuracy
- Understanding timestamp handling
- Optimizing latency-sensitive operations

Prerequisites: `04-communication.md`, `10-master-election.md`

---

## Overview

DOMES requires ±1ms timing accuracy across all pods for:
- Synchronized LED animations
- Accurate reaction time measurement
- Coordinated audio/haptic feedback

The synchronization uses an **NTP-like algorithm** with RTT measurement, not hardcoded delays.

---

## Algorithm: Precision Time Protocol (Simplified)

### Why Not Hardcoded Delay?

The original design assumed 500µs one-way delay. This is wrong because:
1. ESP-NOW latency varies (200µs - 2ms depending on load)
2. BLE coexistence affects timing
3. Radio interference causes jitter

### NTP-Style Sync

```
     Master                           Follower
        │                                 │
        │◄─────────── T1 ────────────────│  (Master send time)
        │                                 │
        │              ┌──────────────────│  T2 (Follower receive)
        │              │                  │
        │              │ Processing       │
        │              │                  │
        │              └──────────────────│  T3 (Follower send)
        │                                 │
        │◄─────────── T4 ────────────────│  (Master receive time)
        │                                 │

    RTT = (T4 - T1) - (T3 - T2)
    Offset = ((T2 - T1) + (T3 - T4)) / 2
```

### Formulas

```cpp
// Round-trip time (network delay only, excluding processing)
int64_t rtt = (t4 - t1) - (t3 - t2);

// Clock offset (positive = follower is ahead)
int64_t offset = ((t2 - t1) + (t3 - t4)) / 2;

// One-way delay estimate
int64_t oneWayDelay = rtt / 2;
```

---

## Protocol Messages

```cpp
#pragma pack(push, 1)

// Master → Follower: Initiate sync
struct SyncRequestMsg {
    MessageHeader header;
    uint64_t t1;          // Master's send timestamp (µs since boot)
    uint8_t syncId;       // Sequence for matching response
};

// Follower → Master: Response
struct SyncResponseMsg {
    MessageHeader header;
    uint64_t t1;          // Echo of master's t1
    uint64_t t2;          // Follower's receive timestamp
    uint64_t t3;          // Follower's send timestamp
    uint8_t syncId;       // Echo of sync ID
    int32_t currentOffset;// Follower's current offset estimate
};

// Master → All: Broadcast current time (for followers joining late)
struct TimeBeaconMsg {
    MessageHeader header;
    uint64_t masterTime;  // Master's current time
    uint8_t quality;      // Sync quality indicator (0-100)
};

#pragma pack(pop)
```

---

## TimingService Implementation

```cpp
// services/timing_service.hpp
#pragma once

#include "esp_err.h"
#include <cstdint>
#include <functional>

namespace domes {

struct SyncStats {
    int64_t offsetUs;           // Current offset from master
    int64_t rttUs;              // Last measured RTT
    int64_t jitterUs;           // Offset variance
    uint32_t syncCount;         // Successful syncs
    uint32_t lastSyncAgeMs;     // Time since last sync
    uint8_t quality;            // 0-100 sync quality
};

class TimingService {
public:
    explicit TimingService(CommService& comm);

    esp_err_t init();

    // --- Time Queries ---

    /**
     * @brief Get synchronized network time (microseconds)
     *
     * Returns local time adjusted by offset to match master.
     * If not synced, returns local time.
     */
    uint64_t getSyncedTimeUs() const;

    /**
     * @brief Get local time (not adjusted)
     */
    uint64_t getLocalTimeUs() const;

    /**
     * @brief Convert local timestamp to network time
     */
    uint64_t localToNetwork(uint64_t localUs) const;

    /**
     * @brief Convert network timestamp to local time
     */
    uint64_t networkToLocal(uint64_t networkUs) const;

    // --- Sync Control ---

    /**
     * @brief Start sync process (master calls this for each follower)
     */
    esp_err_t requestSync(const uint8_t* followerMac);

    /**
     * @brief Handle incoming sync request (follower)
     */
    void onSyncRequest(const SyncRequestMsg& msg, const uint8_t* srcMac);

    /**
     * @brief Handle incoming sync response (master)
     */
    void onSyncResponse(const SyncResponseMsg& msg, const uint8_t* srcMac);

    /**
     * @brief Handle time beacon (late joiner)
     */
    void onTimeBeacon(const TimeBeaconMsg& msg);

    // --- Status ---

    bool isSynced() const { return synced_; }
    SyncStats getStats() const;

    /**
     * @brief Reset sync state (call when master changes)
     */
    void resetSync();

    // --- Master Operations ---

    /**
     * @brief Start periodic sync broadcasts (master only)
     */
    esp_err_t startMasterSync(uint32_t intervalMs = 500);

    /**
     * @brief Stop periodic sync (when no longer master)
     */
    void stopMasterSync();

private:
    CommService& comm_;

    // Sync state
    int64_t offsetUs_ = 0;          // Offset from master
    int64_t rttUs_ = 0;             // Last measured RTT
    int64_t jitterUs_ = 0;          // Offset variance
    uint64_t lastSyncUs_ = 0;       // Last successful sync time
    uint32_t syncCount_ = 0;
    bool synced_ = false;

    // Filtering
    static constexpr size_t kFilterSize = 8;
    int64_t offsetHistory_[kFilterSize] = {};
    size_t historyIndex_ = 0;
    bool historyFull_ = false;

    // Master mode
    esp_timer_handle_t syncTimer_ = nullptr;
    uint8_t nextSyncId_ = 0;

    // Pending sync tracking
    struct PendingSync {
        uint8_t syncId;
        uint64_t t1;
        uint8_t mac[6];
        bool valid;
    };
    static constexpr size_t kMaxPending = 8;
    PendingSync pending_[kMaxPending] = {};

    void updateOffset(int64_t newOffset, int64_t rtt);
    int64_t medianFilter(int64_t value);
    static void syncTimerCallback(void* arg);
};

}  // namespace domes
```

```cpp
// services/timing_service.cpp
#include "timing_service.hpp"
#include "esp_timer.h"
#include "esp_log.h"
#include <algorithm>

namespace domes {

namespace {
    constexpr const char* kTag = "timing";
    constexpr uint32_t kSyncTimeoutMs = 100;
    constexpr uint32_t kStaleThresholdMs = 5000;
    constexpr float kAlphaInitial = 0.5f;    // Fast convergence
    constexpr float kAlphaSteady = 0.1f;     // Stable tracking
    constexpr uint32_t kConvergenceCount = 5;
}

TimingService::TimingService(CommService& comm)
    : comm_(comm) {
}

esp_err_t TimingService::init() {
    ESP_LOGI(kTag, "Initialized");
    return ESP_OK;
}

uint64_t TimingService::getLocalTimeUs() const {
    return esp_timer_get_time();
}

uint64_t TimingService::getSyncedTimeUs() const {
    return getLocalTimeUs() - offsetUs_;
}

uint64_t TimingService::localToNetwork(uint64_t localUs) const {
    return localUs - offsetUs_;
}

uint64_t TimingService::networkToLocal(uint64_t networkUs) const {
    return networkUs + offsetUs_;
}

// --- Master Operations ---

esp_err_t TimingService::startMasterSync(uint32_t intervalMs) {
    if (syncTimer_) {
        esp_timer_stop(syncTimer_);
        esp_timer_delete(syncTimer_);
    }

    esp_timer_create_args_t args = {
        .callback = syncTimerCallback,
        .arg = this,
        .name = "sync_timer",
    };

    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &syncTimer_), kTag, "Timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(syncTimer_, intervalMs * 1000),
                        kTag, "Timer start failed");

    ESP_LOGI(kTag, "Master sync started, interval=%lums", intervalMs);
    return ESP_OK;
}

void TimingService::stopMasterSync() {
    if (syncTimer_) {
        esp_timer_stop(syncTimer_);
        esp_timer_delete(syncTimer_);
        syncTimer_ = nullptr;
    }
}

void TimingService::syncTimerCallback(void* arg) {
    auto* self = static_cast<TimingService*>(arg);

    // Broadcast time beacon for late joiners
    TimeBeaconMsg beacon;
    beacon.header = makeHeader(MessageType::kTimeBeacon, 0);
    beacon.masterTime = self->getLocalTimeUs();
    beacon.quality = 100;  // Master is always perfect

    self->comm_.broadcast(reinterpret_cast<uint8_t*>(&beacon), sizeof(beacon));
}

esp_err_t TimingService::requestSync(const uint8_t* followerMac) {
    // Find free pending slot
    PendingSync* slot = nullptr;
    for (auto& p : pending_) {
        if (!p.valid) {
            slot = &p;
            break;
        }
    }

    if (!slot) {
        ESP_LOGW(kTag, "No pending slots available");
        return ESP_ERR_NO_MEM;
    }

    // Record T1 and send request
    slot->syncId = nextSyncId_++;
    slot->t1 = getLocalTimeUs();
    memcpy(slot->mac, followerMac, 6);
    slot->valid = true;

    SyncRequestMsg msg;
    msg.header = makeHeader(MessageType::kSyncRequest, slot->syncId);
    msg.t1 = slot->t1;
    msg.syncId = slot->syncId;

    return comm_.send(followerMac, reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
}

// --- Follower Operations ---

void TimingService::onSyncRequest(const SyncRequestMsg& msg, const uint8_t* srcMac) {
    uint64_t t2 = getLocalTimeUs();  // Receive time

    // Prepare response
    SyncResponseMsg resp;
    resp.header = makeHeader(MessageType::kSyncResponse, msg.syncId);
    resp.t1 = msg.t1;
    resp.t2 = t2;
    resp.t3 = getLocalTimeUs();  // Send time
    resp.syncId = msg.syncId;
    resp.currentOffset = static_cast<int32_t>(offsetUs_);

    comm_.send(srcMac, reinterpret_cast<uint8_t*>(&resp), sizeof(resp));
}

void TimingService::onSyncResponse(const SyncResponseMsg& msg, const uint8_t* srcMac) {
    uint64_t t4 = getLocalTimeUs();  // Receive time

    // Find matching pending request
    PendingSync* slot = nullptr;
    for (auto& p : pending_) {
        if (p.valid && p.syncId == msg.syncId &&
            memcmp(p.mac, srcMac, 6) == 0) {
            slot = &p;
            break;
        }
    }

    if (!slot) {
        ESP_LOGW(kTag, "Unexpected sync response (id=%d)", msg.syncId);
        return;
    }

    // Verify t1 matches
    if (slot->t1 != msg.t1) {
        ESP_LOGW(kTag, "T1 mismatch in sync response");
        slot->valid = false;
        return;
    }

    // Calculate RTT and offset
    // RTT = total time - processing time at follower
    int64_t rtt = (t4 - slot->t1) - (msg.t3 - msg.t2);

    // Offset = average of forward and backward delays
    // If offset > 0, follower clock is ahead of master
    int64_t offset = ((int64_t)(msg.t2 - slot->t1) + (int64_t)(msg.t3 - t4)) / 2;

    // Sanity check
    if (rtt < 0 || rtt > 50000) {  // > 50ms is suspicious
        ESP_LOGW(kTag, "Suspicious RTT: %lld µs", rtt);
        slot->valid = false;
        return;
    }

    ESP_LOGD(kTag, "Sync to %02X:%02X: RTT=%lld µs, offset=%lld µs",
             srcMac[4], srcMac[5], rtt, offset);

    // Free the slot
    slot->valid = false;

    // This is master receiving response - we don't update our offset
    // We could store per-follower offset for diagnostics
}

void TimingService::onTimeBeacon(const TimeBeaconMsg& msg) {
    if (synced_ && syncCount_ >= kConvergenceCount) {
        // Already synced with good quality, use beacon for drift check
        int64_t localNow = getLocalTimeUs();
        int64_t networkNow = localNow - offsetUs_;
        int64_t drift = msg.masterTime - networkNow;

        if (std::abs(drift) > 2000) {  // > 2ms drift
            ESP_LOGW(kTag, "Clock drift detected: %lld µs", drift);
            // Apply gradual correction
            updateOffset(offsetUs_ + drift, rttUs_);
        }
        return;
    }

    // Not yet synced - use beacon for rough initial sync
    // This is less accurate but gets us close quickly
    int64_t localNow = getLocalTimeUs();
    int64_t roughOffset = localNow - msg.masterTime;

    // Assume ~500µs one-way delay for rough sync
    roughOffset -= 500;

    updateOffset(roughOffset, 1000);  // Assume 1ms RTT
    ESP_LOGI(kTag, "Rough sync from beacon: offset=%lld µs", offsetUs_);
}

// --- Offset Filtering ---

void TimingService::updateOffset(int64_t newOffset, int64_t rtt) {
    // Store RTT
    rttUs_ = rtt;

    // Apply median filter to reject outliers
    int64_t filtered = medianFilter(newOffset);

    // Apply exponential moving average
    float alpha = (syncCount_ < kConvergenceCount) ? kAlphaInitial : kAlphaSteady;

    if (!synced_) {
        // First sync - use directly
        offsetUs_ = filtered;
        synced_ = true;
    } else {
        // Smooth update
        offsetUs_ = static_cast<int64_t>(alpha * filtered + (1.0f - alpha) * offsetUs_);
    }

    // Update jitter (variance of offset)
    int64_t error = newOffset - offsetUs_;
    jitterUs_ = static_cast<int64_t>(0.1f * std::abs(error) + 0.9f * jitterUs_);

    lastSyncUs_ = getLocalTimeUs();
    syncCount_++;

    ESP_LOGD(kTag, "Offset updated: %lld µs (jitter=%lld µs, quality=%d)",
             offsetUs_, jitterUs_, getStats().quality);
}

int64_t TimingService::medianFilter(int64_t value) {
    // Store in history
    offsetHistory_[historyIndex_] = value;
    historyIndex_ = (historyIndex_ + 1) % kFilterSize;
    if (historyIndex_ == 0) historyFull_ = true;

    // Get median
    size_t count = historyFull_ ? kFilterSize : historyIndex_;
    if (count == 0) return value;

    int64_t sorted[kFilterSize];
    memcpy(sorted, offsetHistory_, count * sizeof(int64_t));
    std::sort(sorted, sorted + count);

    return sorted[count / 2];
}

void TimingService::resetSync() {
    synced_ = false;
    syncCount_ = 0;
    offsetUs_ = 0;
    rttUs_ = 0;
    jitterUs_ = 0;
    historyIndex_ = 0;
    historyFull_ = false;

    ESP_LOGI(kTag, "Sync reset");
}

SyncStats TimingService::getStats() const {
    uint32_t ageMs = (getLocalTimeUs() - lastSyncUs_) / 1000;

    // Quality based on jitter and age
    uint8_t quality = 100;
    if (jitterUs_ > 500) quality -= std::min(50, static_cast<int>(jitterUs_ / 100));
    if (ageMs > 1000) quality -= std::min(50, static_cast<int>((ageMs - 1000) / 100));
    if (!synced_) quality = 0;

    return {
        .offsetUs = offsetUs_,
        .rttUs = rttUs_,
        .jitterUs = jitterUs_,
        .syncCount = syncCount_,
        .lastSyncAgeMs = ageMs,
        .quality = quality,
    };
}

}  // namespace domes
```

---

## Sync Accuracy Analysis

### Error Sources

| Source | Magnitude | Mitigation |
|--------|-----------|------------|
| ESP-NOW jitter | 100-500 µs | Median filter |
| Processing delay variance | 10-50 µs | Measure T2/T3 precisely |
| Crystal oscillator drift | 20 ppm (~2µs/100ms) | Periodic resync |
| WiFi/BLE coex interference | 0-2000 µs | Reject outliers |
| Task scheduling delay | 0-1000 µs | High priority callback |

### Expected Accuracy

With proper implementation:
- **Initial sync:** ±2ms (rough beacon)
- **After 5 samples:** ±500µs
- **Steady state:** ±200µs (well within ±1ms target)

---

## Integration with Game Engine

### Reaction Time Measurement

```cpp
void GameEngine::onTouch(const TouchEvent& event) {
    // Touch timestamp is in local time
    uint64_t touchLocalUs = event.timestampUs;

    // Convert to network time for master
    uint64_t touchNetworkUs = timing_.localToNetwork(touchLocalUs);

    // Calculate reaction time (armedAtUs is in network time)
    uint32_t reactionTimeUs = touchNetworkUs - armedAtNetworkUs_;

    ESP_LOGI(kTag, "Reaction time: %lu µs", reactionTimeUs);

    // Send to master (already in network time)
    sendTouchEvent(reactionTimeUs, event.strength);
}
```

### Synchronized LED Flash

```cpp
void GameEngine::scheduleFlash(uint64_t networkTimeUs, Color color) {
    // Convert to local time
    uint64_t localTimeUs = timing_.networkToLocal(networkTimeUs);

    // How long until then?
    uint64_t nowUs = timing_.getLocalTimeUs();
    if (localTimeUs <= nowUs) {
        // Already past, flash immediately
        led_.setAll(color);
        led_.refresh();
    } else {
        // Schedule timer
        uint64_t delayUs = localTimeUs - nowUs;
        esp_timer_start_once(flashTimer_, delayUs);
    }
}
```

---

## Debugging Sync Issues

### Serial Commands

```cpp
// Print sync stats
timing_stats

// Force resync
timing_reset

// Measure RTT to specific pod
timing_ping AA:BB:CC:DD:EE:FF
```

### Log Analysis

```
I (1234) timing: Rough sync from beacon: offset=-1523 µs
I (1734) timing: Offset updated: -1412 µs (jitter=234 µs, quality=95)
I (2234) timing: Offset updated: -1398 µs (jitter=189 µs, quality=97)
W (5234) timing: Clock drift detected: 1823 µs
I (5234) timing: Offset updated: -1421 µs (jitter=312 µs, quality=88)
```

---

## Timing Constants

```cpp
namespace timing {
    constexpr uint32_t kSyncIntervalMs = 500;        // Master sync broadcast rate
    constexpr uint32_t kBeaconIntervalMs = 1000;     // Time beacon rate
    constexpr uint32_t kSyncTimeoutMs = 100;         // Max wait for response
    constexpr uint32_t kStaleThresholdMs = 5000;     // Resync if no update
    constexpr int64_t kMaxRttUs = 50000;             // Reject RTT > 50ms
    constexpr int64_t kMaxDriftUs = 2000;            // Trigger correction if > 2ms
    constexpr uint32_t kAccuracyTargetUs = 1000;     // ±1ms target
}
```

---

## Verification

### Test Cases

| Test | Method | Pass Criteria |
|------|--------|---------------|
| Initial sync | Single beacon | Offset within 3ms of actual |
| Converged sync | 10 full syncs | Jitter < 500µs |
| Master change | Kill master, elect new | Resync within 2s |
| High load | BLE scan + game active | Accuracy still < 2ms |
| Long running | 1 hour test | No drift > 5ms |

### Measurement Setup

```cpp
// Both pods flash LED when network time reaches target
// Use oscilloscope to measure delta between LED transitions
// Should be < 2ms for P95

void syncAccuracyTest(uint64_t targetNetworkTime) {
    // All pods schedule flash at same network time
    while (timing_.getSyncedTimeUs() < targetNetworkTime) {
        // Spin wait for precision
    }
    led_.setAll(Color::white());
    led_.refresh();
}
```

---

*Prerequisites: 04-communication.md, 10-master-election.md*
*Related: 05-game-engine.md*
