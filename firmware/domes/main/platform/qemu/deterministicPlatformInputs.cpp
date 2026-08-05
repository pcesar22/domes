#include "deterministicPlatformInputs.hpp"

namespace domes::platform {

esp_err_t FixedPlatformIdentity::read(PlatformIdentity& identity) const {
    identity = identity_;
    return ESP_OK;
}

esp_err_t RecordedRandomSource::nextU32(uint32_t& value) {
    if (index_ >= values_.size()) {
        return ESP_ERR_INVALID_STATE;
    }
    value = values_[index_++];
    return ESP_OK;
}

}  // namespace domes::platform
