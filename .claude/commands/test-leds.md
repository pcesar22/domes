---
description: Cycle through all LED patterns on one or both pods
argument-hint: [port(s)]
allowed-tools: Bash, Read, Glob
---

# LED Pattern Test

Cycle through all LED patterns to verify the SK6812 LED ring on NFF devboards.

## Arguments

- `$1` - Port(s) (optional, default: auto-detect)
  - Single: `/dev/ttyACM0`
  - Multiple: `/dev/ttyACM0,/dev/ttyACM1` or `all`

## Instructions

1. **Determine ports**: auto-detect if not specified.

2. **Set CLI path**:
   ```bash
   CLI="tools/domes-cli/target/debug/domes-cli"
   ```

3. **Run LED test sequence on each port**:

   ```bash
   for PORT in $PORTS; do
     echo "=== LED Test: $PORT ==="

     # Solid colors (R, G, B, W)
     for color in ff0000 00ff00 0000ff ffffff; do
       echo "  Solid $color"
       $CLI --port $PORT led solid --color $color
       sleep 1
     done

     # Breathing pattern
     echo "  Breathing (purple)"
     $CLI --port $PORT led breathing --color ff00ff
     sleep 3

     # Color cycle
     echo "  Color cycle"
     $CLI --port $PORT led cycle
     sleep 4

     # Off
     echo "  Off"
     $CLI --port $PORT led off
   done
   ```

4. **Get current LED state**:
   ```bash
   $CLI --port $PORT led get
   ```

5. **Ask user**: "Did all 16 LEDs light up correctly for each color? Any dead LEDs or wrong colors?"

6. **Report**:
   ```
   LED TEST: $PORT
   ===============
   Solid red:     OK (command accepted)
   Solid green:   OK
   Solid blue:    OK
   Solid white:   OK
   Breathing:     OK
   Color cycle:   OK

   Visual check: [user must confirm]
   - All 16 LEDs illuminated: YES/NO
   - Colors correct: YES/NO
   - Dead/dim LEDs: NONE / positions [...]
   ```

## Available LED Commands

| Command | Description |
|---------|-------------|
| `led solid --color RRGGBB` | Solid color |
| `led breathing --color RRGGBB` | Breathing/pulsing effect |
| `led cycle` | Rainbow color cycle |
| `led off` | Turn off all LEDs |
| `led get` | Get current LED state |

## Notes

- LED data pin: GPIO16 (NFF devboard)
- 16x SK6812MINI-E RGBW LEDs in a ring
- Level shifter: SN74AHCT1G125 (3.3V → 5V logic)
- Default brightness: 128/255
