#include "drivers/audioDriver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

class CapturingAudioDriver final : public domes::IAudioDriver {
public:
    esp_err_t init() override { return ESP_OK; }
    esp_err_t start() override { return ESP_OK; }
    esp_err_t stop() override { return ESP_OK; }

    esp_err_t write(const int16_t* samples, size_t count, size_t* written, uint32_t) override {
        maxWrite = std::max(maxWrite, count);
        writeCount++;
        if (writeCount == failOnWrite) {
            *written = 0;
            return failure;
        }
        const size_t accepted = partialWrite ? count - 1 : count;
        captured.insert(captured.end(), samples, samples + accepted);
        *written = accepted;
        return ESP_OK;
    }

    void setVolume(uint8_t value) override { volume = value; }
    uint8_t getVolume() const override { return volume; }
    bool isInitialized() const override { return true; }
    bool isStarted() const override { return true; }

    std::vector<int16_t> captured;
    size_t maxWrite = 0;
    size_t writeCount = 0;
    size_t failOnWrite = 0;
    esp_err_t failure = ESP_ERR_TIMEOUT;
    bool partialWrite = false;
    uint8_t volume = 80;
};

std::vector<int16_t> legacyTone(uint16_t frequencyHz, uint16_t durationMs) {
    constexpr float kTwoPi = 2.0f * 3.14159265358979f;
    constexpr int16_t kAmplitude = 24000;
    const size_t requested =
        (static_cast<size_t>(domes::AudioToneWriter::kSampleRate) * durationMs) / 1000;
    const size_t sampleCount = std::min(requested, domes::AudioToneWriter::kMaxSamples);
    std::vector<int16_t> samples(sampleCount);
    const float phaseIncrement = kTwoPi * frequencyHz / domes::AudioToneWriter::kSampleRate;
    float phase = 0.0f;
    for (auto& sample : samples) {
        sample = static_cast<int16_t>(kAmplitude * sinf(phase));
        phase += phaseIncrement;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }

    constexpr size_t kFadeSamples = (domes::AudioToneWriter::kSampleRate * 10) / 1000;
    if (kFadeSamples * 2 < sampleCount) {
        for (size_t i = 0; i < kFadeSamples; ++i) {
            const float gain = static_cast<float>(i) / kFadeSamples;
            samples[i] = static_cast<int16_t>(samples[i] * gain);
        }
        for (size_t i = 0; i < kFadeSamples; ++i) {
            const float gain = static_cast<float>(kFadeSamples - i) / kFadeSamples;
            samples[sampleCount - 1 - i] =
                static_cast<int16_t>(samples[sampleCount - 1 - i] * gain);
        }
    }
    return samples;
}

TEST(AudioToneWriterTest, StreamsOneSecondToneInBoundedChunks) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver driver;

    ASSERT_EQ(writer.write(driver, 440, 1000), ESP_OK);
    EXPECT_EQ(driver.captured.size(), domes::AudioToneWriter::kMaxSamples);
    EXPECT_LE(driver.maxWrite, domes::AudioToneWriter::kChunkSamples);
    EXPECT_EQ(driver.writeCount, 63U);
    EXPECT_EQ(driver.captured.front(), 0);
    EXPECT_NE(driver.captured[domes::AudioToneWriter::kChunkSamples], 0);
}

TEST(AudioToneWriterTest, MatchesLegacyMonolithicWaveformExactly) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver driver;

    ASSERT_EQ(writer.write(driver, 440, 1000), ESP_OK);
    EXPECT_EQ(driver.captured, legacyTone(440, 1000));
}

TEST(AudioToneWriterTest, CapsDurationWithoutChangingFrequencyContinuity) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver oneSecond;
    CapturingAudioDriver overlong;

    ASSERT_EQ(writer.write(oneSecond, 330, 1000), ESP_OK);
    ASSERT_EQ(writer.write(overlong, 330, 2000), ESP_OK);
    EXPECT_EQ(overlong.captured, oneSecond.captured);
}

TEST(AudioToneWriterTest, PreservesShortToneWithoutFadeOrPadding) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver driver;

    ASSERT_EQ(writer.write(driver, 1000, 10), ESP_OK);
    ASSERT_EQ(driver.captured.size(), 160U);
    EXPECT_EQ(driver.writeCount, 1U);
    EXPECT_EQ(driver.captured[4], 24000);
}

TEST(AudioToneWriterTest, PropagatesDriverFailureAndStopsGenerating) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver driver;
    driver.failOnWrite = 2;

    EXPECT_EQ(writer.write(driver, 440, 1000), ESP_ERR_TIMEOUT);
    EXPECT_EQ(driver.writeCount, 2U);
    EXPECT_EQ(driver.captured.size(), domes::AudioToneWriter::kChunkSamples);
}

TEST(AudioToneWriterTest, RejectsSuccessfulPartialWrite) {
    domes::AudioToneWriter writer;
    CapturingAudioDriver driver;
    driver.partialWrite = true;

    EXPECT_EQ(writer.write(driver, 440, 100), ESP_FAIL);
    EXPECT_EQ(driver.writeCount, 1U);
}

}  // namespace
