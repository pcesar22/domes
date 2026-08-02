#pragma once

#include <cstddef>
#include <cstdint>

namespace domes::trace::detail {

enum class SendAllResult : uint8_t {
    kOk,
    kClosed,
    kError,
};

/**
 * @brief Send an entire buffer through a socket-like writer.
 *
 * The writer receives the unsent portion and returns the number of bytes written,
 * zero for a closed connection, or a negative value for an error.
 */
template <typename Send>
SendAllResult sendAll(const uint8_t* data, size_t len, Send&& send) {
    if (data == nullptr && len != 0) {
        return SendAllResult::kError;
    }

    size_t offset = 0;
    while (offset < len) {
        auto sent = send(data + offset, len - offset);
        if (sent < 0) {
            return SendAllResult::kError;
        }
        if (sent == 0) {
            return SendAllResult::kClosed;
        }

        const size_t sentSize = static_cast<size_t>(sent);
        if (sentSize > len - offset) {
            return SendAllResult::kError;
        }
        offset += sentSize;
    }

    return SendAllResult::kOk;
}

}  // namespace domes::trace::detail
