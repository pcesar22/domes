#pragma once

/**
 * @file max98357a.hpp
 * @brief MAX98357A I2S amplifier driver
 *
 * Drives the MAX98357A Class D amplifier via I2S interface.
 * Uses ESP-IDF v5.x I2S driver API.
 *
 * Hardware: MAX98357A with shutdown control via SD pin
 * - BCLK: Bit clock
 * - LRCLK: Word select (left/right clock)
 * - DIN: Data in (from ESP32)
 * - SD: Shutdown (HIGH=enabled, LOW=shutdown)
 */

#include "interfaces/iAudioDriver.hpp"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#include <algorithm>
#include <cstdint>

namespace domes {

/**
 * @brief MAX98357A I2S amplifier driver
 *
 * Configures I2S for 16kHz, 16-bit mono output.
 * Manages amplifier shutdown pin for power control.
 *
 * @code
 * Max98357aDriver audio(GPIO_NUM_12, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_7);
 * audio.init();
 * audio.start();
 * audio.write(samples, count, &written);
 * audio.stop();
 * @endcode
 */
class Max98357aDriver : public IAudioDriver {
public:
    /**
     * @brief Construct MAX98357A driver
     *
     * @param bclk Bit clock GPIO
     * @param lrclk Word select (LRCLK) GPIO
     * @param dout Data out GPIO
     * @param sd Shutdown control GPIO (HIGH=enabled)
     */
    Max98357aDriver(gpio_num_t bclk, gpio_num_t lrclk, gpio_num_t dout, gpio_num_t sd)
        : bclkPin_(bclk),
          lrclkPin_(lrclk),
          doutPin_(dout),
          sdPin_(sd),
          txHandle_(nullptr),
          volume_(80),
          initialized_(false),
          started_(false) {}

    ~Max98357aDriver() override {
        if (started_) {
            stop();
        }
        if (txHandle_) {
            i2s_del_channel(txHandle_);
            txHandle_ = nullptr;
        }
    }

    // Non-copyable
    Max98357aDriver(const Max98357aDriver&) = delete;
    Max98357aDriver& operator=(const Max98357aDriver&) = delete;

    esp_err_t init() override {
        if (initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Configure SD pin (amplifier shutdown control)
        gpio_config_t gpioConfig = {
            .pin_bit_mask = (1ULL << sdPin_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&gpioConfig);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "GPIO config failed: %s", esp_err_to_name(err));
            return err;
        }

        // Start with amplifier disabled
        gpio_set_level(sdPin_, 0);

        // Configure I2S channel
        i2s_chan_config_t chanConfig = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = kDmaBufferCount,
            .dma_frame_num = kDmaBufferFrames,
            .auto_clear = true,  // Clear DMA buffer on underrun
        };

        err = i2s_new_channel(&chanConfig, &txHandle_, nullptr);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "I2S channel creation failed: %s", esp_err_to_name(err));
            return err;
        }

        // Configure I2S standard mode (Philips I2S format)
        i2s_std_config_t stdConfig = {
            .clk_cfg = {
                .sample_rate_hz = kSampleRate,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_MONO
            ),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclkPin_,
                .ws = lrclkPin_,
                .dout = doutPin_,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        err = i2s_channel_init_std_mode(txHandle_, &stdConfig);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "I2S init failed: %s", esp_err_to_name(err));
            i2s_del_channel(txHandle_);
            txHandle_ = nullptr;
            return err;
        }

        initialized_ = true;
        ESP_LOGI(kTag, "MAX98357A initialized (BCLK=%d, LRCLK=%d, DOUT=%d, SD=%d)",
                 bclkPin_, lrclkPin_, doutPin_, sdPin_);
        return ESP_OK;
    }

    esp_err_t start() override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }
        if (started_) {
            return ESP_OK;  // Already started
        }

        // Enable I2S channel
        esp_err_t err = i2s_channel_enable(txHandle_);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "I2S enable failed: %s", esp_err_to_name(err));
            return err;
        }

        // Enable amplifier (SD pin HIGH)
        gpio_set_level(sdPin_, 1);

        started_ = true;
        ESP_LOGD(kTag, "Audio started");
        return ESP_OK;
    }

    esp_err_t stop() override {
        if (!started_) {
            return ESP_OK;
        }

        // Disable amplifier first (SD pin LOW)
        gpio_set_level(sdPin_, 0);

        // Disable I2S channel
        esp_err_t err = i2s_channel_disable(txHandle_);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "I2S disable failed: %s", esp_err_to_name(err));
        }

        started_ = false;
        ESP_LOGD(kTag, "Audio stopped");
        return ESP_OK;
    }

    esp_err_t write(const int16_t* samples, size_t count,
                    size_t* written, uint32_t timeoutMs = 1000) override {
        if (!started_) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!samples || count == 0) {
            if (written) *written = 0;
            return ESP_OK;
        }

        // Apply volume scaling
        // Use a local buffer to avoid modifying input
        // For efficiency, scale in chunks
        constexpr size_t kChunkSize = 256;
        int16_t scaledBuffer[kChunkSize];

        size_t totalWritten = 0;
        size_t remaining = count;
        const int16_t* src = samples;

        while (remaining > 0) {
            size_t chunkSamples = std::min(remaining, kChunkSize);

            // Scale samples by volume
            for (size_t i = 0; i < chunkSamples; ++i) {
                int32_t scaled = (static_cast<int32_t>(src[i]) * volume_) / 100;
                scaledBuffer[i] = static_cast<int16_t>(
                    std::clamp(scaled, static_cast<int32_t>(-32768), static_cast<int32_t>(32767))
                );
            }

            // Write to I2S
            size_t bytesWritten = 0;
            esp_err_t err = i2s_channel_write(txHandle_, scaledBuffer,
                                              chunkSamples * sizeof(int16_t),
                                              &bytesWritten,
                                              pdMS_TO_TICKS(timeoutMs));

            size_t samplesWritten = bytesWritten / sizeof(int16_t);
            totalWritten += samplesWritten;
            src += samplesWritten;
            remaining -= samplesWritten;

            if (err == ESP_ERR_TIMEOUT) {
                ESP_LOGW(kTag, "I2S write timeout");
                break;
            }
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "I2S write failed: %s", esp_err_to_name(err));
                if (written) *written = totalWritten;
                return err;
            }
        }

        if (written) *written = totalWritten;
        return ESP_OK;
    }

    void setVolume(uint8_t volume) override {
        volume_ = std::min(volume, static_cast<uint8_t>(100));
    }

    uint8_t getVolume() const override { return volume_; }

    bool isInitialized() const override { return initialized_; }

    bool isStarted() const override { return started_; }

private:
    static constexpr const char* kTag = "max98357a";
    static constexpr uint32_t kSampleRate = 16000;
    static constexpr size_t kDmaBufferCount = 4;
    static constexpr size_t kDmaBufferFrames = 256;  // Samples per DMA buffer

    gpio_num_t bclkPin_;
    gpio_num_t lrclkPin_;
    gpio_num_t doutPin_;
    gpio_num_t sdPin_;

    i2s_chan_handle_t txHandle_;
    uint8_t volume_;
    bool initialized_;
    bool started_;
};

}  // namespace domes
