#include "featureManager.hpp"

namespace domes::config {

FeatureManager::FeatureManager()
    : enabledMask_(0xFFFFFFFF)  // All features enabled by default
{
}

bool FeatureManager::isEnabled(Feature feature) const {
    if (!isValidFeature(feature)) {
        return false;
    }
    const uint32_t mask = enabledMask_.load(std::memory_order_acquire);
    return (mask & (1U << featureToBit(feature))) != 0;
}

bool FeatureManager::setEnabled(Feature feature, bool enabled) {
    if (!isValidFeature(feature)) {
        return false;
    }

    const uint32_t bit = 1U << featureToBit(feature);
    if (enabled) {
        enabledMask_.fetch_or(bit, std::memory_order_release);
    } else {
        enabledMask_.fetch_and(~bit, std::memory_order_release);
    }
    return true;
}

size_t FeatureManager::getAll(FeatureState* states) const {
    const uint32_t mask = enabledMask_.load(std::memory_order_acquire);
    size_t count = 0;

    // Iterate over all valid features (skip kUnknown and kCount)
    for (uint8_t i = 1; i < static_cast<uint8_t>(Feature::kCount); ++i) {
        states[count].feature = i;
        states[count].enabled = ((mask & (1U << i)) != 0) ? 1 : 0;
        ++count;
    }

    return count;
}

uint32_t FeatureManager::getMask() const {
    return enabledMask_.load(std::memory_order_acquire);
}

void FeatureManager::setMask(uint32_t mask) {
    enabledMask_.store(mask, std::memory_order_release);
}

uint8_t FeatureManager::featureToBit(Feature feature) {
    return static_cast<uint8_t>(feature);
}

bool FeatureManager::isValidFeature(Feature feature) {
    const auto id = static_cast<uint8_t>(feature);
    return id > 0 && id < static_cast<uint8_t>(Feature::kCount);
}

}  // namespace domes::config
