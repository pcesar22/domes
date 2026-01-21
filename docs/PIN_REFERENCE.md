# GPIO Pin Reference

**Single source of truth for all DOMES hardware platforms.**

Last updated: 2026-01-18

---

## Platform Comparison

| Function | DevKitC-1 v1.0 | DevKitC-1 v1.1 | NFF Devboard | Production (planned) |
|----------|----------------|----------------|--------------|----------------------|
| **LED Data** | GPIO48 | GPIO38 | GPIO48 | GPIO14 |
| **I2C SDA** | - | - | GPIO8 | GPIO8 |
| **I2C SCL** | - | - | GPIO9 | GPIO9 |
| **I2S BCLK** | - | - | GPIO12 | GPIO12 |
| **I2S LRCLK** | - | - | GPIO11 | GPIO11 |
| **I2S DATA** | - | - | GPIO10 | GPIO13 |
| **Audio SD** | - | - | GPIO13 | GPIO7 |
| **IMU INT1** | - | - | GPIO3 | GPIO5 |
| **Touch** | GPIO1-4 | GPIO1-4 | - | GPIO1-4 |
| **Battery ADC** | - | - | - | GPIO10 |

---

## ESP32-S3-DevKitC-1

Espressif's official development board. Two versions exist with different LED pins.

### Version Detection

- **v1.0**: LED on GPIO48 (no silkscreen version marking)
- **v1.1**: LED on GPIO38 (check for "v1.1" on silkscreen)

### Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| RGB LED | 38 (v1.1) / 48 (v1.0) | WS2812, use SPI backend with PSRAM |
| Boot Button | 0 | Active low, directly usable |
| USB D+ | 20 | Native USB (internal) |
| USB D- | 19 | Native USB (internal) |
| Touch Capable | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 | ESP32-S3 touch pins |

### Important Notes

- Use **SPI backend** for LED strip driver (RMT conflicts with Octal PSRAM)
- GPIO38 on v1.1 is directly connected to WS2812 data pin
- All GPIOs except 19/20 (USB) are available for testing

---

## NFF Development Board

Custom sensor board that accepts an ESP32-S3-DevKitC-1 module via headers.
Full fabrication files: `hardware/nff-devboard/`

### Pin Mapping

| Function | GPIO | Peripheral | Notes |
|----------|------|------------|-------|
| **LED Ring** | 48 | SPI/RMT | 16x SK6812MINI-E via SN74AHCT1G125 level shifter |
| **I2C SDA** | 8 | I2C0 | Shared bus: LIS2DW12, DRV2605L |
| **I2C SCL** | 9 | I2C0 | Shared bus: LIS2DW12, DRV2605L |
| **IMU INT1** | 3 | GPIO | LIS2DW12 interrupt, active low |
| **I2S BCLK** | 12 | I2S0 | MAX98357A bit clock |
| **I2S LRCLK** | 11 | I2S0 | MAX98357A word select |
| **I2S DATA** | 10 | I2S0 | MAX98357A data in |
| **Audio SD** | 13 | GPIO | MAX98357A shutdown, active low |

### I2C Bus

| Device | Address | Function |
|--------|---------|----------|
| LIS2DW12 | 0x18 or 0x19 | 3-axis accelerometer with tap detection |
| DRV2605L | 0x5A | Haptic driver for LRA motor |

### Peripheral Specifications

| Peripheral | Part Number | Specifications |
|------------|-------------|----------------|
| LEDs | SK6812MINI-E | RGBW, 3.5x3.5mm, 800kHz protocol |
| Accelerometer | LIS2DW12TR | LGA-12, ±2/4/8/16g, tap detection |
| Haptic Driver | DRV2605LDGSR | I2C, LRA support, 123 effects |
| Audio Amp | MAX98357AETE+T | I2S, 3.2W Class D, filterless |
| Speaker | GSPK2307P-8R1W | 23mm, 8Ω, 1W |
| Level Shifter | SN74AHCT1G125DBV | 3.3V→5V for LED data |

---

## Production PCB (Planned)

Final form-factor design for enclosed pods. Pin assignments are preliminary.

### Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| LED Ring | 14 | 16x SK6812 RGBW |
| I2S BCLK | 12 | MAX98357A |
| I2S LRCLK | 11 | MAX98357A |
| I2S DIN | 13 | MAX98357A |
| I2C SDA | 8 | DRV2605L, LIS2DW12 |
| I2C SCL | 9 | DRV2605L, LIS2DW12 |
| Touch | 1, 2, 3, 4 | Capacitive pads under diffuser |
| IMU INT | 5 | Wake from sleep |
| Haptic EN | 6 | DRV2605L enable (optional) |
| Speaker EN | 7 | MAX98357A SD pin |
| Battery ADC | 10 | Voltage divider for SoC |

### JTAG Debug (TagConnect TC2030)

| Pin | Signal | GPIO |
|-----|--------|------|
| 1 | VTREF | 3.3V |
| 2 | TMS | 42 |
| 3 | GND | GND |
| 4 | TCK | 39 |
| 5 | TDO | 40 |
| 6 | TDI | 41 |

---

## Configuration in Code

Pin definitions are centralized in `firmware/domes/main/config.hpp`:

```cpp
namespace domes::config {

// Platform-specific LED pin
#if defined(CONFIG_DOMES_PLATFORM_DEVKIT_V11)
    constexpr gpio_num_t kLedPin = GPIO_NUM_38;
#elif defined(CONFIG_DOMES_PLATFORM_NFF_DEVBOARD)
    constexpr gpio_num_t kLedPin = GPIO_NUM_48;
#else
    constexpr gpio_num_t kLedPin = GPIO_NUM_14;  // Production default
#endif

// I2C bus (shared across platforms with I2C)
constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kI2cScl = GPIO_NUM_9;

// I2S audio
constexpr gpio_num_t kI2sBclk = GPIO_NUM_12;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_11;
constexpr gpio_num_t kI2sData = GPIO_NUM_10;

}  // namespace domes::config
```

---

## Troubleshooting

### LED Not Working

1. **DevKitC-1**: Check board version (v1.0 vs v1.1) - different GPIOs
2. **DevKitC-1 with PSRAM**: Must use SPI backend, not RMT
3. **NFF Devboard**: Verify level shifter is powered (3.3V rail)

### I2C Devices Not Detected

```bash
# Scan I2C bus from firmware
i2c_master_bus_scan(bus_handle, ...);
```

Expected addresses on NFF devboard:
- 0x18 or 0x19: LIS2DW12 (SA0 pin determines address)
- 0x5A: DRV2605L

### Audio Not Working

1. Check Audio SD pin is HIGH (GPIO13 on NFF devboard)
2. Verify I2S clock configuration matches MAX98357A requirements
3. Check speaker connection (positive to OUT+, negative to OUT-)

---

## References

- [ESP32-S3-DevKitC-1 Schematic](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html)
- [NFF Devboard Schematic](../hardware/nff-devboard/docs/schematic.pdf)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
