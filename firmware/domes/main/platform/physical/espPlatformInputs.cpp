#include "espPlatformInputs.hpp"

#include "esp_random.h"
#include "esp_wifi.h"

namespace domes::platform {

esp_err_t EspPlatformIdentity::read(PlatformIdentity& identity) const {
    return esp_wifi_get_mac(WIFI_IF_STA, identity.data());
}

esp_err_t EspRandomSource::nextU32(uint32_t& value) {
    value = esp_random();
    return ESP_OK;
}

}  // namespace domes::platform
