/**
 * @file pins.hpp
 * @brief Platform-specific pin configuration router
 *
 * This header includes the appropriate pin definitions based on
 * the selected platform in Kconfig (CONFIG_DOMES_PLATFORM_*).
 */

#pragma once

#if defined(CONFIG_DOMES_PLATFORM_DEVKIT)
    #include "pinsDevkit.hpp"
#elif defined(CONFIG_DOMES_PLATFORM_PCB_V1)
    #include "pinsPcbV1.hpp"
#else
    // Default to DevKit if no platform specified
    #include "pinsDevkit.hpp"
#endif
