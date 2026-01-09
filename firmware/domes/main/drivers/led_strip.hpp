#pragma once

#include "interfaces/i_led_driver.hpp"
#include "driver/gpio.h"
#include "led_strip.h"
#include <array>

namespace domes {

/// LED strip driver using ESP-IDF's RMT-based led_strip component
/// Supports WS2812 (RGB) and SK6812 (RGBW) addressable LEDs
template<uint8_t NUM_LEDS>
class LedStripDriver : public ILedDriver {
public:
    /// Construct LED strip driver
    /// @param gpio_pin GPIO pin connected to LED data line
    /// @param use_rgbw True for SK6812 RGBW, false for WS2812 RGB
    explicit LedStripDriver(gpio_num_t gpio_pin, bool use_rgbw = false)
        : gpio_pin_(gpio_pin)
        , use_rgbw_(use_rgbw)
        , brightness_(255)
        , strip_handle_(nullptr) {
        colors_.fill(Color::off());
    }

    ~LedStripDriver() override {
        if (strip_handle_) {
            led_strip_del(strip_handle_);
        }
    }

    // Non-copyable
    LedStripDriver(const LedStripDriver&) = delete;
    LedStripDriver& operator=(const LedStripDriver&) = delete;

    esp_err_t init() override {
        // Configure LED strip using RMT backend
        led_strip_config_t strip_config = {
            .strip_gpio_num = gpio_pin_,
            .max_leds = NUM_LEDS,
            .led_pixel_format = use_rgbw_ ? LED_PIXEL_FORMAT_GRBW : LED_PIXEL_FORMAT_GRB,
            .led_model = use_rgbw_ ? LED_MODEL_SK6812 : LED_MODEL_WS2812,
            .flags = {
                .invert_out = false,
            },
        };

        led_strip_rmt_config_t rmt_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000,  // 10 MHz = 100ns resolution
            .mem_block_symbols = 64,
            .flags = {
                .with_dma = false,
            },
        };

        esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_handle_);
        if (err != ESP_OK) {
            return err;
        }

        // Clear all LEDs on init
        return clear();
    }

    esp_err_t setPixel(uint8_t index, Color color) override {
        if (index >= NUM_LEDS) {
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
        if (strip_handle_) {
            return led_strip_clear(strip_handle_);
        }
        return ESP_OK;
    }

    esp_err_t refresh() override {
        if (!strip_handle_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Apply brightness scaling and send to strip
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            const auto& c = colors_[i];
            uint8_t r = scale8(c.r, brightness_);
            uint8_t g = scale8(c.g, brightness_);
            uint8_t b = scale8(c.b, brightness_);

            esp_err_t err;
            if (use_rgbw_) {
                uint8_t w = scale8(c.w, brightness_);
                err = led_strip_set_pixel_rgbw(strip_handle_, i, r, g, b, w);
            } else {
                err = led_strip_set_pixel(strip_handle_, i, r, g, b);
            }

            if (err != ESP_OK) {
                return err;
            }
        }

        return led_strip_refresh(strip_handle_);
    }

    void setBrightness(uint8_t brightness) override {
        brightness_ = brightness;
    }

    uint8_t getLedCount() const override {
        return NUM_LEDS;
    }

private:
    /// Scale a value by brightness (0-255)
    static uint8_t scale8(uint8_t value, uint8_t scale) {
        return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) >> 8);
    }

    gpio_num_t gpio_pin_;
    bool use_rgbw_;
    uint8_t brightness_;
    led_strip_handle_t strip_handle_;
    std::array<Color, NUM_LEDS> colors_;
};

} // namespace domes
