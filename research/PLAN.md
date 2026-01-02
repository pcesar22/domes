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

### 1.3 Feedback Features Comparison

| Product | Light | Sound | Haptic | On-Pod Score | Phone-Free? |
|---------|-------|-------|--------|--------------|-------------|
| **BlazePod** | ✅ 8 colors | ❌ None | ❌ None | ❌ None | ❌ App-dependent |
| **A-Champs ROX** | ✅ RGB | ✅ Speaker | ✅ Vibration | ❌ None | ⚠️ Partial |
| **ROXProX** | ✅ + Symbols | ✅ Speaker | ✅ Vibration | ⚠️ Symbols only | ⚠️ Partial |
| **FitLight** | ✅ 6 colors | ⚠️ Optional beep | ❌ None | ❌ None | ❌ Tablet required |
| **RXTR** | ✅ Basic | ✅ Basic | ❌ None | ✅ LED display | ✅ Yes |

**Key Insight**: A-Champs uses sound/vibration as *triggers* (perceive→process→react), but NOT as *satisfying performance feedback*. No competitor delivers arcade-quality "game feel."

### 1.4 Target Customer Segments

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
- ❌ **No Audio Feedback**: Light-only, silent pods

### 2.2 FitLight Issues
- ❌ **Extreme Cost**: $1,000-3,000+
- ❌ **Tablet Required**: No smartphone app
- ❌ **Short Battery**: Only 4 hours
- ❌ **Compatibility**: 2024 systems incompatible with older versions

### 2.3 A-Champs Issues
- ❌ Uses sound/vibration as triggers, not satisfying feedback
- ❌ No real-time performance indication on pods
- ❌ Still requires app for scoring/analytics

### 2.4 General Market Gaps
- ❌ No truly open-source option exists
- ❌ No modular/repairable designs
- ❌ Limited offline functionality across all products
- ❌ No affordable "gateway" product for home users
- ❌ Poor multi-room/large venue support
- ❌ **No product delivers satisfying "game feel" feedback**

---

## 3. CORE DIFFERENTIATOR: PHONE-FREE EXPERIENCE

### 3.1 The Problem
Every competitor requires constant phone/tablet interaction to understand performance. Athletes must interrupt training flow to check scores and stats.

### 3.2 Our Solution
**The pods themselves communicate everything you need to know.**

You should *feel* how you're doing through:
- Sound that responds to your speed
- Haptic feedback that rewards fast reactions
- LED patterns that show streaks and performance
- End-of-drill celebrations or feedback

### 3.3 On-Pod Feedback Options (To Be Refined)

#### During Drill
- Audio pitch/tone indicates reaction speed (higher = faster)
- Combo/streak sounds for consecutive fast hits
- Haptic vibration intensity reflects performance
- LED color shift: green (fast) → yellow (ok) → red (slow)
- LED "breathing" or pulse patterns for streaks

#### End of Drill
- Victory jingle vs disappointment tone
- Master pod announces score via speaker
- LED sequence shows grade (pattern-based A/B/C)
- Rainbow celebration animation for personal bests

#### Intensity & Challenge Feel
- Urgent countdown sounds before each target
- Escalating tempo/intensity as drill progresses
- Satisfying impact sound on every touch
- Competitive audio cues when falling behind personal best

### 3.4 Sound Design Direction

**NOT going for**: Arcade/8-bit, DJ announcer style

**Exploring**:
- **Modern Gaming**: Polished UI sounds, subtle but satisfying
- **Minimal/Premium**: Elegant tones, not overwhelming
- **Sci-fi**: Futuristic, synth-based feedback

*Final direction TBD in product design phase.*

---

## 4. PROPOSED FEATURE SET

### 4.1 Core Philosophy
- **Open Source**: Hardware designs and software fully open
- **No Subscription**: All features unlocked forever, free app
- **Repairable**: User-serviceable, standard components

### 4.2 Business Model
- Revenue from hardware sales
- Free app with all features
- Future monetization options (TBD later):
  - Premium drill packs
  - Custom sound themes
  - Special LED color profiles

### 4.3 Feature Tiers

#### Tier 1: Must-Have (Match Competition)
- [ ] Touch-activated LED pods with 8+ colors
- [ ] Smartphone app (iOS/Android) - FREE
- [ ] Rechargeable batteries (8+ hour life)
- [ ] Water/impact resistant (IP65+)
- [ ] 50+ pre-built drills
- [ ] Reaction time tracking & analytics

