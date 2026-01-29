#pragma once

/**
 * @file audioService.hpp
 * @brief Audio playback service with FreeRTOS task
 *
 * Provides non-blocking audio playback via a dedicated task.
 * Supports playing embedded assets and generated tones.
 */

#include "config/featureManager.hpp"
#include "interfaces/iAudioDriver.hpp"
#include "trace/traceApi.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <atomic>
#include <cmath>
#include <cstring>

namespace domes {

/**
 * @brief Audio asset descriptor
 */
struct AudioAsset {
    const char* name;
    const int16_t* samples;
    size_t sampleCount;
};

/**
 * @brief Audio playback service
 *
 * Manages audio playback in a dedicated FreeRTOS task.
 * Thread-safe request methods allow calling from any task.
 *
 * @code
 * AudioService audio(driver, features);
 * audio.start();
 * audio.playAsset("beep");  // Non-blocking
 * audio.playTone(440, 200); // 440Hz for 200ms
 * @endcode
 */
class AudioService {
public:
    /**
     * @brief Construct audio service
     *
     * @param driver Audio driver reference (must outlive service)
     * @param features Feature manager for runtime enable/disable
     */
    AudioService(IAudioDriver& driver, config::FeatureManager& features)
        : driver_(driver),
          features_(features),
          taskHandle_(nullptr),
          requestQueue_(nullptr),
          running_(false),
          state_(State::kIdle) {}

    ~AudioService() { stop(); }

    // Non-copyable
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    /**
     * @brief Start the audio service task
     * @return ESP_OK on success
     */
    esp_err_t start() {
        if (running_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Create request queue
        requestQueue_ = xQueueCreate(kQueueDepth, sizeof(PlayRequest));
        if (!requestQueue_) {
            ESP_LOGE(kTag, "Failed to create request queue");
            return ESP_ERR_NO_MEM;
        }

        running_ = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            taskEntry, "audio_svc", kStackSize, this, 5, &taskHandle_, 1  // Core 1
        );

        if (ret != pdPASS) {
            running_ = false;
            vQueueDelete(requestQueue_);
            requestQueue_ = nullptr;
            ESP_LOGE(kTag, "Failed to create audio task");
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI(kTag, "Audio service started");
        return ESP_OK;
    }

    /**
     * @brief Stop the audio service
     */
    void stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        // Wait for task to finish
        if (taskHandle_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelete(taskHandle_);
            taskHandle_ = nullptr;
        }

        if (requestQueue_) {
            vQueueDelete(requestQueue_);
            requestQueue_ = nullptr;
        }

        driver_.stop();
        ESP_LOGI(kTag, "Audio service stopped");
    }

    /**
     * @brief Play an audio asset by name (thread-safe)
     *
     * Queues the asset for playback. Returns immediately.
     *
     * @param name Asset name to play
     * @return true if queued successfully
     */
    bool playAsset(const char* name) {
        if (!running_ || !requestQueue_) {
            return false;
        }

        PlayRequest req = {};
        req.type = RequestType::kAsset;
        strncpy(req.assetName, name, sizeof(req.assetName) - 1);

        if (xQueueSend(requestQueue_, &req, 0) != pdTRUE) {
            ESP_LOGW(kTag, "Audio queue full, dropping request");
            return false;
        }

        return true;
    }

    /**
     * @brief Play a generated tone (thread-safe)
     *
     * Queues the tone for playback. Returns immediately.
     *
     * @param frequencyHz Tone frequency in Hz
     * @param durationMs Duration in milliseconds
     * @return true if queued successfully
     */
    bool playTone(uint16_t frequencyHz, uint16_t durationMs) {
        if (!running_ || !requestQueue_) {
            return false;
        }

        PlayRequest req = {};
        req.type = RequestType::kTone;
        req.toneFrequency = frequencyHz;
        req.toneDuration = durationMs;

        if (xQueueSend(requestQueue_, &req, 0) != pdTRUE) {
            ESP_LOGW(kTag, "Audio queue full, dropping request");
            return false;
        }

        return true;
    }

    /**
     * @brief Request playback stop (thread-safe)
     */
    void stopPlayback() {
        stopRequested_ = true;
    }

    /**
     * @brief Set playback volume (thread-safe)
     * @param volume Volume 0-100
     */
    void setVolume(uint8_t volume) {
        driver_.setVolume(volume);
    }

    /**
     * @brief Get current volume
     * @return Volume 0-100
     */
    uint8_t getVolume() const {
        return driver_.getVolume();
    }

    /**
     * @brief Check if audio is currently playing
     */
    bool isPlaying() const {
        return state_.load() == State::kPlaying;
    }

private:
    static constexpr const char* kTag = "audio_svc";
    static constexpr size_t kStackSize = 4096;
    static constexpr size_t kQueueDepth = 4;
    static constexpr uint32_t kSampleRate = 16000;
    static constexpr size_t kMaxToneSamples = 16000;  // 1 second max

    enum class State : uint8_t {
        kIdle,
        kPlaying,
        kError
    };

    enum class RequestType : uint8_t {
        kNone,
        kAsset,
        kTone
    };

    struct PlayRequest {
        RequestType type;
        char assetName[32];
        uint16_t toneFrequency;
        uint16_t toneDuration;
    };

    static void taskEntry(void* arg) {
        static_cast<AudioService*>(arg)->taskLoop();
    }

