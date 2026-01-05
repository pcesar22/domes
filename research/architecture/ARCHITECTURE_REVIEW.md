# DOMES Firmware Architecture Review

**Review Date:** 2026-01-05
**Reviewer:** Claude (Expert Architect)

---

## Executive Summary

| Aspect | Rating | Summary |
|--------|--------|---------|
| Interface Design | ⚠️ Good with gaps | Missing critical methods |
| Dependency Management | ✅ Good | Clean layering |
| Communication | ⚠️ Needs work | Master election undefined |
| State Machine | ⚠️ Incomplete | Missing error recovery |
| Memory Budget | ✅ Adequate | Within constraints |
| Testability | ✅ Good | Interfaces enable mocking |
| **Overall** | ⚠️ Ready with caveats | Address gaps before M5 |

---

## 1. Dependency Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            APPLICATION LAYER                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │ Game Engine │  │ Drill Mgr   │  │ OTA Manager │  │ Config Mgr  │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         └────────────────┼────────────────┼────────────────┘                 │
│                          ▼                ▼                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                            SERVICE LAYER                                      │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐  ┌──────────────┐  │
│  │FeedbackService│  │  CommService  │  │ TimingService │  │ NVS Service  │  │
│  └───────┬───────┘  └───────┬───────┘  └───────┬───────┘  └──────┬───────┘  │
├──────────┼──────────────────┼──────────────────┼──────────────────┼──────────┤
│          ▼                  ▼                  ▼                  ▼          │
│                            DRIVER LAYER                                       │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐    │
│  │   LED   │ │  Audio  │ │ Haptic  │ │  Touch  │ │   IMU   │ │  Power  │    │
│  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘    │
├───────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────────┤
│       ▼          ▼          ▼          ▼          ▼          ▼              │
│                           PLATFORM LAYER                                      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐    │
│  │   RMT   │ │   I2S   │ │   I2C   │ │  Touch  │ │   I2C   │ │   ADC   │    │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘    │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         ESP-IDF / FreeRTOS                            │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Shared Resources

| Resource | Components | Arbitration |
|----------|------------|-------------|
| I2C Bus | IMU + Haptic | Mutex |
| WiFi Radio | ESP-NOW + scan | ESP-IDF coex |
| RF Radio | WiFi + BLE | ESP-IDF coex |
| Core 0 | Protocol tasks | FreeRTOS |
| Core 1 | App tasks | FreeRTOS |

**No circular dependencies detected.**

---

## 2. Interface Design Gaps

### Missing Methods by Interface

| Interface | Missing Methods | Rationale |
|-----------|-----------------|-----------|
| ILedDriver | `setRange()`, `setBuffer()`, `deinit()`, `getLed()`, `getBrightness()` | Animation patterns, shutdown, state query |
| IAudioDriver | `queueSound()`, `onComplete()`, `deinit()`, `getVolume()` | Sequential sounds, callbacks, shutdown |
| ITouchDriver | `calibrate()`, `getThreshold()`, `deinit()` | Runtime calibration, state query |
| IImuDriver | `enterLowPowerMode()`, `exitLowPowerMode()`, `configureTapDetection()` | Power management |
| IPowerDriver | `isLowBattery()`, `setWakeupSource()`, `onLowBattery()` | Battery warnings, deep sleep |

### std::optional Concern

**Issue:** `std::optional` may have heap implications in some embedded implementations.

**Recommendation:** Use `tl::expected` or out-parameter pattern instead.

---

## 3. Communication Gaps

### Master Election - UNDEFINED

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      REQUIRED: MASTER ELECTION PROTOCOL                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Scenarios needing handling:                                                │
│   • Initial startup - who becomes master?                                   │
│   • Master pod battery dies                                                 │
│   • Master goes out of range                                                │
│   • Phone reconnects to different pod                                       │
│                                                                              │
│   Recommended approach:                                                      │
│   1. On boot, broadcast ELECTION_START after random delay (100-500ms)       │
│   2. Pods respond with priority (battery × 10 + random)                     │
│   3. Highest priority with majority ack becomes master                      │
│   4. Master broadcasts heartbeat every 1s                                   │
│   5. No heartbeat for 3s → trigger new election                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Clock Sync Issues

| Issue | Current | Recommended |
|-------|---------|-------------|
| Hardcoded delay | 500μs assumed | Measure RTT |
| Low-pass filter | α=0.1 (slow) | α=0.3 initial, 0.1 steady |
| Sync interval | 100ms (overkill) | 500ms sufficient |

**Use NTP-style sync:** Measure T1-T4 timestamps, calculate offset from RTT.

### Race Conditions

| Scenario | Risk | Mitigation |
|----------|------|------------|
| BLE + ESP-NOW concurrent | State machine corruption | Mutex or single event loop |
| Touch during transition | Event in wrong state | Queue with atomic state check |
| OTA + game running | Resource contention | Disallow OTA during drill |
| Election + clock sync | Sync from old master | Ignore sync until election done |

---

## 4. State Machine Validation

### Current States

```
kInitializing → kIdle → kConnecting → kConnected → kArmed → kTriggered → kFeedback
                  ↓
              kStandalone → kError
```

### Missing States