#### Tier 2: Competitive Edge (Beat BlazePod)
- [ ] **No Subscription** - All features unlocked forever
- [ ] **User-Replaceable Battery** - Standard cells, user-serviceable
- [ ] **Physical On/Off Switch** - Prevent battery drain
- [ ] **Rich Audio Feedback** - Speaker in each pod
- [ ] **Haptic Feedback** - Vibration motor for tactile response
- [ ] **On-Pod Performance Indication** - Know how you're doing without phone
- [ ] **Extended Range** - Pods relay signals for 100m+ range
- [ ] **24+ Pod Support** - Match A-Champs capability
- [ ] **Offline Mode** - Full functionality without phone after setup
- [ ] **Open Source** - Full hardware/software transparency
- [ ] **Open API** - Developer integrations welcome
- [ ] **Affordable Accessories** - Standard hardware, 3D-printable mounts

#### Tier 3: Innovation (Leapfrog Competition)
- [ ] **Multi-Modal Sensing**:
  - Touch (capacitive, pressure-sensitive)
  - Proximity (adjustable distance sensing)
  - Sound activation (clap/voice trigger)
  - Motion/impact (accelerometer)

- [ ] **Modular Design**:
  - Base pod with swappable sensor modules
  - Upgrade path (buy basic, add features later)
  - User-serviceable with standard tools

- [ ] **Smart Training Features**:
  - Adaptive difficulty based on performance
  - Fatigue detection (slowing reaction times)
  - Personalized training recommendations
  - Progress gamification with achievements

- [ ] **Multi-Player/Team Modes**:
  - Competitive head-to-head
  - Team relay races
  - Synchronized group training
  - Leaderboards (local & cloud)

- [ ] **Integration Ecosystem**:
  - Heart rate monitor pairing
  - Fitness app export (Strava, Apple Health, Google Fit)
  - Video recording sync for coaching review
  - Smart TV/Projector display mode

- [ ] **Accessibility Features**:
  - Audio cues for visually impaired
  - High-contrast color modes
  - Adjustable timing for different ability levels
  - Therapeutic presets for PT use

---

## 5. OPEN QUESTIONS

### Feature Prioritization
1. Which Tier 3 features are most compelling for v1?
2. Should we focus on one vertical first (e.g., PT clinics vs athletes)?
3. How important is the modular/upgrade path vs simpler single-SKU?

### Differentiation Strategy
1. How much audio/haptic capability per pod? (cost vs experience tradeoff)
2. What's the minimum viable "phone-free" experience for v1?

### Use Cases to Explore
1. Are there underserved niches (elderly cognitive, special needs, gaming)?
2. Should we partner with specific sports/PT organizations early?
3. B2B (gyms, clinics) vs B2C (home users) focus?

---

## 6. COMPETITIVE MOAT STRATEGY

### Short-term Differentiation
1. **Phone-Free Experience**: Feel your performance through the pods
2. **Price**: Undercut BlazePod by 50-60%
3. **No Subscription**: Major selling point
4. **Open Source**: Build community, earn trust
5. **Repairability**: Stand out from disposable competitors

### Long-term Moat
1. **Ecosystem**: Community-created drills, sound packs, accessories
2. **API/SDK**: Third-party integrations
3. **Data Network Effects**: Better AI with more users
4. **Brand Loyalty**: Right-to-repair advocates, coaches

---

## APPENDIX: RESEARCH SOURCES

### Product Information
- [BlazePod Official](https://www.blazepod.com/)
- [FitLight Training](https://www.fitlighttraining.com/)
- [A-Champs ROXPro](https://a-champs.com/)
- [A-Champs vs BlazePod](https://a-champs.com/pages/a-champs-vs-blazepod)

### Market Analysis
- [Sports Technology Market - Fortune Business Insights](https://www.fortunebusinessinsights.com/sports-technology-market-112896)
- [Sports Technology Market - Straits Research](https://straitsresearch.com/report/sports-technology-market)

### User Reviews & Pain Points
- [BlazePod TrustPilot Reviews](https://www.trustpilot.com/review/blazepod.com)
- [BlazePod vs FitLight Comparison](https://gadgetsandwearables.com/2020/12/06/blazepod-vs-fitlight/)

### Game Audio & Feedback Design
- [Impact of Sound in Arcade Gaming](https://www.homegamesroom.co.uk/blogs/types-of-arcade-machines-and-their-features/the-impact-of-sound-and-music-in-arcade-gaming)
- [Game Sound Design Principles](https://gamedesignskills.com/game-design/sound/)
- [Sound Design Tips for Games](https://www.gameanalytics.com/blog/9-sound-design-tips-to-improve-your-games-audio/)

### Rehabilitation Research
- [Reaction Training in Physical Therapy](https://motusspt.com/reaction-training-lights-what-you-need-to-know/)
- [Cognitive Training for Elderly](https://pmc.ncbi.nlm.nih.gov/articles/PMC3881481/)

---

*Document Created: 2026-01-02*
*Last Updated: 2026-01-02*
*Project Codename: DOMES*
