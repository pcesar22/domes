#include "featureManager.hpp"

#include <utility>

namespace domes::config {

FeatureManager::FeatureManager(uint32_t supportedMask)
    : enabledMask_(supportedMask & kAllFeaturesMask),
      supportedMask_(supportedMask & kAllFeaturesMask) {}

bool FeatureManager::isSupported(Feature feature) const {
    if (!isValidFeature(feature)) {
        return false;
    }
    return (supportedMask_ & (1U << featureToBit(feature))) != 0;
}

void FeatureManager::onChange(FeatureChangeCallback callback) {
    utils::MutexGuard guard(mutationMutex_);
    changeCallback_ = std::move(callback);
}

bool FeatureManager::isEnabled(Feature feature) const {
    if (!isSupported(feature)) {
        return false;
    }
    const uint32_t mask = enabledMask_.load(std::memory_order_acquire);
    return (mask & (1U << featureToBit(feature))) != 0;
}

bool FeatureManager::setEnabled(Feature feature, bool enabled) {
    return setEnabled(feature, enabled, {});
}

bool FeatureManager::setEnabled(Feature feature, bool enabled,
                                BeforeRuntimeApplyCallback beforeRuntimeApply) {
    if (!isSupported(feature)) {
        return false;
    }

    utils::MutexGuard guard(mutationMutex_);
    const uint32_t bit = 1U << featureToBit(feature);
    const uint32_t oldMask = enabledMask_.load(std::memory_order_relaxed);
    const uint32_t newMask = enabled ? oldMask | bit : oldMask & ~bit;
    enabledMask_.store(newMask, std::memory_order_release);
    if (beforeRuntimeApply) {
        beforeRuntimeApply();
    }
    if (((oldMask & bit) != 0) != enabled && changeCallback_) {
        changeCallback_(feature, enabled);
    }
    return true;
}

size_t FeatureManager::getAll(domes_config_FeatureState* states) const {
    const uint32_t mask = enabledMask_.load(std::memory_order_acquire);
    size_t count = 0;

    // Iterate over all valid features (skip kUnknown and kCount)
    for (uint8_t i = 1; i < static_cast<uint8_t>(Feature::kCount); ++i) {
        const auto feature = static_cast<Feature>(i);
        if (!isSupported(feature)) {
            continue;
        }
        states[count].feature = static_cast<domes_config_Feature>(i);
        states[count].enabled = ((mask & (1U << i)) != 0);
        ++count;
    }

    return count;
}

uint32_t FeatureManager::getMask() const {
    return enabledMask_.load(std::memory_order_acquire);
}

void FeatureManager::setMask(uint32_t mask) {
    utils::MutexGuard guard(mutationMutex_);
    const uint32_t newMask = mask & supportedMask_;
    const uint32_t oldMask = enabledMask_.load(std::memory_order_relaxed);
    enabledMask_.store(newMask, std::memory_order_release);
    const uint32_t changed = oldMask ^ newMask;
    if (!changeCallback_ || changed == 0) {
        return;
    }

    for (uint8_t i = 1; i < static_cast<uint8_t>(Feature::kCount); ++i) {
        const uint32_t bit = 1U << i;
        const auto feature = static_cast<Feature>(i);
        if ((changed & bit) != 0 && isSupported(feature)) {
            changeCallback_(feature, (newMask & bit) != 0);
        }
    }
}

uint8_t FeatureManager::featureToBit(Feature feature) {
    return static_cast<uint8_t>(feature);
}

bool FeatureManager::isValidFeature(Feature feature) {
    const auto id = static_cast<uint8_t>(feature);
    return id > 0 && id < static_cast<uint8_t>(Feature::kCount);
}

}  // namespace domes::config