| State | Use Case |
|-------|----------|
| kLowBattery | Battery < 20%, warn user |
| kOtaInProgress | Receiving firmware |
| kCalibrating | Touch/IMU calibration |

### Missing Transitions

| From | To | Scenario |
|------|-----|----------|
| Any | kLowBattery | Battery threshold |
| Any | kError | Critical failure |
| kTriggered | kError | Comm failure (no recovery path!) |
| kTriggered | kConnected | Timeout fallback |

### Missing Timeout Handling

| State | Current | Needed |
|-------|---------|--------|
| kTriggered | None | → kError or kConnected |
| kFeedback | None | → kConnected |
| kOtaInProgress | None | → kError |

---

## 5. Memory Constraints

### Task Stack Budget

| Task | Stack | Priority | Core |
|------|-------|----------|------|
| game | 8KB | MEDIUM | 1 |
| comm | 4KB | HIGH | 0 |
| audio | 4KB | MEDIUM | 1 |
| led | 2KB | LOW | 1 |
| monitor | 2KB | LOW | - |
| **Total** | ~21KB | | |

**Issue:** No stack for OTA task (needs 8KB+)

**Recommendation:** Merge LED into game task, free 2KB for OTA.

### PSRAM Policy - UNDEFINED

| Item | Location | Rationale |
|------|----------|-----------|
| Task stacks | SRAM | FreeRTOS requirement |
| DMA buffers | SRAM | Hardware requirement |
| Audio cache | PSRAM | Large, sequential |
| OTA staging | PSRAM | Large, temporary |

**Rule:** Allocations ≤4KB → SRAM, >4KB → PSRAM

---

## 6. Testability

### Unit Testing Feasibility

| Component | Testable | Mock Needed |
|-----------|----------|-------------|
| LedDriver | ✅ | Mock RMT |
| AudioDriver | ✅ | Mock I2S |
| HapticDriver | ✅ | Mock I2C |
| StateMachine | ✅ | None |
| GameEngine | ✅ | All driver interfaces |
| CommService | ⚠️ Partial | Hard to mock ESP-NOW fully |

### Missing Integration Tests

| Test Case | Status |
|-----------|--------|
| Multi-pod sync accuracy | ❌ |
| Touch event end-to-end | ❌ |
| BLE + ESP-NOW coexistence | ❌ |
| Master failover | ❌ |

---

## 7. Risk Matrix

| # | Risk | Likelihood | Impact | Overall |
|---|------|------------|--------|---------|
| 1 | ESP-NOW latency > 2ms under BLE | Medium | High | **HIGH** |
| 2 | Master election race conditions | High | High | **HIGH** |
| 3 | Clock sync drift > ±1ms | Medium | Medium | MEDIUM |
| 4 | Touch false positives | Medium | Medium | MEDIUM |
| 5 | Heap fragmentation | Low | High | MEDIUM |
| 6 | I2C bus contention | Medium | Low | LOW |

### Unaddressed Failure Modes

| Failure | Needed |
|---------|--------|
| I2C stuck | Timeout + bus reset |
| Touch saturated | Auto-calibration |
| Battery dies mid-drill | Graceful shutdown |
| Pod physically damaged | Health check + exclude |

---

## 8. Gaps Summary

### Under-Specified Components

| Component | Gap |
|-----------|-----|
| Master Election | No algorithm |
| OTA Protocol | No error handling |
| BLE Service | GATT incomplete |
| Drill Types | Only stubs |
| Metrics/Telemetry | Not mentioned |

### Missing Error Handling

| Scenario | Current | Needed |
|----------|---------|--------|
| I2C fails | None | Retry + backoff |
| ESP-NOW fails | Log | Retry + notify |
| Low battery mid-drill | None | Finish + block next |
| Invalid protocol msg | None | Log + discard |

---

## 9. Recommendations

### Immediate (Before M5)

1. ✅ Define master election protocol
2. ✅ Fix clock sync (use RTT measurement)
3. ✅ Add missing interface methods
4. ✅ Add I2C bus mutex
5. ✅ Define PSRAM policy

### Short-Term (M5-M7)

1. ⚠️ Complete state machine transitions
2. ⚠️ Add kLowBattery, kOtaInProgress states
3. ⚠️ Implement heap monitoring
4. ⚠️ Write integration test plan

### Long-Term (M8+)

1. 📋 OTA error handling and rollback
2. 📋 Metrics/telemetry system
3. 📋 Diagnostic mode
4. 📋 Pod provisioning flow

---

## 10. Open Questions

### Architectural Decisions Needed

1. **Master Election:** User-selected or auto-elected? Priority metric?
2. **Pod Identity:** Static (NVS) or dynamic? Pairing flow?
3. **Offline Mode:** What drills available? Discovery method?
4. **Power States:** When light/deep sleep? Idle timeout?
5. **Error Escalation:** When exclude from network? Diagnostic mode?

### Implementation Clarifications

| Question | Options |
|----------|---------|
| Touch fusion | AND or OR for capacitive + IMU? |
| LED animations | Built-in or app-provided? |
| Audio format | RAW only or WAV/MP3? |
| BLE bonding | Supported or open? |
| OTA signing | RSA or ECDSA? |

---

*Review Complete*
*Next Review: After M4 milestone*
