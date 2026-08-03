#pragma once

#include "driver/gpio.h"

#include <cstdint>

namespace domes::config {

// =============================================================================
// Active Board Profile
// =============================================================================
// The NFF carrier with an ESP32-S3 N8R8 module is the only supported firmware
// target. Add another complete, build-tested profile before introducing board
// selection again.
//
// Pin mapping from schematic: ESP32-S3-DEVKIT_Sensor_Project V1.0.

namespace pins {
// UART0 to the DevKit CP2102N bridge. ESP-IDF does not route these pins when
// the console is assigned to native USB Serial/JTAG, so the runtime transport
// must configure both signals explicitly.
constexpr gpio_num_t kUartTx = GPIO_NUM_43;
constexpr gpio_num_t kUartRx = GPIO_NUM_44;

// LED Ring (16x SK6812MINI-E via SN74AHCT1G125 level shifter)
// H1 pin 9 = LED_DATA_3V3 = ESP32 GPIO16
// NOTE: The SK6812MINI-E on this board appears to be RGB, not RGBW
constexpr gpio_num_t kLedData = GPIO_NUM_16;
constexpr uint8_t kLedCount = 16;
constexpr bool kLedIsRgbw = false;  // Using RGB mode

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

// Touch pads (4x 15mm copper pads with 1mm guard ring)
// From schematic: K1=GPIO1, K2=GPIO2, K3=GPIO4, K4=GPIO6
constexpr gpio_num_t kTouch1 = GPIO_NUM_1;
constexpr gpio_num_t kTouch2 = GPIO_NUM_2;
constexpr gpio_num_t kTouch3 = GPIO_NUM_4;
constexpr gpio_num_t kTouch4 = GPIO_NUM_6;
constexpr uint8_t kTouchPadCount = 4;
}  // namespace pins

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
