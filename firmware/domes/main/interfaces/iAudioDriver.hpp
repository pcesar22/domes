#pragma once

/**
 * @file iAudioDriver.hpp
 * @brief Abstract interface for audio drivers
 *
 * Provides hardware-agnostic interface for I2S audio output.
 * Implementations handle specific DAC/amplifier chips.
 */

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace domes {

/**
 * @brief Abstract interface for audio drivers
 *
 * Controls I2S output to DAC/amplifier for audio playback.
 * Implementations should handle DMA buffer management.
 *
 * @note Call init() before any other methods.
 */
class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;

    /**
     * @brief Initialize I2S peripheral and amplifier
     *
     * Configures I2S in master TX mode with appropriate format.
     * Does NOT start playback - call start() first.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already initialized
     * @return ESP_FAIL on hardware failure
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Start I2S transmission and enable amplifier
     *
     * Must be called before write(). Enables amplifier via SD pin.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not initialized or already started
     */
    virtual esp_err_t start() = 0;

    /**
     * @brief Stop I2S transmission and disable amplifier
     *
     * Drains remaining DMA buffers, then stops I2S and disables amplifier.
     *
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Write PCM samples to I2S DMA buffer
     *
     * Blocks until samples are accepted by DMA or timeout.
     *
     * @param samples Pointer to 16-bit signed PCM samples
     * @param count Number of samples to write
     * @param written Output: number of samples actually written
     * @param timeoutMs Maximum time to wait for DMA buffer space
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not started
     * @return ESP_ERR_TIMEOUT if DMA buffers full
     */
    virtual esp_err_t write(const int16_t* samples, size_t count,
                            size_t* written, uint32_t timeoutMs = 1000) = 0;

    /**
     * @brief Set output volume (software scaling)
     *
     * @param volume Volume level 0-100 (0=mute, 100=full)
     */
    virtual void setVolume(uint8_t volume) = 0;

    /**
     * @brief Get current volume level
     * @return Volume 0-100
     */
    virtual uint8_t getVolume() const = 0;

    /**
     * @brief Check if driver is initialized
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Check if I2S is currently transmitting
     */
    virtual bool isStarted() const = 0;
};

}  // namespace domes
