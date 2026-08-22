#pragma once

#include "config/configProtocol.hpp"
#include "config/featureManager.hpp"
#include "infra/configKeys.hpp"
#include "interfaces/iConfigStorage.hpp"
#include "utils/mutex.hpp"

#include <cstdint>
#include <functional>
#include <utility>

namespace domes {

/** Owns the bounded host-facing feedback interface and persisted audio gain. */
class FeedbackController {
public:
    static constexpr uint8_t kDefaultVolume = 80;

    enum class Result : uint8_t {
        kOk,
        kInvalid,
        kDisabled,
        kUnavailable,
        kRejected,
        kStorageError,
    };

    struct ProbeResult {
        Result result;
        bool accepted;
    };

    using ApplyVolume = std::function<void(uint8_t)>;
    using ReadVolume = std::function<uint8_t()>;
    using QueueBeep = std::function<bool()>;
    using TriggerHaptic = std::function<esp_err_t()>;

    FeedbackController(config::FeatureManager& features, IConfigStorage& storage,
                       ApplyVolume applyVolume, ReadVolume readVolume, QueueBeep queueBeep,
                       TriggerHaptic triggerHaptic)
        : features_(features),
          storage_(storage),
          applyVolume_(std::move(applyVolume)),
          readVolume_(std::move(readVolume)),
          queueBeep_(std::move(queueBeep)),
          triggerHaptic_(std::move(triggerHaptic)) {}

    /** Restore a valid stored gain, or the documented default when no value exists. */
    Result initialize() {
        utils::MutexGuard guard(mutex_);
        uint8_t volume = kDefaultVolume;
        const esp_err_t openResult = storage_.open(infra::nvs_ns::kConfig);
        if (openResult != ESP_OK) {
            return Result::kStorageError;
        }
        const esp_err_t readResult = storage_.getU8(infra::config_key::kVolume, volume);
        storage_.close();
        if (readResult != ESP_OK && readResult != ESP_ERR_NVS_NOT_FOUND) {
            return Result::kStorageError;
        }
        if (volume > 100) {
            volume = kDefaultVolume;
        }
        applyVolume_(volume);
        return Result::kOk;
    }

    Result getVolume(uint8_t& volume) {
        utils::MutexGuard guard(mutex_);
        if (!features_.isEnabled(config::Feature::kAudio)) {
            return Result::kDisabled;
        }
        volume = readVolume_();
        return volume <= 100 ? Result::kOk : Result::kUnavailable;
    }

    Result setVolume(uint32_t requested, uint8_t& applied) {
        if (requested > 100) {
            return Result::kInvalid;
        }
        utils::MutexGuard guard(mutex_);
        if (!features_.isEnabled(config::Feature::kAudio)) {
            return Result::kDisabled;
        }
        if (storage_.open(infra::nvs_ns::kConfig) != ESP_OK) {
            return Result::kStorageError;
        }
        const uint8_t volume = static_cast<uint8_t>(requested);
        const esp_err_t writeResult = storage_.setU8(infra::config_key::kVolume, volume);
        const esp_err_t commitResult = writeResult == ESP_OK ? storage_.commit() : writeResult;
        storage_.close();
        if (writeResult != ESP_OK || commitResult != ESP_OK) {
            return Result::kStorageError;
        }
        applyVolume_(volume);
        applied = readVolume_();
        return applied == volume ? Result::kOk : Result::kUnavailable;
    }

    ProbeResult trigger(config::FeedbackProbe probe) {
        utils::MutexGuard guard(mutex_);
        switch (probe) {
            case config::FeedbackProbe::kEmbeddedBeep:
                if (!features_.isEnabled(config::Feature::kAudio)) {
                    return {Result::kDisabled, false};
                }
                if (!queueBeep_) {
                    return {Result::kUnavailable, false};
                }
                return queueBeep_() ? ProbeResult{Result::kOk, true}
                                    : ProbeResult{Result::kRejected, false};
            case config::FeedbackProbe::kFixedHaptic:
                if (!features_.isEnabled(config::Feature::kHaptic)) {
                    return {Result::kDisabled, false};
                }
                if (!triggerHaptic_) {
                    return {Result::kUnavailable, false};
                }
                return triggerHaptic_() == ESP_OK ? ProbeResult{Result::kOk, true}
                                                  : ProbeResult{Result::kRejected, false};
            default:
                return {Result::kInvalid, false};
        }
    }

private:
    config::FeatureManager& features_;
    IConfigStorage& storage_;
    ApplyVolume applyVolume_;
    ReadVolume readVolume_;
    QueueBeep queueBeep_;
    TriggerHaptic triggerHaptic_;
    utils::Mutex mutex_;
};

}  // namespace domes
