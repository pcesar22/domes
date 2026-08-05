#pragma once

#include "interfaces/iAudioDriver.hpp"
#include "interfaces/iHapticDriver.hpp"
#include "interfaces/iImuDriver.hpp"
#include "interfaces/iLedDriver.hpp"
#include "interfaces/iTouchDriver.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace domes::platform {

enum class QemuAdapterBit : uint32_t {
    kLed = 1U << 0,
    kImu = 1U << 1,
    kHaptic = 1U << 2,
    kAudio = 1U << 3,
    kTouch = 1U << 4,
};

inline constexpr uint32_t kAllQemuAdapterBits = 0x1fU;

class QemuAdapterEvidence {
public:
    void markInitialized(QemuAdapterBit bit);
    void markProgress(QemuAdapterBit bit);

    uint32_t initMask() const { return initMask_.load(std::memory_order_acquire); }
    uint32_t progressMask() const { return progressMask_.load(std::memory_order_acquire); }
    uint32_t coreProgress(uint8_t core) const;

private:
    std::atomic<uint32_t> initMask_{0};
    std::atomic<uint32_t> progressMask_{0};
    std::array<std::atomic<uint32_t>, 2> coreProgress_{};
};

class QemuLedDriver final : public ILedDriver {
public:
    explicit QemuLedDriver(QemuAdapterEvidence& evidence) : evidence_(evidence) {}

    esp_err_t init() override;
    esp_err_t setPixel(uint8_t index, Color color) override;
    esp_err_t setAll(Color color) override;
    esp_err_t clear() override;
    esp_err_t refresh() override;
    void setBrightness(uint8_t brightness) override { brightness_ = brightness; }
    uint8_t getLedCount() const override { return static_cast<uint8_t>(pixels_.size()); }

private:
    QemuAdapterEvidence& evidence_;
    std::array<Color, 16> pixels_{};
    uint8_t brightness_ = 128;
    bool initialized_ = false;
};

class QemuImuDriver final : public IImuDriver {
public:
    explicit QemuImuDriver(QemuAdapterEvidence& evidence) : evidence_(evidence) {}

    esp_err_t init() override;
    esp_err_t enableTapDetection(bool singleTap, bool doubleTap) override;
    esp_err_t readAccel(AccelData& data) override;
    bool isTapDetected() override { return false; }
    esp_err_t clearInterrupt() override;

    void armSingleTap() { tapArmed_.store(true, std::memory_order_release); }

private:
    QemuAdapterEvidence& evidence_;
    std::atomic<bool> tapArmed_{false};
    bool initialized_ = false;
};

class QemuHapticDriver final : public IHapticDriver {
public:
    explicit QemuHapticDriver(QemuAdapterEvidence& evidence) : evidence_(evidence) {}

    esp_err_t init() override;
    esp_err_t playEffect(uint8_t effectId) override;
    esp_err_t playSequence(const uint8_t* effectIds, size_t count) override;
    esp_err_t stop() override;
    void setIntensity(uint8_t intensity) override {
        intensity_ = intensity > 100 ? 100 : intensity;
    }
    uint8_t getIntensity() const override { return intensity_; }
    bool isInitialized() const override { return initialized_; }
    bool isPlaying() const override { return playing_; }

private:
    QemuAdapterEvidence& evidence_;
    uint8_t intensity_ = 100;
    uint8_t lastEffect_ = 0;
    bool initialized_ = false;
    bool playing_ = false;
};

class QemuAudioDriver final : public IAudioDriver {
public:
    explicit QemuAudioDriver(QemuAdapterEvidence& evidence) : evidence_(evidence) {}

    esp_err_t init() override;
    esp_err_t start() override;
    esp_err_t stop() override;
    esp_err_t write(const int16_t* samples, size_t count, size_t* written,
                    uint32_t timeoutMs) override;
    void setVolume(uint8_t volume) override { volume_ = volume > 100 ? 100 : volume; }
    uint8_t getVolume() const override { return volume_; }
    bool isInitialized() const override { return initialized_; }
    bool isStarted() const override { return started_; }
    uint32_t sampleCount() const { return sampleCount_.load(std::memory_order_acquire); }
    uint32_t sampleHash() const { return sampleHash_.load(std::memory_order_acquire); }

private:
    QemuAdapterEvidence& evidence_;
    std::atomic<uint32_t> sampleCount_{0};
    std::atomic<uint32_t> sampleHash_{2166136261U};
    uint8_t volume_ = 50;
    bool initialized_ = false;
    bool started_ = false;
};

class QemuTouchDriver final : public ITouchDriver {
public:
    explicit QemuTouchDriver(QemuAdapterEvidence& evidence) : evidence_(evidence) {}

    esp_err_t init() override;
    esp_err_t update() override;
    bool isTouched(uint8_t padIndex) const override;
    TouchPadState getPadState(uint8_t padIndex) const override;
    uint8_t getPadCount() const override { return 4; }
    esp_err_t calibrate() override;

    void setTouched(uint8_t padIndex, bool touched);

private:
    QemuAdapterEvidence& evidence_;
    std::atomic<uint8_t> touchedMask_{0};
    bool initialized_ = false;
};

}  // namespace domes::platform
