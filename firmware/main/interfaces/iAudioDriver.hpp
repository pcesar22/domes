/**
 * @file iAudioDriver.hpp
 * @brief Abstract interface for audio driver implementations
 */

#pragma once

#include "esp_err.h"
#include <cstdint>
#include <cstddef>

/**
 * @brief Audio playback state
 */
enum class AudioState : uint8_t {
    Idle,       ///< No audio playing
    Playing,    ///< Audio currently playing
    Paused      ///< Playback paused
};

/**
 * @brief Abstract audio driver interface
 *
 * Provides a hardware-independent interface for audio playback
 * via I2S (MAX98357A amplifier).
 */
class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;

    /**
     * @brief Initialize the audio driver hardware
     * @return ESP_OK on success
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Play a sound by ID from flash storage
     * @param soundId Identifier of the sound to play
     * @return ESP_OK on success, ESP_ERR_NOT_FOUND if sound not found
     */
    virtual esp_err_t playSound(uint8_t soundId) = 0;

    /**
     * @brief Play raw audio samples
     * @param samples Pointer to audio sample buffer (16-bit PCM)
     * @param length Number of samples
     * @return ESP_OK on success
     */
    virtual esp_err_t playSamples(const int16_t* samples, size_t length) = 0;

    /**
     * @brief Play a simple tone
     * @param frequencyHz Tone frequency in Hz
     * @param durationMs Duration in milliseconds
     * @return ESP_OK on success
     */
    virtual esp_err_t playTone(uint16_t frequencyHz, uint16_t durationMs) = 0;

    /**
     * @brief Stop any currently playing audio
     * @return ESP_OK on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Pause audio playback
     * @return ESP_OK on success
     */
    virtual esp_err_t pause() = 0;

    /**
     * @brief Resume paused audio playback
     * @return ESP_OK on success
     */
    virtual esp_err_t resume() = 0;

    /**
     * @brief Set audio volume
     * @param volume Volume level (0-255)
     */
    virtual void setVolume(uint8_t volume) = 0;

    /**
     * @brief Get current volume
     * @return Current volume level (0-255)
     */
    virtual uint8_t getVolume() const = 0;

    /**
     * @brief Get current playback state
     * @return Current audio state
     */
    virtual AudioState getState() const = 0;

    /**
     * @brief Check if audio is currently playing
     * @return true if playing, false otherwise
     */
    virtual bool isPlaying() const = 0;

    /**
     * @brief Enable or disable the amplifier
     * @param enable true to enable, false to disable (power save)
     * @return ESP_OK on success
     */
    virtual esp_err_t setEnabled(bool enable) = 0;
};
