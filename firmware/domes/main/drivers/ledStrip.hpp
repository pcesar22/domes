#pragma once

#include "driver/gpio.h"
#include "interfaces/iLedDriver.hpp"
#include "led_strip.h"

#include <array>
#include <cstdint>

namespace domes {

/**
 * @brief LED strip driver using ESP-IDF's RMT-based led_strip component
 *
 * Supports WS2812 (RGB) and SK6812 (RGBW) addressable LEDs. Uses a static
 * buffer sized at compile time via template parameter.
 *
 * @tparam kNumLeds Number of LEDs in the strip (compile-time constant)
 *
 * @note Uses RMT backend by default. For boards with Octal PSRAM (like
 *       DevKitC-1 v1.1), consider using SPI backend directly instead.
 */
template <uint8_t kNumLeds>
class LedStripDriver : public ILedDriver {
public:
    /**
     * @brief Construct LED strip driver
     *
     * @param gpioPin GPIO pin connected to LED data line
     * @param useRgbw True for SK6812 RGBW, false for WS2812 RGB
     */
    explicit LedStripDriver(gpio_num_t gpioPin, bool useRgbw = false)
        : gpioPin_(gpioPin), useRgbw_(useRgbw), brightness_(255), stripHandle_(nullptr) {
        colors_.fill(Color::off());
    }

    ~LedStripDriver() override {
        if (stripHandle_) {
            led_strip_del(stripHandle_);
        }
    }

    // Non-copyable
    LedStripDriver(const LedStripDriver&) = delete;
    LedStripDriver& operator=(const LedStripDriver&) = delete;

    esp_err_t init() override {
        led_strip_config_t stripConfig = {
            .strip_gpio_num = gpioPin_,
            .max_leds = kNumLeds,
            .led_pixel_format = useRgbw_ ? LED_PIXEL_FORMAT_GRBW : LED_PIXEL_FORMAT_GRB,
            .led_model = useRgbw_ ? LED_MODEL_SK6812 : LED_MODEL_WS2812,
            .flags =
                {
                    .invert_out = false,
                },
        };

        led_strip_rmt_config_t rmtConfig = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = kRmtResolutionHz,
            .mem_block_symbols = kMemBlockSymbols,
            .flags =
                {
                    .with_dma = false,
                },
        };

        esp_err_t err = led_strip_new_rmt_device(&stripConfig, &rmtConfig, &stripHandle_);
        if (err != ESP_OK) {
            return err;
        }

        return clear();
    }

    esp_err_t setPixel(uint8_t index, Color color) override {
        if (index >= kNumLeds) {
            return ESP_ERR_INVALID_ARG;
        }
        colors_[index] = color;
        return ESP_OK;
    }

    esp_err_t setAll(Color color) override {
        colors_.fill(color);
        return ESP_OK;
    }

    esp_err_t clear() override {
        colors_.fill(Color::off());
        if (stripHandle_) {
            return led_strip_clear(stripHandle_);
        }
        return ESP_OK;
    }

    esp_err_t refresh() override {
        if (!stripHandle_) {
            return ESP_ERR_INVALID_STATE;
        }

        for (uint8_t i = 0; i < kNumLeds; ++i) {
            const auto& c = colors_[i];
            uint8_t r = scale8(c.r, brightness_);
            uint8_t g = scale8(c.g, brightness_);
            uint8_t b = scale8(c.b, brightness_);

            esp_err_t err;
            if (useRgbw_) {
                uint8_t w = scale8(c.w, brightness_);
                err = led_strip_set_pixel_rgbw(stripHandle_, i, r, g, b, w);
            } else {
                err = led_strip_set_pixel(stripHandle_, i, r, g, b);
            }

            if (err != ESP_OK) {
                return err;
            }
        }

        return led_strip_refresh(stripHandle_);
    }

    void setBrightness(uint8_t brightness) override { brightness_ = brightness; }

    uint8_t getLedCount() const override { return kNumLeds; }

private:
    static constexpr uint32_t kRmtResolutionHz = 10 * 1000 * 1000;  ///< 10 MHz = 100ns
    static constexpr size_t kMemBlockSymbols = 64;

    /**
     * @brief Scale a value by brightness factor
     * @param value Input value (0-255)
     * @param scale Scale factor (0-255)
     * @return Scaled value
     */
    static uint8_t scale8(uint8_t value, uint8_t scale) {
        return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) >> 8);
    }

    gpio_num_t gpioPin_;
    bool useRgbw_;
    uint8_t brightness_;
    led_strip_handle_t stripHandle_;
    std::array<Color, kNumLeds> colors_;
};

}  // namespace domes
