# NFF Development Board Bring-Up Checklist

Use this checklist when you receive boards from the fab house. Complete each step in order.

## Before Powering On

- [ ] **Visual inspection**
  - No solder bridges visible
  - All components placed correctly (check orientation of U1, U3, U4, U6)
  - No missing components
  - No burnt or damaged components

- [ ] **Check for shorts** (multimeter continuity mode)
  - [ ] 3.3V to GND: Should be open (no continuity)
  - [ ] 5V to GND: Should be open
  - [ ] VBAT pads to GND: Should be open

## Power-On Test

- [ ] **Insert ESP32-S3-DevKitC-1 module** into headers (check orientation!)
- [ ] **Connect USB** to DevKit (not the NFF board USB if present)
- [ ] **Check power rails** (multimeter DC voltage)
  - [ ] 3.3V rail: 3.25V - 3.35V
  - [ ] 5V rail (if present): 4.9V - 5.1V

## LED Ring Test

- [ ] **Flash LED test firmware**
  ```bash
  cd firmware/domes
  idf.py build flash monitor
  ```
- [ ] **Verify LED behavior**
  - [ ] All 16 LEDs light up
  - [ ] Colors cycle correctly (R → G → B → W)
  - [ ] No flickering or dead LEDs
  - [ ] Brightness is uniform

**Troubleshooting:**
- No LEDs: Check level shifter (U1) is powered and LED data line (GPIO48)
- Some LEDs dead: Check solder joints on affected LED and previous LED in chain
- Flickering: Check 100nF decoupling caps near LEDs

## I2C Bus Test

- [ ] **Scan I2C bus** (add this to firmware or use logic analyzer)
  ```
  Expected devices:
  - 0x18 or 0x19: LIS2DW12 accelerometer
  - 0x5A: DRV2605L haptic driver
  ```

- [ ] **Accelerometer (LIS2DW12)**
  - [ ] WHO_AM_I register returns 0x44
  - [ ] Acceleration readings change when board is tilted
  - [ ] Tap detection triggers interrupt on GPIO3

- [ ] **Haptic Driver (DRV2605L)**
  - [ ] Device ACKs on I2C
  - [ ] Can read status register
  - [ ] (If LRA motor connected) Effect plays when triggered

**Troubleshooting:**
- No devices found: Check I2C pull-ups (R2, R3 = 4.7kΩ)
- Wrong address: LIS2DW12 SA0 pin determines address (0x18 or 0x19)

## Audio Test

- [ ] **Connect speaker** to SPK1 pads (check polarity)
- [ ] **Flash audio test firmware** (if available) or use I2S test
- [ ] **Verify audio output**
  - [ ] Sound is audible from speaker
  - [ ] No distortion at moderate volume
  - [ ] Audio SD pin (GPIO13) controls shutdown

**Troubleshooting:**
- No sound: Check Audio SD pin is HIGH (not shutdown)
- Distorted: Check speaker impedance (should be 8Ω)
- Quiet: Verify I2S clock configuration

## GPIO Breakout Test

- [ ] **Test GPIO breakout pads** (optional)
  - [ ] GPIO6: Can toggle as output
  - [ ] GPIO4: Can toggle as output
  - [ ] GPIO2: Can read as input
  - [ ] GPIO1: Can read as input
  - [ ] GND pads have continuity to ground

## Full System Test

- [ ] **Run all peripherals simultaneously**
  - [ ] LED animation running
  - [ ] IMU reading acceleration
  - [ ] Audio playing (if test sound available)
  - [ ] No crashes or watchdog resets for 10 minutes

- [ ] **Test config protocol**
  ```bash
  python3 tools/test_config.py /dev/ttyACM0
  ```
  - [ ] LIST_FEATURES returns all features
  - [ ] SET_FEATURE toggles LED effects

- [ ] **Test WiFi (if configured)**
  ```bash
  # From Windows (WSL2 limitation)
  /mnt/c/Python313/python.exe tools/test_config_wifi.py <DEVICE_IP>:5000
  ```

## Sign-Off

| Test | Result | Date | Tester |
|------|--------|------|--------|
| Visual Inspection | ☐ Pass / ☐ Fail | | |
| Power Rails | ☐ Pass / ☐ Fail | | |
| LED Ring | ☐ Pass / ☐ Fail | | |
| I2C Bus | ☐ Pass / ☐ Fail | | |
| Accelerometer | ☐ Pass / ☐ Fail | | |
| Haptic Driver | ☐ Pass / ☐ Fail | | |
| Audio | ☐ Pass / ☐ Fail | | |
| Full System | ☐ Pass / ☐ Fail | | |

**Board Serial Number:** _______________

**Notes:**
