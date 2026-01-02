# DOMES: Distributed Open-source Motion & Exercise System
## Project Planning Document - BlazePod Alternative

---

## 1. MARKET RESEARCH SUMMARY

### 1.1 Market Size & Opportunity
- **Global Sports Technology Market**: $21.6B (2024) → $86B+ by 2033 (CAGR ~16%)
- **Physical Education Technology**: Growing segment with BlazePod as key player
- **BlazePod Stats**: 1M+ pods sold, 120+ countries, 100M+ activities completed

### 1.2 Current Market Leaders

| Product | Price Range | Pods | Key Differentiator | Weaknesses |
|---------|-------------|------|-------------------|------------|
| **BlazePod** | $459-$1,159 + $15/mo subscription | 4-6 | Market leader, 500+ drills, great app | Buggy app, charging pin failures, expensive accessories |
| **FitLight** | $1,000-$3,000+ | 4-24 | Proximity sensing, 75m range, pro-grade | Very expensive, requires tablet, shorter battery |
| **A-Champs ROXPro** | ~$800-1,200 | 6-24 | Multi-sensory (light+sound+vibration), 12 sensitivity levels | Less market presence, heavier pods |
| **ROXProX** | Premium tier | 6+ | Laser proximity 0.1-1.5m, touchless | High cost |
| **Reaction X** | Budget tier | 4+ | Most affordable, 4 sensor modes | Less refined app |

### 1.3 Target Customer Segments

1. **Athletic Training** (40% of market)
   - Professional sports teams
   - College/university athletic programs
   - Personal trainers

2. **Physical Therapy / Rehabilitation** (25%)
   - PT clinics
   - Hospitals
   - Cognitive rehabilitation centers

3. **Fitness Enthusiasts** (20%)
   - Home gym users
   - CrossFit boxes
   - Boxing/MMA gyms

4. **Education** (10%)
   - Schools (PE departments)
   - Youth sports programs
   - Special needs education

5. **Elderly/Cognitive Health** (5% - GROWING)
   - Senior living facilities
   - Fall prevention programs
   - Cognitive decline prevention

---

## 2. COMPETITOR PAIN POINTS (Our Opportunities)

### 2.1 BlazePod Issues (from real user reviews)
- ❌ **App Reliability**: Crashes, disconnections, pods stop responding
- ❌ **Charging Failures**: Pins break, batteries not user-replaceable
- ❌ **Expensive Subscription**: $15/mo ($180/yr) for full features
- ❌ **Fragile Accessories**: Mounting clips break easily, expensive to replace
- ❌ **Pod Stability**: Useless without $60+ bases
- ❌ **Always-On**: Can't turn off pods, batteries drain if unchecked
- ❌ **Limited Connectivity**: Only 12 pods max, 40m range
- ❌ **Touch-Only**: Single activation mode

### 2.2 FitLight Issues
- ❌ **Extreme Cost**: $1,000-3,000+
- ❌ **Tablet Required**: No smartphone app
- ❌ **Short Battery**: Only 4 hours
- ❌ **Compatibility**: 2024 systems incompatible with older versions

### 2.3 General Market Gaps
- ❌ No truly open-source option exists
- ❌ No modular/repairable designs
- ❌ Limited offline functionality across all products
- ❌ No affordable "gateway" product for home users
- ❌ Poor multi-room/large venue support

---

## 3. PROPOSED COMPETITIVE ADVANTAGES

### 3.1 Core Philosophy: "Open, Affordable, Repairable"

```
DOMES POSITIONING TRIANGLE:

         AFFORDABLE
            /\
           /  \
          /    \
         /  ✓   \
        /________\
   OPEN          REPAIRABLE
  SOURCE         /MODULAR
```

### 3.2 Killer Features to Differentiate

#### Tier 1: Must-Have (Match Competition)
- [x] Touch-activated LED pods with 8+ colors
- [x] Smartphone app (iOS/Android) via BLE
- [x] Rechargeable batteries (8+ hour life)
- [x] Water/impact resistant (IP65+)
- [x] 50+ pre-built drills
- [x] Reaction time tracking & analytics

