# DOMES Pod - Industrial Design Requirements
## Form Factor Specification for ID Engineer

---

## 1. OVERVIEW

Design a hexagonal puck-shaped reaction training pod with the following priorities:
1. **Ground adherence** - Must not move/slide on light touch activation
2. **Stackable** - 6-8 pods stack magnetically for storage and charging
3. **Minimal/premium aesthetic** - Clean, modern, high-quality feel

---

## 2. FORM FACTOR

### 2.1 Shape: Hexagonal Puck

```
        TOP VIEW                      SIDE VIEW

         _____                    ┌─────────────┐
       ╱       ╲                  │░░░░░░░░░░░░░│ ← Diffused top + upper sides
      ╱         ╲                 │░░░░░░░░░░░░░│   (translucent, glows)
     │           │                ├─────────────┤
     │           │                │             │ ← Solid base
      ╲         ╱                 │             │   (opaque, houses electronics)
       ╲_______╱                  └─────────────┘
                                        ▲
                                   Rubber grip ring
```

### 2.2 Dimensions

| Parameter | Target | Range | Notes |
|-----------|--------|-------|-------|
| Width (corner to corner) | 11 cm | 10-12 cm | Balance of stability and portability |
| Width (flat edge to edge) | ~9.5 cm | 9-10 cm | Derived from hex geometry |
| Total height | 4.5 cm | 4-5 cm | Low profile for stability |
| Diffused zone height | ~2 cm | 1.5-2.5 cm | Upper portion of puck |
| Base zone height | ~2.5 cm | 2-3 cm | Houses battery, PCB, speaker |
| Weight | 175 g | 150-200 g | Heavy enough for stability |

### 2.3 Aspect Ratio
- **Wide and low** - Prioritize ground contact area over height
- Target ratio: Width ~2.5x Height

---

## 3. GROUND ADHERENCE

### 3.1 Primary Use Surface
- Indoor flat surfaces: gym floors (wood, rubber, vinyl), tables, concrete
- NOT optimized for: grass, turf, uneven outdoor surfaces

### 3.2 Stability Requirements
- Pod must NOT slide or tip when struck with typical touch force
- Touch activation force: ~1-3 N (light tap to firm press)
- Must remain stable with off-center touches near edges

### 3.3 Design Features for Stability

| Feature | Specification |
|---------|---------------|
| **Base grip ring** | Silicone or rubber ring/feet on bottom perimeter |
| **Low center of gravity** | Battery and weighted components in bottom 1/3 |
| **Wide footprint** | Hex shape maximizes base area |
| **Weight distribution** | Concentrated low, not top-heavy |

### 3.4 Grip Material
- Material: Silicone or TPE (thermoplastic elastomer)
- Durometer: 40-60 Shore A (soft enough to grip, durable)
- Pattern: Continuous ring or 6 corner pads (TBD by ID)
- Must not mark/scuff flooring

---

## 4. STACKING & CHARGING SYSTEM

### 4.1 Stack Configuration
- **Ideal stack**: 6 pods
- **Maximum stack**: 8 pods
- All pods are identical (no special "bottom" pod)
- Separate charging base receives the stack

### 4.2 Magnetic Alignment

```
    ┌───────────┐
    │   Pod 6   │
    ├───────────┤ ← Magnets + pogo pins
    │   Pod 5   │
    ├───────────┤
    │    ...    │
    ├───────────┤
    │   Pod 1   │
    └─────┬─────┘
          │
    ┌─────┴─────┐
    │   BASE    │ ← USB-C input
    └───────────┘
```

### 4.3 Magnet Requirements
- Self-aligning: Pods snap into correct orientation
- Strong enough: Stack of 8 is stable, won't topple easily
- Easy to separate: Single-hand pull-apart force ~3-5 N
- Magnet type: Neodymium preferred (N35 or similar)
- Placement: 3 or 6 points around perimeter (aligned with hex geometry)

### 4.4 Charging Contacts
- Type: Pogo pins (spring-loaded)
- Location: Center of top and bottom faces
- Configuration: Minimum 2 pins (power + ground), recommend 3+ for data/balancing
- Must make reliable contact when magnetically aligned
- Recessed to protect pins, but accessible

### 4.5 Charging Base
- Accepts bottom of pod stack
- USB-C power input
- LED indicator for charging status
- Same hex footprint as pods (aesthetic consistency)
- Optional: Slightly weighted for stack stability

---

## 5. SHELL & MATERIALS

### 5.1 Two-Zone Construction

