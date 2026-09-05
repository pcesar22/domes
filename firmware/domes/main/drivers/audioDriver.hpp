#pragma once

/**
 * @file audioDriver.hpp
 * @brief Bounded generated-tone streaming for audio drivers
 */

#include "interfaces/iAudioDriver.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace domes {

/**
 * Streams generated PCM in fixed-size chunks owned by the audio service.
 *
 * IAudioDriver::write() copies each chunk into driver-managed DMA storage before
 * returning, so this source buffer does not require DMA capabilities. Keeping it
 * under the audio task's single-owner service avoids a persistent 32,000-byte
 * internal-heap allocation and any associated allocation-failure or fragmentation
 * path.
 */
class AudioToneWriter {
public:
    static constexpr uint32_t kSampleRate = 16000;
    static constexpr size_t kMaxSamples = kSampleRate;  // One second.
    static constexpr size_t kChunkSamples = 256;
    static constexpr size_t kBufferBytes = kChunkSamples * sizeof(int16_t);

    /**
     * Generate and synchronously write one bounded tone.
     *
     * @return ESP_OK only when every generated sample was accepted by the driver.
     */
    esp_err_t write(IAudioDriver& driver, uint16_t frequencyHz, uint16_t durationMs) {
        const size_t requestedSamples = (static_cast<size_t>(kSampleRate) * durationMs) / 1000;
        const size_t totalSamples = std::min(requestedSamples, kMaxSamples);
        constexpr size_t kFadeSamples = (kSampleRate * 10) / 1000;
        constexpr float kTwoPi = 2.0f * 3.14159265358979f;
        constexpr int16_t kAmplitude = 24000;

        float phase = 0.0f;
        const float phaseIncrement = kTwoPi * frequencyHz / kSampleRate;
        size_t offset = 0;
        while (offset < totalSamples) {
            const size_t chunkSamples = std::min(kChunkSamples, totalSamples - offset);
            for (size_t i = 0; i < chunkSamples; ++i) {
                const size_t sampleIndex = offset + i;
                int16_t sample = static_cast<int16_t>(kAmplitude * sinf(phase));
                phase += phaseIncrement;
                if (phase >= kTwoPi) {
                    phase -= kTwoPi;
                }

                if (kFadeSamples * 2 < totalSamples) {
                    if (sampleIndex < kFadeSamples) {
                        const float gain = static_cast<float>(sampleIndex) / kFadeSamples;
                        sample = static_cast<int16_t>(sample * gain);
                    } else if (sampleIndex >= totalSamples - kFadeSamples) {
                        const size_t reverseIndex = totalSamples - 1 - sampleIndex;
                        const float gain =
                            static_cast<float>(kFadeSamples - reverseIndex) / kFadeSamples;
                        sample = static_cast<int16_t>(sample * gain);
                    }
                }
                buffer_[i] = sample;
            }

            size_t written = 0;
            const esp_err_t err = driver.write(buffer_.data(), chunkSamples, &written);
            if (err != ESP_OK) {
                return err;
            }
            if (written != chunkSamples) {
                return ESP_FAIL;
            }
            offset += chunkSamples;
        }
        return ESP_OK;
    }

private:
    std::array<int16_t, kChunkSamples> buffer_{};
};

static_assert(sizeof(AudioToneWriter) == AudioToneWriter::kBufferBytes);

}  // namespace domes
