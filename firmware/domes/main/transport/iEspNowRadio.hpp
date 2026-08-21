#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace domes {

constexpr size_t kEspNowAddressSize = 6;
constexpr size_t kEspNowMaxPayload = 250;
using EspNowAddress = std::array<uint8_t, kEspNowAddressSize>;
using EspNowCorrelationToken = uint32_t;

enum class EspNowRadioResult : uint8_t {
    kOk,
    kAlreadyExists,
    kNotFound,
    kError,
};

enum class EspNowRadioSendStatus : uint8_t {
    kSuccess,
    kFailure,
};

struct EspNowReceiveMetadata {
    EspNowAddress source{};
    int8_t rssi = 0;
    bool sourceValid = false;
    bool rssiValid = false;
};

struct EspNowPeerCounts {
    uint8_t total = 0;
};

/** Narrow vendor-lifecycle and I/O seam owned by the DOMES project. */
class IEspNowRadio {
public:
    using ReceiveCallback = void (*)(void* context, EspNowCorrelationToken token,
                                     const EspNowReceiveMetadata& metadata, const uint8_t* data,
                                     size_t len);
    using SendCallback = void (*)(void* context, EspNowCorrelationToken token,
                                  const EspNowAddress& destination, EspNowRadioSendStatus status);

    virtual ~IEspNowRadio() = default;

    virtual EspNowRadioResult init(void* context, ReceiveCallback receiveCallback,
                                   SendCallback sendCallback) = 0;
    virtual void deinit() = 0;
    virtual EspNowRadioResult addPeer(const EspNowAddress& address) = 0;
    virtual EspNowRadioResult removePeer(const EspNowAddress& address) = 0;
    virtual EspNowRadioResult getPeerCounts(EspNowPeerCounts& counts) const = 0;
    virtual bool peerExists(const EspNowAddress& address) const = 0;
    virtual EspNowRadioResult send(const EspNowAddress& destination, const uint8_t* data,
                                   size_t len, EspNowCorrelationToken token) = 0;
};

}  // namespace domes
