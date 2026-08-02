#pragma once

#include "esp_app_desc.h"

namespace domes::infra {

inline const esp_app_desc_t& appMetadata() {
    return *esp_app_get_description();
}

inline const char* firmwareVersion() {
    return appMetadata().version;
}

}  // namespace domes::infra
