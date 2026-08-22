#include "config/featureManager.hpp"
#include "config/feedbackCommandHandler.hpp"
#include "interfaces/iAudioDriver.hpp"
#include "pb_decode.h"
#include "pb_encode.h"
#include "services/feedbackController.hpp"
#include "sim/simConfigStorage.hpp"

#include <array>
#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

class FailingStorage final : public sim::SimConfigStorage {
public:
    esp_err_t commit() override { return ESP_FAIL; }
};

class ConcurrentPlaybackDriver final : public domes::IAudioDriver {
public:
    esp_err_t init() override { return ESP_OK; }
    esp_err_t start() override { return ESP_OK; }
    esp_err_t stop() override { return ESP_OK; }

    esp_err_t write(const int16_t*, size_t count, size_t* written, uint32_t) override {
        writing = true;
        while (!release.load()) {
            const uint8_t observed = volume.load();
            if (observed > 100) {
                invalidRead = true;
            }
            readCount++;
            std::this_thread::yield();
        }
        if (written) {
            *written = count;
        }
        return ESP_OK;
    }

    void setVolume(uint8_t value) override { volume.store(value); }
    uint8_t getVolume() const override { return volume.load(); }
    bool isInitialized() const override { return true; }
    bool isStarted() const override { return writing.load(); }

    std::atomic<uint8_t> volume{17};
    std::atomic<bool> writing{false};
    std::atomic<bool> release{false};
    std::atomic<bool> invalidRead{false};
    std::atomic<size_t> readCount{0};
};

struct Fixture {
    domes::config::FeatureManager features;
    sim::SimConfigStorage storage;
    std::atomic<uint8_t> volume{17};
    bool beepAccepted = true;
    esp_err_t hapticResult = ESP_OK;
    domes::FeedbackController controller{
        features,
        storage,
        [this](uint8_t value) { volume.store(value); },
        [this] { return volume.load(); },
        [this] { return beepAccepted; },
        [this] { return hapticResult; },
    };
};

TEST(FeedbackController, RestoresDocumentedDefaultAndStoredValue) {
    Fixture fixture;
    EXPECT_EQ(fixture.controller.initialize(), domes::FeedbackController::Result::kOk);
    EXPECT_EQ(fixture.volume, domes::FeedbackController::kDefaultVolume);

    ASSERT_EQ(fixture.storage.open(domes::infra::nvs_ns::kConfig), ESP_OK);
    ASSERT_EQ(fixture.storage.setU8(domes::infra::config_key::kVolume, 42), ESP_OK);
    ASSERT_EQ(fixture.storage.commit(), ESP_OK);
    fixture.storage.close();
    fixture.volume = 0;
    EXPECT_EQ(fixture.controller.initialize(), domes::FeedbackController::Result::kOk);
    EXPECT_EQ(fixture.volume, 42);
}

TEST(FeedbackController, PersistsOnlyBoundedVolumeAndReportsStorageFailure) {
    Fixture fixture;
    uint8_t applied = 0;
    EXPECT_EQ(fixture.controller.setVolume(101, applied),
              domes::FeedbackController::Result::kInvalid);
    EXPECT_EQ(fixture.volume, 17);
    EXPECT_EQ(fixture.controller.setVolume(73, applied), domes::FeedbackController::Result::kOk);
    EXPECT_EQ(applied, 73);

    uint8_t stored = 0;
    ASSERT_EQ(fixture.storage.open(domes::infra::nvs_ns::kConfig), ESP_OK);
    EXPECT_EQ(fixture.storage.getU8(domes::infra::config_key::kVolume, stored), ESP_OK);
    fixture.storage.close();
    EXPECT_EQ(stored, 73);

    domes::config::FeatureManager features;
    FailingStorage storage;
    std::atomic<uint8_t> volume{23};
    domes::FeedbackController controller(
        features, storage, [&](uint8_t value) { volume = value; }, [&] { return volume.load(); },
        [] { return true; }, [] { return ESP_OK; });
    EXPECT_EQ(controller.setVolume(50, applied), domes::FeedbackController::Result::kStorageError);
    EXPECT_EQ(volume, 23);
}