#### Tier 2: Competitive Edge (Beat BlazePod)
- [x] **No Subscription Required** - All features unlocked forever
- [x] **User-Replaceable Battery** - Standard 18650 or USB-C rechargeable
- [x] **Physical On/Off Switch** - Prevent battery drain
- [x] **Mesh Networking** - Pods relay signals for 100m+ range
- [x] **24+ Pod Support** - Match A-Champs capability
- [x] **Offline Mode** - Full functionality without phone after setup
- [x] **Open API** - Let developers create custom integrations
- [x] **Affordable Accessories** - 3D-printable mounts, standard hardware

#### Tier 3: Innovation (Leapfrog Competition)
- [x] **Multi-Modal Sensing**:
  - Touch (capacitive, pressure-sensitive)
  - Proximity (IR sensor, 5-50cm adjustable)
  - Sound activation (clap/voice trigger)
  - Motion/impact (accelerometer)
  - Optional vibration feedback

- [x] **Modular Design**:
  - Base pod with swappable sensor modules
  - Upgrade path (buy basic, add features later)
  - User-serviceable with standard tools

- [x] **Smart Training AI**:
  - Adaptive difficulty based on performance
  - Fatigue detection (slowing reaction times)
  - Personalized training recommendations
  - Progress gamification with achievements

- [x] **Multi-Player/Team Modes**:
  - Competitive head-to-head
  - Team relay races
  - Synchronized group training
  - Leaderboards (local & cloud)

- [x] **Integration Ecosystem**:
  - Heart rate monitor pairing
  - Fitness app export (Strava, Apple Health, Google Fit)
  - Video recording sync for coaching review
  - Smart TV/Projector display mode

- [x] **Accessibility Features**:
  - Audio cues for visually impaired
  - High-contrast color modes
  - Adjustable timing for different ability levels
  - Therapeutic presets for PT use

---

## 4. PROPOSED PRODUCT LINEUP

### 4.1 Product Tiers

```
┌─────────────────────────────────────────────────────────────────┐
│                     DOMES PRODUCT LINEUP                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  HOME KIT (4 pods)          $149-199                           │
│  ├─ Touch sensing                                               │
│  ├─ 8 LED colors                                                │
│  ├─ BLE connectivity                                            │
│  ├─ 8-hour battery                                              │
│  └─ Basic app (no subscription)                                 │
│                                                                 │
│  TRAINER KIT (6 pods)       $249-299                           │
│  ├─ Everything in Home +                                        │
│  ├─ Proximity sensing                                           │
│  ├─ Mesh networking                                             │
│  ├─ Offline mode                                                │
│  └─ Pro app features                                            │
│                                                                 │
│  PRO KIT (8 pods)           $349-449                           │
│  ├─ Everything in Trainer +                                     │
│  ├─ Multi-modal sensors                                         │
│  ├─ Vibration feedback                                          │
│  ├─ Sound activation                                            │
│  └─ API access                                                  │
│                                                                 │
│  EXPANSION PACKS            $59-79 per 2 pods                  │
│                                                                 │
│  ACCESSORIES                                                    │
│  ├─ Ground stake bases      $15/4-pack                         │
│  ├─ Cone adapters           $12/4-pack                         │
│  ├─ Wall mounts             $10/4-pack                         │
│  ├─ Suction cups            $8/4-pack                          │
│  ├─ Carrying case           $25                                │
│  └─ Replacement batteries   $5 each                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 Price Comparison

| Configuration | BlazePod | DOMES | Savings |
|---------------|----------|-------|---------|
| 4 pods + basics | $459 | $149-199 | **60-67%** |
| 6 pods + Pro | $1,159 + $180/yr | $249-299 (no sub) | **75%+ over 2 years** |
| 8 pods + Full accessories | $1,500+ | $399-499 | **65-70%** |

---

## 5. TECHNOLOGY ARCHITECTURE

### 5.1 Hardware Platform Options

#### Option A: ESP32-S3 (Recommended)
```
Pros:
+ Dual-core 240MHz processor
+ Native USB support
+ BLE 5.0 + WiFi
+ Low power modes
+ $3-5 per chip
+ Excellent Arduino/PlatformIO support
+ Large community

Cons:
- More complex than simpler MCUs
- Higher power than nRF series
```

#### Option B: nRF52840
```
Pros:
+ Best-in-class BLE
+ Ultra low power
+ Native USB
+ ARM Cortex-M4

