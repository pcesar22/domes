#pragma once

#include "interfaces/iPlatformIdentity.hpp"
#include "interfaces/iRandomSource.hpp"

namespace domes::platform {

/** Uses the initialized WiFi station MAC as the physical pod identity. */
class EspPlatformIdentity final : public IPlatformIdentity {
public:
    esp_err_t read(PlatformIdentity& identity) const override;
};

/** Uses the ESP32 hardware random source. */
class EspRandomSource final : public IRandomSource {
public:
    esp_err_t nextU32(uint32_t& value) override;
};

}  // namespace domes::platform