    void taskLoop() {
        ESP_LOGI(kTag, "Audio task starting");

        // Allocate tone buffer (heap, not stack)
        int16_t* toneBuffer = static_cast<int16_t*>(
            heap_caps_malloc(kMaxToneSamples * sizeof(int16_t), MALLOC_CAP_INTERNAL)
        );
        if (!toneBuffer) {
            ESP_LOGE(kTag, "Failed to allocate tone buffer");
            running_ = false;
            return;
        }

        while (running_) {
            // Check if audio feature is enabled
            if (!features_.isEnabled(config::Feature::kAudio)) {
                // Drain queue without playing
                PlayRequest req;
                while (xQueueReceive(requestQueue_, &req, 0) == pdTRUE) {
                    // Discard
                }
                driver_.stop();
                state_ = State::kIdle;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // Wait for request
            PlayRequest req;
            if (xQueueReceive(requestQueue_, &req, pdMS_TO_TICKS(50)) == pdTRUE) {
                processRequest(req, toneBuffer);
            }

            // Handle stop request
            if (stopRequested_.load()) {
                driver_.stop();
                state_ = State::kIdle;
                stopRequested_ = false;
            }
        }

        heap_caps_free(toneBuffer);
        ESP_LOGI(kTag, "Audio task exiting");
    }

    void processRequest(const PlayRequest& req, int16_t* toneBuffer) {
        switch (req.type) {
            case RequestType::kAsset:
                playAssetInternal(req.assetName);
                break;
            case RequestType::kTone:
                playToneInternal(req.toneFrequency, req.toneDuration, toneBuffer);
                break;
            default:
                break;
        }
    }

    void playAssetInternal(const char* name) {
        TRACE_SCOPE(TRACE_ID("Audio.PlayAsset"), trace::Category::kAudio);

        const AudioAsset* asset = lookupAsset(name);
        if (!asset) {
            ESP_LOGW(kTag, "Asset not found: %s", name);
            return;
        }

        ESP_LOGI(kTag, "Playing asset: %s (%zu samples)", name, asset->sampleCount);
        state_ = State::kPlaying;

        esp_err_t err = driver_.start();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to start driver: %s", esp_err_to_name(err));
            state_ = State::kError;
            return;
        }

        size_t written = 0;
        err = driver_.write(asset->samples, asset->sampleCount, &written);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Write failed: %s", esp_err_to_name(err));
        }

        // Small delay to let DMA finish
        vTaskDelay(pdMS_TO_TICKS(50));

        driver_.stop();
        state_ = State::kIdle;
    }

    void playToneInternal(uint16_t frequencyHz, uint16_t durationMs, int16_t* buffer) {
        TRACE_SCOPE(TRACE_ID("Audio.PlayTone"), trace::Category::kAudio);

        // Generate sine wave
        size_t sampleCount = (static_cast<size_t>(kSampleRate) * durationMs) / 1000;
        if (sampleCount > kMaxToneSamples) {
            sampleCount = kMaxToneSamples;
        }

        ESP_LOGI(kTag, "Playing tone: %uHz for %ums (%zu samples)",
                 frequencyHz, durationMs, sampleCount);

        generateSineWave(buffer, sampleCount, frequencyHz);

        state_ = State::kPlaying;

        esp_err_t err = driver_.start();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to start driver: %s", esp_err_to_name(err));
            state_ = State::kError;
            return;
        }

        size_t written = 0;
        err = driver_.write(buffer, sampleCount, &written);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Write failed: %s", esp_err_to_name(err));
        }

        // Small delay to let DMA finish
        vTaskDelay(pdMS_TO_TICKS(50));

        driver_.stop();
        state_ = State::kIdle;
    }

    void generateSineWave(int16_t* buffer, size_t sampleCount, uint16_t frequencyHz) {
        constexpr float kTwoPi = 2.0f * 3.14159265358979f;
        constexpr int16_t kAmplitude = 24000;  // ~75% of max to avoid clipping

        float phaseIncrement = kTwoPi * frequencyHz / kSampleRate;
        float phase = 0.0f;

        for (size_t i = 0; i < sampleCount; ++i) {
            buffer[i] = static_cast<int16_t>(kAmplitude * sinf(phase));
            phase += phaseIncrement;
            if (phase >= kTwoPi) {
                phase -= kTwoPi;
            }
        }

        // Apply simple fade-in/fade-out to avoid clicks (10ms each)
        size_t fadeSamples = (kSampleRate * 10) / 1000;  // 10ms
        if (fadeSamples * 2 < sampleCount) {
            // Fade in
            for (size_t i = 0; i < fadeSamples; ++i) {
                float gain = static_cast<float>(i) / fadeSamples;
                buffer[i] = static_cast<int16_t>(buffer[i] * gain);
            }
            // Fade out
            for (size_t i = 0; i < fadeSamples; ++i) {
                float gain = static_cast<float>(fadeSamples - i) / fadeSamples;
                buffer[sampleCount - 1 - i] = static_cast<int16_t>(
                    buffer[sampleCount - 1 - i] * gain
                );
            }
        }
    }

    /**
     * @brief Look up audio asset by name
     *
     * Currently uses a hardcoded asset table. Will be replaced
     * with generated asset registry.
     */
    const AudioAsset* lookupAsset(const char* name) {
        // Hardcoded beep for initial bringup
        // 330Hz (E4) for 200ms - a low beep
        static const int16_t kBeepSamples[] = {
            #include "audio/beep_data.inc"
        };
        static constexpr size_t kBeepSampleCount = sizeof(kBeepSamples) / sizeof(kBeepSamples[0]);

        static const AudioAsset kAssets[] = {
            {"beep", kBeepSamples, kBeepSampleCount},
        };

        for (const auto& asset : kAssets) {
            if (strcmp(asset.name, name) == 0) {
                return &asset;
            }
        }
        return nullptr;
    }

    IAudioDriver& driver_;
    config::FeatureManager& features_;
    TaskHandle_t taskHandle_;
    QueueHandle_t requestQueue_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<State> state_;
};

}  // namespace domes
