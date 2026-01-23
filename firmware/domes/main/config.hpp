#pragma once

#include "driver/gpio.h"

#include <cstdint>

namespace domes::config {

// =============================================================================
// Board Selection
// =============================================================================
// Uncomment ONE of these to select the target board
#define BOARD_DEVKITC1  // ESP32-S3-DevKitC-1 (development)
// #define BOARD_DOMES_V1  // DOMES Pod v1 PCB (production)

// =============================================================================
// DevKitC-1 Pin Definitions
// =============================================================================
#ifdef BOARD_DEVKITC1

namespace pins {
// Built-in RGB LED (WS2812)
// NOTE: DevKitC-1 v1.0 uses GPIO48, v1.1 uses GPIO38
constexpr gpio_num_t kLedData = GPIO_NUM_38;  // v1.1
constexpr uint8_t kLedCount = 1;

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
// DOMES Pod v1 Pin Definitions (Future)
// =============================================================================
#ifdef BOARD_DOMES_V1

namespace pins {
// LED Ring (16x SK6812 RGBW)
constexpr gpio_num_t kLedData = GPIO_NUM_14;
constexpr uint8_t kLedCount = 16;

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
// Initialization Timing (main.cpp)
// =============================================================================
namespace init_timing {
constexpr uint32_t kLogFlushDelayMs = 100;      // Delay to flush logs before stack init
constexpr uint32_t kBleSettleDelayMs = 500;     // Let BLE stack settle after init
constexpr uint32_t kWifiConnectTimeoutS = 30;   // Max wait for WiFi connection
constexpr uint32_t kStatusIndicatorMs = 2000;   // LED status indication duration
constexpr uint32_t kMinHeapForSelfTest = 50000; // Minimum heap for self-test pass
}  // namespace init_timing

// =============================================================================
// LED Configuration
// =============================================================================
namespace led {
constexpr uint8_t kDefaultBrightness = 32;       // 0-255, start dim
constexpr uint32_t kRmtResolutionHz = 10000000;  // 10 MHz = 100ns resolution
}  // namespace led

}  // namespace domes::config
