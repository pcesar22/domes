#pragma once

#include "driver/gpio.h"

#include <cstdint>

namespace domes::config {

// =============================================================================
// Board Selection
// =============================================================================
// Uncomment ONE of these to select the target board
// #define BOARD_DEVKITC1    // ESP32-S3-DevKitC-1 standalone (development)
#define BOARD_NFF_DEVBOARD   // NFF devboard with DevKitC-1 plugged in
// #define BOARD_DOMES_V1    // DOMES Pod v1 PCB (production)

// =============================================================================
// DevKitC-1 Pin Definitions
// =============================================================================
#ifdef BOARD_DEVKITC1

namespace pins {
// Built-in RGB LED (WS2812)
// NOTE: DevKitC-1 v1.0 uses GPIO48, v1.1 uses GPIO38
constexpr gpio_num_t kLedData = GPIO_NUM_38;  // v1.1
constexpr uint8_t kLedCount = 1;
constexpr bool kLedIsRgbw = false;  // WS2812 is RGB only

// Boot button
constexpr gpio_num_t kButtonBoot = GPIO_NUM_0;

// Touch test pins (directly touchable on devkit)
constexpr gpio_num_t kTouch1 = GPIO_NUM_1;
constexpr gpio_num_t kTouch2 = GPIO_NUM_2;
constexpr gpio_num_t kTouch3 = GPIO_NUM_3;
constexpr gpio_num_t kTouch4 = GPIO_NUM_4;

// I2C (directly touchable on devkit)
constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kI2cScl = GPIO_NUM_9;

// I2S (directly touchable on devkit)
constexpr gpio_num_t kI2sBclk = GPIO_NUM_12;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_11;
constexpr gpio_num_t kI2sDout = GPIO_NUM_13;
}  // namespace pins

#endif  // BOARD_DEVKITC1

// =============================================================================
// NFF Development Board Pin Definitions
// =============================================================================
// DevKitC-1 plugged into NFF board with SK6812 ring, IMU, haptic, audio
// Pin mapping from schematic: ESP32-S3-DEVKIT_Sensor_Project V1.0
#ifdef BOARD_NFF_DEVBOARD

namespace pins {
// LED Ring (16x SK6812MINI-E RGBW via SN74AHCT1G125 level shifter)
// H1 pin 9 = LED_DATA_3V3 = ESP32 GPIO16
constexpr gpio_num_t kLedData = GPIO_NUM_16;
constexpr uint8_t kLedCount = 16;
constexpr bool kLedIsRgbw = true;  // SK6812 has white channel

// I2C bus (LIS2DW12 @ 0x19, DRV2605L @ 0x5A)
// SA0 tied to 3.3V sets LIS2DW12 address to 0x19
constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kI2cScl = GPIO_NUM_9;

// I2C device addresses
constexpr uint8_t kLis2dw12Addr = 0x19;  // LIS2DW12 with SA0=high
constexpr uint8_t kDrv2605lAddr = 0x5A;  // DRV2605L haptic driver


// IMU interrupt (LIS2DW12 INT1)
// H1 pin 5 = IMU_INT = ESP32 GPIO5
constexpr gpio_num_t kImuInt1 = GPIO_NUM_5;

// I2S audio (MAX98357A)
// H1 pin 18 = I2S_BCLK = GPIO12
// H1 pin 17 = I2S_LRCLK = GPIO11
// H1 pin 19 = I2S_DIN = GPIO13
constexpr gpio_num_t kI2sBclk = GPIO_NUM_12;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_11;
constexpr gpio_num_t kI2sDout = GPIO_NUM_13;

// Audio amplifier shutdown (MAX98357A SD_MODE#)
// H1 pin 7 = AMP_SD = GPIO7
// High or floating = enabled, Low = shutdown
constexpr gpio_num_t kAudioSd = GPIO_NUM_7;
}  // namespace pins

#endif  // BOARD_NFF_DEVBOARD

// =============================================================================
// DOMES Pod v1 Pin Definitions (Future)
// =============================================================================
#ifdef BOARD_DOMES_V1

namespace pins {
// LED Ring (16x SK6812 RGBW)
constexpr gpio_num_t kLedData = GPIO_NUM_14;
constexpr uint8_t kLedCount = 16;
constexpr bool kLedIsRgbw = true;  // SK6812 has white channel

// Touch pads
constexpr gpio_num_t kTouch1 = GPIO_NUM_1;
constexpr gpio_num_t kTouch2 = GPIO_NUM_2;
constexpr gpio_num_t kTouch3 = GPIO_NUM_3;
constexpr gpio_num_t kTouch4 = GPIO_NUM_4;

// I2C bus (DRV2605L @ 0x5A, LIS2DW12 @ 0x18)
constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kI2cScl = GPIO_NUM_9;

// I2S audio (MAX98357A)
constexpr gpio_num_t kI2sBclk = GPIO_NUM_12;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_11;
constexpr gpio_num_t kI2sDout = GPIO_NUM_13;

// Battery ADC
constexpr gpio_num_t kBatteryAdc = GPIO_NUM_5;
}  // namespace pins

#endif  // BOARD_DOMES_V1

// =============================================================================
// Timing Constants
// =============================================================================
namespace timing {
constexpr uint32_t kLedRefreshMs = 16;  // ~60 FPS
constexpr uint32_t kTouchPollMs = 10;   // 100 Hz touch polling
constexpr uint32_t kWatchdogTimeoutS = 10;
}  // namespace timing

// =============================================================================
// LED Configuration
// =============================================================================
namespace led {
constexpr uint8_t kDefaultBrightness = 32;       // 0-255, start dim
constexpr uint32_t kRmtResolutionHz = 10000000;  // 10 MHz = 100ns resolution
}  // namespace led

}  // namespace domes::config
