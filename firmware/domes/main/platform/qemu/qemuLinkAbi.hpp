#pragma once

#include "transport/iEspNowRadio.hpp"

#include <cstddef>
#include <cstdint>

namespace domes::platform::qemu_link {

inline constexpr uintptr_t kMmioBase = 0x600D0000U;
inline constexpr size_t kMmioWindowSize = 0x1000U;
inline constexpr uint32_t kCapabilityMagic = 0x444C4E4BU;  // "DLNK" little-endian.
inline constexpr uint32_t kAbiVersion = 1U;
inline constexpr int kInterruptSource = 0;  // ESP32-S3 ETS_WIFI_MAC_INTR_SOURCE.
inline constexpr size_t kPayloadWindowSize = 0x100U;

enum class Register : uintptr_t {
    kCapability = 0x000,
    kVersion = 0x004,
    kMaxPayload = 0x008,
    kTxDestinationLow = 0x010,
    kTxDestinationHigh = 0x014,
    kTxLength = 0x018,
    kTxCorrelation = 0x01c,
    kTxSubmit = 0x020,
    kTxStatus = 0x024,
    kRxSourceLow = 0x030,
    kRxSourceHigh = 0x034,
    kRxRssi = 0x038,
    kRxLength = 0x03c,
    kRxCorrelation = 0x040,
    kRxConsume = 0x044,
    kInterruptStatus = 0x050,
    kInterruptMask = 0x054,
    kInterruptAck = 0x058,
    kStickyStatus = 0x05c,
    kTxPayload = 0x100,
    kRxPayload = 0x200,
};

enum Interrupt : uint32_t {
    kInterruptTxComplete = 1U << 0,
    kInterruptRxReady = 1U << 1,
};

enum Sticky : uint32_t {
    kStickyOverflow = 1U << 0,
    kStickyInvalidAccess = 1U << 1,
    kStickyExhausted = 1U << 2,
    kStickyModelFailure = 1U << 3,
    kStickySequence = 1U << 4,
    kStickyTruncated = 1U << 5,
    kStickyOverwrite = 1U << 6,
    kStickyUnknownVersion = 1U << 7,
};

enum class TxStatus : uint32_t {
    kIdle = 0,
    kPending = 1,
    kSuccess = 2,
    kFailure = 3,
};

constexpr uintptr_t address(Register reg) {
    return kMmioBase + static_cast<uintptr_t>(reg);
}

static_assert(kEspNowMaxPayload <= kPayloadWindowSize);
static_assert(static_cast<uintptr_t>(Register::kTxPayload) + kPayloadWindowSize <=
              static_cast<uintptr_t>(Register::kRxPayload));
static_assert(static_cast<uintptr_t>(Register::kRxPayload) + kPayloadWindowSize <= kMmioWindowSize);
static_assert(sizeof(EspNowCorrelationToken) == sizeof(uint32_t));

}  // namespace domes::platform::qemu_link
