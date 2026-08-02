/**
 * @file otaProtocol.cpp
 * @brief OTA protocol message serialization/deserialization
 */

#include "otaProtocol.hpp"

#include <cstring>

namespace domes {

// =============================================================================
// Serialization
// =============================================================================

TransportError serializeOtaBegin(uint32_t firmwareSize, const uint8_t* sha256, const char* version,
                                 uint8_t* buf, size_t bufSize, size_t* outLen) {
    constexpr size_t payloadSize = sizeof(OtaBeginPayload);

    if (sha256 == nullptr || version == nullptr || version[0] == '\0' || buf == nullptr ||
        outLen == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (bufSize < payloadSize) {
        return TransportError::kInvalidArg;
    }

    auto* payload = reinterpret_cast<OtaBeginPayload*>(buf);

    // Firmware size (little-endian)
    payload->firmwareSize = firmwareSize;

    // SHA256
    std::memcpy(payload->sha256.data(), sha256, kSha256Size);

    // Version string
    payload->version.fill('\0');
    const size_t versionLen = std::strlen(version);
    if (versionLen >= kOtaVersionMaxLen) {
        return TransportError::kInvalidArg;
    }
    std::memcpy(payload->version.data(), version, versionLen);

    *outLen = payloadSize;
    return TransportError::kOk;
}

TransportError serializeOtaData(uint32_t offset, const uint8_t* data, size_t dataLen, uint8_t* buf,
                                size_t bufSize, size_t* outLen) {
    const size_t totalSize = sizeof(OtaDataHeader) + dataLen;

    if (buf == nullptr || outLen == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (dataLen == 0 || data == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (bufSize < totalSize) {
        return TransportError::kInvalidArg;
    }
    if (dataLen > kOtaChunkSize) {
        return TransportError::kInvalidArg;
    }

    auto* header = reinterpret_cast<OtaDataHeader*>(buf);
    header->offset = offset;
    header->length = static_cast<uint16_t>(dataLen);

    if (dataLen > 0) {
        std::memcpy(buf + sizeof(OtaDataHeader), data, dataLen);
    }

    *outLen = totalSize;
    return TransportError::kOk;
}

TransportError serializeOtaEnd(uint8_t* buf, size_t bufSize, size_t* outLen) {
    if (outLen == nullptr) {
        return TransportError::kInvalidArg;
    }

    // OTA_END has no payload
    (void)buf;
    (void)bufSize;
    *outLen = 0;
    return TransportError::kOk;
}

TransportError serializeOtaAck(OtaStatus status, uint32_t nextOffset, uint8_t* buf, size_t bufSize,
                               size_t* outLen) {
    constexpr size_t payloadSize = sizeof(OtaAckPayload);

    if (buf == nullptr || outLen == nullptr || !isValidOtaStatus(status)) {
        return TransportError::kInvalidArg;
    }
    if (bufSize < payloadSize) {
        return TransportError::kInvalidArg;
    }

    auto* payload = reinterpret_cast<OtaAckPayload*>(buf);
    payload->status = static_cast<uint8_t>(status);
    payload->nextOffset = nextOffset;

    *outLen = payloadSize;
    return TransportError::kOk;
}

TransportError serializeOtaAbort(OtaStatus reason, uint8_t* buf, size_t bufSize, size_t* outLen) {
    constexpr size_t payloadSize = sizeof(OtaAbortPayload);

    if (buf == nullptr || outLen == nullptr || !isValidOtaStatus(reason)) {
        return TransportError::kInvalidArg;
    }
    if (bufSize < payloadSize) {
        return TransportError::kInvalidArg;
    }

    auto* payload = reinterpret_cast<OtaAbortPayload*>(buf);
    payload->reason = static_cast<uint8_t>(reason);

    *outLen = payloadSize;
    return TransportError::kOk;
}

// =============================================================================
// Deserialization
// =============================================================================

TransportError deserializeOtaBegin(const uint8_t* payload, size_t payloadLen,
                                   uint32_t* firmwareSize, uint8_t* sha256, char* version,
                                   size_t versionBufSize) {
    constexpr size_t expectedSize = sizeof(OtaBeginPayload);

    if (payload == nullptr || firmwareSize == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (payloadLen != expectedSize) {
        return TransportError::kProtocolError;
    }

    const auto* msg = reinterpret_cast<const OtaBeginPayload*>(payload);

    *firmwareSize = msg->firmwareSize;

    if (sha256 != nullptr) {
        std::memcpy(sha256, msg->sha256.data(), kSha256Size);
    }

    const void* terminator = std::memchr(msg->version.data(), '\0', msg->version.size());
    if (terminator == nullptr) {
        return TransportError::kProtocolError;
    }
    const size_t versionLen =
        static_cast<const char*>(terminator) - static_cast<const char*>(msg->version.data());
    if (versionLen == 0) {
        return TransportError::kProtocolError;
    }

    if (version != nullptr) {
        if (versionBufSize <= versionLen) {
            return TransportError::kInvalidArg;
        }
        std::memcpy(version, msg->version.data(), versionLen);
        version[versionLen] = '\0';
    }

    return TransportError::kOk;
}

TransportError deserializeOtaData(const uint8_t* payload, size_t payloadLen, uint32_t* offset,
                                  const uint8_t** data, size_t* dataLen) {
    constexpr size_t headerSize = sizeof(OtaDataHeader);

    if (payload == nullptr || offset == nullptr || data == nullptr || dataLen == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (payloadLen < headerSize) {
        return TransportError::kProtocolError;
    }

    const auto* header = reinterpret_cast<const OtaDataHeader*>(payload);

    *offset = header->offset;
    *dataLen = header->length;

    if (header->length == 0 || header->length > kOtaChunkSize ||
        payloadLen != headerSize + header->length) {
        return TransportError::kProtocolError;
    }

    *data = payload + headerSize;
    return TransportError::kOk;
}

TransportError deserializeOtaEnd(const uint8_t*, size_t payloadLen) {
    return payloadLen == 0 ? TransportError::kOk : TransportError::kProtocolError;
}

TransportError deserializeOtaAck(const uint8_t* payload, size_t payloadLen, OtaStatus* status,
                                 uint32_t* nextOffset) {
    constexpr size_t expectedSize = sizeof(OtaAckPayload);

    if (payload == nullptr || status == nullptr || nextOffset == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (payloadLen != expectedSize) {
        return TransportError::kProtocolError;
    }

    const auto* msg = reinterpret_cast<const OtaAckPayload*>(payload);

    *status = static_cast<OtaStatus>(msg->status);
    if (!isValidOtaStatus(*status)) {
        return TransportError::kProtocolError;
    }
    *nextOffset = msg->nextOffset;

    return TransportError::kOk;
}

TransportError deserializeOtaAbort(const uint8_t* payload, size_t payloadLen, OtaStatus* reason) {
    constexpr size_t expectedSize = sizeof(OtaAbortPayload);

    if (payload == nullptr || reason == nullptr) {
        return TransportError::kInvalidArg;
    }
    if (payloadLen != expectedSize) {
        return TransportError::kProtocolError;
    }

    const auto* msg = reinterpret_cast<const OtaAbortPayload*>(payload);
    *reason = static_cast<OtaStatus>(msg->reason);
    if (!isValidOtaStatus(*reason)) {
        return TransportError::kProtocolError;
    }

    return TransportError::kOk;
}

}  // namespace domes