Cons:
- No WiFi
- Smaller community
- Nordic SDK learning curve
```

#### Recommendation: **ESP32-S3** for v1.0
- Better developer ecosystem
- WiFi enables OTA updates & mesh networking
- Cost-effective at scale
- Easier to prototype rapidly

### 5.2 Hardware BOM Estimate (per pod)

| Component | Est. Cost | Notes |
|-----------|-----------|-------|
| ESP32-S3-WROOM-1 | $3.50 | Main MCU |
| WS2812B LEDs (ring of 12) | $1.50 | RGB addressable |
| 18650 Li-ion battery | $2.50 | 3000mAh, replaceable |
| Battery holder + protection | $0.80 | BMS circuit |
| Touch sensor (TTP223) | $0.30 | Capacitive |
| IR proximity sensor | $0.50 | Optional module |
| Piezo buzzer | $0.25 | Audio feedback |
| Vibration motor | $0.40 | Haptic feedback |
| USB-C connector | $0.30 | Charging |
| PCB | $1.50 | Custom 4-layer |
| Housing (injection molded) | $2.00 | At 10k+ qty |
| Misc (buttons, LEDs, caps) | $0.50 | |
| **Total per pod** | **~$14-16** | At volume |

#### Margin Analysis
- Manufacturing cost: ~$15/pod
- Target retail (4-pack): $149-199
- Per-pod revenue: $37-50
- **Gross margin: 60-70%**

### 5.3 Software Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       SOFTWARE STACK                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  MOBILE APP (React Native / Flutter)                           │
│  ├─ Drill library & editor                                      │
│  ├─ Real-time pod control                                       │
│  ├─ Analytics dashboard                                         │
│  ├─ User profiles & progress                                    │
│  ├─ Social features (leaderboards)                              │
│  └─ Offline mode with sync                                      │
│                                                                 │
│  FIRMWARE (ESP-IDF / Arduino)                                   │
│  ├─ BLE GATT services                                           │
│  ├─ LED control (FastLED)                                       │
│  ├─ Sensor fusion                                               │
│  ├─ Power management                                            │
│  ├─ OTA updates                                                 │
│  ├─ Mesh networking (ESP-NOW)                                   │
│  └─ Standalone drill execution                                  │
│                                                                 │
│  CLOUD (Optional, Firebase/Supabase)                            │
│  ├─ User accounts                                               │
│  ├─ Drill sharing community                                     │
│  ├─ Cloud sync                                                  │
│  └─ Analytics aggregation                                       │
│                                                                 │
│  OPEN API                                                       │
│  ├─ REST/WebSocket endpoints                                    │
│  ├─ BLE direct control spec                                     │
│  └─ SDK for custom apps                                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.4 Communication Protocol

```
Pod-to-App Communication (BLE):
┌─────────┐         ┌─────────┐
│   App   │◄───────►│  Pod 1  │
│(Central)│   BLE   │(Periph) │
└─────────┘         └────┬────┘
                         │ ESP-NOW
                    ┌────▼────┐
                    │  Pod 2  │
                    │ (Mesh)  │
                    └────┬────┘
                         │
                    ┌────▼────┐
                    │  Pod 3  │
                    └─────────┘

