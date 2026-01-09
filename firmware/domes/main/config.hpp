#pragma once

#include <cstdint>
#include "driver/gpio.h"

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
    constexpr gpio_num_t LED_DATA = GPIO_NUM_38;  // v1.1
    constexpr uint8_t LED_COUNT = 1;

    // Boot button
    constexpr gpio_num_t BUTTON_BOOT = GPIO_NUM_0;

    // Touch test pins (directly touchable on devkit)
    constexpr gpio_num_t TOUCH_1 = GPIO_NUM_1;
    constexpr gpio_num_t TOUCH_2 = GPIO_NUM_2;
    constexpr gpio_num_t TOUCH_3 = GPIO_NUM_3;
    constexpr gpio_num_t TOUCH_4 = GPIO_NUM_4;

    // I2C (directly touchable on devkit)
    constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
    constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;

    // I2S (directly touchable on devkit)
    constexpr gpio_num_t I2S_BCLK = GPIO_NUM_12;
    constexpr gpio_num_t I2S_LRCLK = GPIO_NUM_11;
    constexpr gpio_num_t I2S_DOUT = GPIO_NUM_13;
}

#endif // BOARD_DEVKITC1

// =============================================================================
// DOMES Pod v1 Pin Definitions (Future)
// =============================================================================
#ifdef BOARD_DOMES_V1

namespace pins {
    // LED Ring (16x SK6812 RGBW)
    constexpr gpio_num_t LED_DATA = GPIO_NUM_14;
    constexpr uint8_t LED_COUNT = 16;

    // Touch pads
    constexpr gpio_num_t TOUCH_1 = GPIO_NUM_1;
    constexpr gpio_num_t TOUCH_2 = GPIO_NUM_2;
    constexpr gpio_num_t TOUCH_3 = GPIO_NUM_3;
    constexpr gpio_num_t TOUCH_4 = GPIO_NUM_4;

    // I2C bus (DRV2605L @ 0x5A, LIS2DW12 @ 0x18)
    constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
    constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;

    // I2S audio (MAX98357A)
    constexpr gpio_num_t I2S_BCLK = GPIO_NUM_12;
    constexpr gpio_num_t I2S_LRCLK = GPIO_NUM_11;
    constexpr gpio_num_t I2S_DOUT = GPIO_NUM_13;

    // Battery ADC
    constexpr gpio_num_t BATTERY_ADC = GPIO_NUM_5;
}

#endif // BOARD_DOMES_V1

// =============================================================================
// Timing Constants
// =============================================================================
namespace timing {
    constexpr uint32_t LED_REFRESH_MS = 16;        // ~60 FPS
    constexpr uint32_t TOUCH_POLL_MS = 10;         // 100 Hz touch polling
    constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;
}

// =============================================================================
// LED Configuration
// =============================================================================
namespace led {
    constexpr uint8_t DEFAULT_BRIGHTNESS = 32;     // 0-255, start dim
    constexpr uint32_t RMT_RESOLUTION_HZ = 10000000; // 10 MHz = 100ns resolution
}

} // namespace domes::config
