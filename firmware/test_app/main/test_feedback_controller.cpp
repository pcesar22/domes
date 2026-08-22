#include "config/featureManager.hpp"
#include "services/feedbackController.hpp"
#include "sim/simConfigStorage.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

class FailingStorage final : public sim::SimConfigStorage {
public:
    esp_err_t commit() override { return ESP_FAIL; }
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

TEST(FeedbackController, SerializesConcurrentPlaybackVolumeUpdates) {
    Fixture fixture;
    std::vector<std::thread> writers;
    for (uint8_t value = 0; value <= 100; value += 10) {
        writers.emplace_back([&fixture, value] {
            uint8_t applied = 0;
            EXPECT_EQ(fixture.controller.setVolume(value, applied),
                      domes::FeedbackController::Result::kOk);
            EXPECT_LE(applied, 100);
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }
    uint8_t observed = 0;
    EXPECT_EQ(fixture.controller.getVolume(observed), domes::FeedbackController::Result::kOk);
    EXPECT_LE(observed, 100);
}

}  // namespace
