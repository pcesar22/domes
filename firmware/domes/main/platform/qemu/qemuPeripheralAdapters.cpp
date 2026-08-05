#include "qemuPeripheralAdapters.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>

namespace domes::platform {

namespace {
constexpr uint32_t bit(QemuAdapterBit value) {
    return static_cast<uint32_t>(value);
}
}  // namespace

void QemuAdapterEvidence::markInitialized(QemuAdapterBit value) {
    initMask_.fetch_or(bit(value), std::memory_order_acq_rel);
}

void QemuAdapterEvidence::markProgress(QemuAdapterBit value) {
    progressMask_.fetch_or(bit(value), std::memory_order_acq_rel);
    const BaseType_t core = xPortGetCoreID();
    if (core >= 0 && core < static_cast<BaseType_t>(coreProgress_.size())) {
        coreProgress_[core].fetch_add(1, std::memory_order_relaxed);
    }
}

uint32_t QemuAdapterEvidence::coreProgress(uint8_t core) const {
    return core < coreProgress_.size() ? coreProgress_[core].load(std::memory_order_acquire) : 0;
}

esp_err_t QemuLedDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    initialized_ = true;
    evidence_.markInitialized(QemuAdapterBit::kLed);
    return ESP_OK;
}

esp_err_t QemuLedDriver::setPixel(uint8_t index, Color color) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (index >= pixels_.size()) {
        return ESP_ERR_INVALID_ARG;
    }
    pixels_[index] = color;
    return ESP_OK;
}

esp_err_t QemuLedDriver::setAll(Color color) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    pixels_.fill(color);
    return ESP_OK;
}

esp_err_t QemuLedDriver::clear() {
    return setAll(Color::off());
}

esp_err_t QemuLedDriver::refresh() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    evidence_.markProgress(QemuAdapterBit::kLed);
    return ESP_OK;
}

esp_err_t QemuImuDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    initialized_ = true;
    evidence_.markInitialized(QemuAdapterBit::kImu);
    return ESP_OK;
}

esp_err_t QemuImuDriver::enableTapDetection(bool, bool) {
    return initialized_ ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t QemuImuDriver::readAccel(AccelData& data) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    data = {.x = 0.0F, .y = 0.0F, .z = tapArmed_.exchange(false) ? 1.5F : 1.0F};
    evidence_.markProgress(QemuAdapterBit::kImu);
    return ESP_OK;
}

esp_err_t QemuImuDriver::clearInterrupt() {
    return initialized_ ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t QemuHapticDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    initialized_ = true;
    evidence_.markInitialized(QemuAdapterBit::kHaptic);
    return ESP_OK;
}

esp_err_t QemuHapticDriver::playEffect(uint8_t effectId) {
    if (!initialized_ || effectId == 0 || effectId > 123) {
        return initialized_ ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }
    lastEffect_ = effectId;
    playing_ = true;
    evidence_.markProgress(QemuAdapterBit::kHaptic);
    return ESP_OK;
}

esp_err_t QemuHapticDriver::playSequence(const uint8_t* effectIds, size_t count) {
    if (!effectIds || count == 0 || count > 8) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t index = 0; index < count; ++index) {
        const esp_err_t err = playEffect(effectIds[index]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t QemuHapticDriver::stop() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    playing_ = false;
    return ESP_OK;
}

esp_err_t QemuAudioDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    initialized_ = true;
    evidence_.markInitialized(QemuAdapterBit::kAudio);
    return ESP_OK;
}

esp_err_t QemuAudioDriver::start() {
    if (!initialized_ || started_) {
        return ESP_ERR_INVALID_STATE;
    }
    started_ = true;
    return ESP_OK;
}

esp_err_t QemuAudioDriver::stop() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    started_ = false;
    return ESP_OK;
}

esp_err_t QemuAudioDriver::write(const int16_t* samples, size_t count, size_t* written, uint32_t) {
    if (!initialized_ || !started_) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((!samples && count != 0) || !written) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t hash = sampleHash_.load(std::memory_order_relaxed);
    for (size_t index = 0; index < count; ++index) {
        const uint16_t sample = static_cast<uint16_t>(samples[index]);
        hash = (hash ^ static_cast<uint8_t>(sample & 0xffU)) * 16777619U;
        hash = (hash ^ static_cast<uint8_t>(sample >> 8U)) * 16777619U;
    }
    sampleHash_.store(hash, std::memory_order_release);
    sampleCount_.fetch_add(static_cast<uint32_t>(count), std::memory_order_acq_rel);
    *written = count;
    evidence_.markProgress(QemuAdapterBit::kAudio);
    return ESP_OK;
}

esp_err_t QemuTouchDriver::init() {
    if (initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    initialized_ = true;
    evidence_.markInitialized(QemuAdapterBit::kTouch);
    return ESP_OK;
}

esp_err_t QemuTouchDriver::update() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    evidence_.markProgress(QemuAdapterBit::kTouch);
    return ESP_OK;
}

bool QemuTouchDriver::isTouched(uint8_t padIndex) const {
    return initialized_ && padIndex < getPadCount() &&
           (touchedMask_.load(std::memory_order_acquire) & (1U << padIndex)) != 0;
}

TouchPadState QemuTouchDriver::getPadState(uint8_t padIndex) const {
    if (padIndex >= getPadCount()) {
        return {};
    }
    const bool touched = isTouched(padIndex);
    return {.touched = touched, .rawValue = touched ? 400U : 1000U, .threshold = 700U};
}

esp_err_t QemuTouchDriver::calibrate() {
    return initialized_ ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void QemuTouchDriver::setTouched(uint8_t padIndex, bool touched) {
    if (padIndex >= getPadCount()) {
        return;
    }
    const uint8_t mask = static_cast<uint8_t>(1U << padIndex);
    if (touched) {
        touchedMask_.fetch_or(mask, std::memory_order_acq_rel);
    } else {
        touchedMask_.fetch_and(static_cast<uint8_t>(~mask), std::memory_order_acq_rel);
    }
}

}  // namespace domes::platform