TEST(FeedbackController, ReportsApplicationReadbackFailure) {
    domes::config::FeatureManager features;
    sim::SimConfigStorage storage;
    uint8_t unchanged = 23;
    domes::FeedbackController controller(
        features, storage, [](uint8_t) {}, [&] { return unchanged; }, [] { return true; },
        [] { return ESP_OK; });
    uint8_t applied = 0;

    EXPECT_EQ(controller.setVolume(50, applied), domes::FeedbackController::Result::kUnavailable);
    EXPECT_EQ(applied, unchanged);
}

TEST(FeedbackController, AcceptsOnlySuccessfulKnownProbes) {
    Fixture fixture;

    const auto beep = fixture.controller.trigger(domes::config::FeedbackProbe::kEmbeddedBeep);
    EXPECT_EQ(beep.result, domes::FeedbackController::Result::kOk);
    EXPECT_TRUE(beep.accepted);

    const auto haptic = fixture.controller.trigger(domes::config::FeedbackProbe::kFixedHaptic);
    EXPECT_EQ(haptic.result, domes::FeedbackController::Result::kOk);
    EXPECT_TRUE(haptic.accepted);
}

TEST(FeedbackController, RejectsDisabledFailedAndUnknownProbes) {
    Fixture fixture;
    fixture.features.setEnabled(domes::config::Feature::kAudio, false);
    EXPECT_EQ(fixture.controller.trigger(domes::config::FeedbackProbe::kEmbeddedBeep).result,
              domes::FeedbackController::Result::kDisabled);
    fixture.features.setEnabled(domes::config::Feature::kAudio, true);
    fixture.beepAccepted = false;
    EXPECT_EQ(fixture.controller.trigger(domes::config::FeedbackProbe::kEmbeddedBeep).result,
              domes::FeedbackController::Result::kRejected);

    fixture.features.setEnabled(domes::config::Feature::kHaptic, false);
    EXPECT_EQ(fixture.controller.trigger(domes::config::FeedbackProbe::kFixedHaptic).result,
              domes::FeedbackController::Result::kDisabled);
    fixture.features.setEnabled(domes::config::Feature::kHaptic, true);
    fixture.hapticResult = ESP_FAIL;
    EXPECT_EQ(fixture.controller.trigger(domes::config::FeedbackProbe::kFixedHaptic).result,
              domes::FeedbackController::Result::kRejected);
    EXPECT_EQ(fixture.controller.trigger(domes::config::FeedbackProbe::kUnknown).result,
              domes::FeedbackController::Result::kInvalid);
}