| Zone | Location | Material | Finish |
|------|----------|----------|--------|
| **Diffused top** | Upper ~45% | Translucent PC or PMMA | Frosted/matte diffusion |
| **Solid base** | Lower ~55% | Opaque ABS or PC | Matte or soft-touch |

### 5.2 Diffused Zone Requirements
- Must evenly diffuse LED light (no hot spots)
- Visible from standing angle (top + upper sides glow)
- Scratch-resistant surface
- Color: White or light gray translucent (TBD)

### 5.3 Base Zone Requirements
- Houses: Battery, PCB, speaker, vibration motor
- Opaque to hide internals
- Color: Dark gray, charcoal, or black (TBD)
- Soft-touch coating optional for premium feel

### 5.4 Durability
- Drop test: Survive 1m drop onto hard floor
- Impact resistant: Withstand repeated hand strikes
- IP rating: IP54 minimum (dust protected, splash resistant)

---

## 6. TOUCH SURFACE

### 6.1 Active Touch Zone
- **Full top surface** must be touch-sensitive
- No dead zones - edge-to-edge activation
- Users will strike center, edges, and corners

### 6.2 Touch Reliability
- Must register 100% of intentional touches
- Reject false triggers (vibration, nearby touches)
- Work with bare hands, gloved hands, and light slaps
- Response time: <10ms from touch to LED/sound feedback

### 6.3 Surface Feel
- Slight texture or matte finish (not slippery)
- Comfortable for repeated palm strikes
- No sharp edges

---

## 7. INTERNAL COMPONENT ACCOMMODATION

The ID must accommodate these internal components (sizes approximate):

| Component | Approximate Size | Placement |
|-----------|------------------|-----------|
| PCB (main) | 60mm diameter circle | Center, base zone |
| Battery (18650 or LiPo) | 65x18mm cylinder or 50x35x8mm pouch | Base zone |
| Speaker | 20-28mm diameter | Base zone, downward-firing or side port |
| Vibration motor | 10mm coin or 6x12mm cylinder | Base zone |
| LED ring/array | Perimeter of diffused zone | Under diffuser |
| Touch sensor | Full top coverage | Under touch surface |
| Pogo pins (top) | 3-pin cluster, center | Top face, recessed |
| Pogo pins (bottom) | 3-pin cluster, center | Bottom face, recessed |
| Magnets (x6) | 6mm diameter x 3mm | Perimeter, top and bottom |
| On/off switch | Small slide or button | Side of base zone |
| USB-C port | Standard size | Side of base zone (individual charging fallback) |

### 7.1 Audio Port
- Speaker needs acoustic port for sound output
- Options: Downward-firing (protected), side-firing, or grill on base
- Must not compromise water resistance

---

## 8. USER INTERACTIONS

### 8.1 Physical Controls
- **On/Off switch**: Physical slide or button, accessible but not accidental
- **USB-C port**: For individual pod charging if needed (fallback to stack charging)

### 8.2 Visual Feedback
- LED diffusion shows: Colors, patterns, brightness changes
- Must be visible in bright gym lighting
- 8+ distinct colors required

### 8.3 Status Indicators
- Charging: LED behavior (pulsing, color)
- Battery low: LED behavior
- Pairing/connection: LED behavior
- Consider small dedicated status LED if needed

---

## 9. AESTHETIC DIRECTION

### 9.1 Style: Minimal / Premium
- Clean lines, no unnecessary details
- High-quality materials and finish
- Scandinavian or Apple-esque design sensibility
- Should look at home in a modern gym or living room

### 9.2 Color Palette (Initial)
- Diffused zone: White or light warm gray
- Base zone: Charcoal, slate gray, or matte black
- Accent: None or subtle (brand color TBD)

### 9.3 Branding
- Logo placement: Subtle, on base zone side or bottom
- No large branding on top surface

---

## 10. DELIVERABLES REQUESTED

1. **Concept sketches**: 3-5 variations exploring hex puck form
2. **3D CAD model**: Refined concept with accurate dimensions
3. **Exploded view**: Showing shell components and internal layout
4. **Stacking visualization**: 6-pod stack on charging base
5. **CMF recommendations**: Color, material, finish options
6. **Prototype-ready files**: For 3D printing initial prototypes

---

## 11. OPEN ITEMS FOR ID INPUT

- Exact hex corner radius (sharp vs. rounded)
- Grip ring pattern (continuous vs. segmented)
- Parting line location between diffused and solid zones
- Speaker port design and location
- Magnet and pogo pin exact positioning
- Charging base form factor

---

## REFERENCE IMAGES

*To be added: Mood board with minimal/premium product examples*

---

*Document Created: 2026-01-02*
*Project: DOMES*
*Status: Ready for ID concept phase*