Benefits:
- App connects to 1 "master" pod
- Master relays to mesh network
- Reduces BLE connection overhead
- Extends range significantly
```

---

## 6. DEVELOPMENT ROADMAP

### Phase 1: Proof of Concept (4-8 weeks)
- [ ] ESP32-S3 dev board prototypes
- [ ] Basic BLE communication
- [ ] LED ring control
- [ ] Touch detection
- [ ] Simple mobile app (React Native)
- [ ] 3 basic drills working

### Phase 2: Alpha Hardware (6-10 weeks)
- [ ] Custom PCB design
- [ ] 3D printed enclosure prototypes
- [ ] Battery management integration
- [ ] Proximity sensor integration
- [ ] Multi-pod synchronization
- [ ] Expanded drill library (20+)

### Phase 3: Beta Product (8-12 weeks)
- [ ] Injection molded housing design
- [ ] Water resistance testing (IP65)
- [ ] Drop/impact testing
- [ ] App feature complete
- [ ] Analytics and tracking
- [ ] User testing (10-20 testers)

### Phase 4: Production (8-12 weeks)
- [ ] Manufacturing partner selection
- [ ] FCC/CE certification
- [ ] Final testing & QA
- [ ] Packaging design
- [ ] Launch marketing
- [ ] Initial production run (1,000 units)

---

## 7. RISKS & MITIGATIONS

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| BLE range/reliability | High | Medium | Mesh networking, extensive testing |
| Battery life < 8hrs | Medium | Low | Aggressive power management, larger cell option |
| App store rejection | High | Low | Follow guidelines, prepare for review |
| Patent infringement | High | Medium | Freedom-to-operate analysis, unique features |
| Manufacturing quality | High | Medium | Rigorous QA, multiple supplier quotes |
| Price competition | Medium | High | Focus on value (no subscription, repairability) |

---

## 8. COMPETITIVE MOAT STRATEGY

### Short-term (0-12 months)
1. **Price**: Undercut BlazePod by 50-60%
2. **No Subscription**: Major selling point
3. **Open Source**: Build community, earn trust
4. **Repairability**: Stand out from disposable competitors

### Long-term (1-3 years)
1. **Ecosystem**: Community-created drills, accessories
2. **API/SDK**: Third-party integrations
3. **Data Network Effects**: Better AI with more users
4. **Brand Loyalty**: Right-to-repair advocates, coaches

---

## 9. OPEN QUESTIONS FOR DECISION

### Hardware Decisions
1. **Battery**: Replaceable 18650 vs integrated LiPo with USB-C charging only?
2. **LED Count**: 12-LED ring vs 24-LED ring vs single dome LED?
3. **Sensors**: Which to include in base model vs upgrades?
4. **Form Factor**: Flat puck vs dome vs cone-shaped?

### Software Decisions
1. **App Framework**: React Native vs Flutter vs native?
2. **Firmware**: Arduino/PlatformIO vs ESP-IDF native?
3. **Cloud**: Self-hosted vs Firebase vs Supabase vs none?
4. **Licensing**: MIT vs GPL vs proprietary core + open API?

### Business Decisions
1. **Launch Platform**: Kickstarter vs direct-to-consumer?
2. **Target Market First**: Home fitness vs Pro trainers vs PT clinics?
3. **Geographic Focus**: US first vs global?
4. **Manufacturing**: China vs domestic vs hybrid?

---

## 10. NEXT STEPS

1. **Validate decisions above** with user/founder input
2. **Order dev hardware** (ESP32-S3 dev boards, LEDs, sensors)
3. **Set up development environment** (firmware + app)
4. **Build first working prototype** (1 pod, basic features)
5. **Create project repository structure**

---

## APPENDIX A: RESEARCH SOURCES

### Product Information
- [BlazePod Official](https://www.blazepod.com/)
- [FitLight Training](https://www.fitlighttraining.com/)
- [A-Champs ROXPro](https://a-champs.com/)

### Market Analysis
- [Sports Technology Market - Fortune Business Insights](https://www.fortunebusinessinsights.com/sports-technology-market-112896)
- [Sports Technology Market - Straits Research](https://straitsresearch.com/report/sports-technology-market)

### User Reviews & Pain Points
- [BlazePod TrustPilot Reviews](https://www.trustpilot.com/review/blazepod.com)
- [BlazePod vs FitLight Comparison](https://gadgetsandwearables.com/2020/12/06/blazepod-vs-fitlight/)
- [A-Champs vs BlazePod](https://a-champs.com/pages/a-champs-vs-blazepod)

### Technical Resources
- [ESP32-BLE-Arduino GitHub](https://github.com/nkolban/ESP32_BLE_Arduino)
- [FastLED BLE Control](https://github.com/jasoncoon/esp32-fastled-ble)

### Rehabilitation Research
- [Reaction Training in Physical Therapy](https://motusspt.com/reaction-training-lights-what-you-need-to-know/)
- [Cognitive Training for Elderly](https://pmc.ncbi.nlm.nih.gov/articles/PMC3881481/)

---

*Document Created: 2026-01-02*
*Project Codename: DOMES*