TEST(FeedbackController, PlaybackReadsRemainRaceSafeDuringVolumeUpdates) {
    domes::config::FeatureManager features;
    sim::SimConfigStorage storage;
    ConcurrentPlaybackDriver driver;
    domes::FeedbackController controller(
        features, storage, [&](uint8_t value) { driver.setVolume(value); },
        [&] { return driver.getVolume(); }, [] { return true; }, [] { return ESP_OK; });

    const std::array<int16_t, 32> samples{};
    std::thread playback([&] {
        size_t written = 0;
        EXPECT_EQ(driver.write(samples.data(), samples.size(), &written, 1000), ESP_OK);
        EXPECT_EQ(written, samples.size());
    });
    while (!driver.writing.load()) {
        std::this_thread::yield();
    }

    std::vector<std::thread> writers;
    for (uint8_t value = 0; value <= 100; value += 10) {
        writers.emplace_back([&controller, value] {
            uint8_t applied = 0;
            EXPECT_EQ(controller.setVolume(value, applied), domes::FeedbackController::Result::kOk);
            EXPECT_LE(applied, 100);
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }
    driver.release = true;
    playback.join();

    uint8_t observed = 0;
    EXPECT_EQ(controller.getVolume(observed), domes::FeedbackController::Result::kOk);
    EXPECT_LE(observed, 100);
    EXPECT_GT(driver.readCount, 0u);
    EXPECT_FALSE(driver.invalidRead);
}

TEST(FeedbackCommandHandler, DecodesRoutesAndSerializesStatusEnvelope) {
    Fixture fixture;
    domes::config::FeedbackCommandHandler handler(&fixture.controller);
    domes::config::FeedbackCommandHandler::Response response;

    domes_config_SetAudioVolumeRequest setRequest = domes_config_SetAudioVolumeRequest_init_zero;
    setRequest.volume = 64;
    std::array<uint8_t, domes_config_SetAudioVolumeRequest_size> requestBytes{};
    pb_ostream_t output = pb_ostream_from_buffer(requestBytes.data(), requestBytes.size());
    ASSERT_TRUE(pb_encode(&output, domes_config_SetAudioVolumeRequest_fields, &setRequest));
    ASSERT_TRUE(handler.handle(domes::config::MsgType::kSetAudioVolumeReq, requestBytes.data(),
                               output.bytes_written, response));
    EXPECT_EQ(response.type, domes::config::MsgType::kSetAudioVolumeRsp);
    ASSERT_GT(response.length, 1u);
    EXPECT_EQ(response.payload[0], static_cast<uint8_t>(domes::config::Status::kOk));
    domes_config_SetAudioVolumeResponse setBody = domes_config_SetAudioVolumeResponse_init_zero;
    pb_istream_t input = pb_istream_from_buffer(response.payload.data() + 1, response.length - 1);
    ASSERT_TRUE(pb_decode(&input, domes_config_SetAudioVolumeResponse_fields, &setBody));
    EXPECT_EQ(setBody.volume, 64u);

    domes_config_TriggerFeedbackRequest triggerRequest =
        domes_config_TriggerFeedbackRequest_init_zero;
    triggerRequest.probe = domes_config_FeedbackProbe_FEEDBACK_PROBE_EMBEDDED_BEEP;
    output = pb_ostream_from_buffer(requestBytes.data(), requestBytes.size());
    ASSERT_TRUE(pb_encode(&output, domes_config_TriggerFeedbackRequest_fields, &triggerRequest));
    fixture.features.setEnabled(domes::config::Feature::kAudio, false);
    ASSERT_TRUE(handler.handle(domes::config::MsgType::kTriggerFeedbackReq, requestBytes.data(),
                               output.bytes_written, response));
    EXPECT_EQ(response.type, domes::config::MsgType::kTriggerFeedbackRsp);
    EXPECT_EQ(response.payload[0], static_cast<uint8_t>(domes::config::Status::kDisabled));
    domes_config_TriggerFeedbackResponse triggerBody =
        domes_config_TriggerFeedbackResponse_init_zero;
    input = pb_istream_from_buffer(response.payload.data() + 1, response.length - 1);
    ASSERT_TRUE(pb_decode(&input, domes_config_TriggerFeedbackResponse_fields, &triggerBody));
    EXPECT_FALSE(triggerBody.accepted);
    EXPECT_EQ(triggerBody.probe, domes_config_FeedbackProbe_FEEDBACK_PROBE_EMBEDDED_BEEP);

    fixture.features.setEnabled(domes::config::Feature::kAudio, true);
    fixture.beepAccepted = false;
    ASSERT_TRUE(handler.handle(domes::config::MsgType::kTriggerFeedbackReq, requestBytes.data(),
                               output.bytes_written, response));
    EXPECT_EQ(response.payload[0], static_cast<uint8_t>(domes::config::Status::kRejected));
    input = pb_istream_from_buffer(response.payload.data() + 1, response.length - 1);
    triggerBody = domes_config_TriggerFeedbackResponse_init_zero;
    ASSERT_TRUE(pb_decode(&input, domes_config_TriggerFeedbackResponse_fields, &triggerBody));
    EXPECT_FALSE(triggerBody.accepted);
}

}  // namespace
