#pragma once

/**
 * @file otaProtocol.hpp
 * @brief OTA update protocol message definitions
 *
 * Defines the bidirectional OTA protocol used over serial/BLE transports.
 *
 * Flow:
 *   Host → ESP32: OTA_BEGIN (size, sha256, version)
 *   ESP32 → Host: OTA_ACK (status=OK, nextOffset=0)
 *   Host → ESP32: OTA_DATA (offset=0, data[0..1023])
 *   ESP32 → Host: OTA_ACK (status=OK, nextOffset=1024)
 *   Host → ESP32: OTA_DATA (offset=1024, data[1024..2047])
 *   ...
 *   Host → ESP32: OTA_END
 *   ESP32 → Host: OTA_ACK (status=OK) → reboot
 *
 * On error:
 *   ESP32 → Host: OTA_ABORT (reason)
 */

#include "interfaces/result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace domes {

/// OTA chunk size (kMaxPayloadSize - sizeof(OtaDataHeader) = 1024 - 6 = 1018)
/// Rounded down to 1016 for 8-byte alignment
constexpr size_t kOtaChunkSize = 1016;

/// Maximum version string length (including null terminator)
constexpr size_t kOtaVersionMaxLen = 32;

/// SHA256 hash size in bytes
constexpr size_t kSha256Size = 32;

/**
 * @brief OTA message types
 */
enum class OtaMsgType : uint8_t {
    kBegin = 0x01,  ///< Start OTA transfer (host → device)
    kData = 0x02,   ///< Firmware data chunk (host → device)
    kEnd = 0x03,    ///< Transfer complete (host → device)
    kAck = 0x04,    ///< Acknowledge (device → host)
    kAbort = 0x05,  ///< Abort transfer (either direction)
};

// =============================================================================
// Message Structures (packed for wire transmission)
// =============================================================================

#pragma pack(push, 1)

/**
 * @brief OTA_BEGIN message payload
 *
 * Sent by host to start OTA transfer.
 */
struct OtaBeginPayload {
    uint32_t firmwareSize;                        ///< Total firmware size in bytes
    std::array<uint8_t, kSha256Size> sha256;      ///< Expected SHA256 hash
    std::array<char, kOtaVersionMaxLen> version;  ///< Version string (null-terminated)
};
static_assert(sizeof(OtaBeginPayload) == 4 + 32 + 32, "OtaBeginPayload size mismatch");

/**
 * @brief OTA_DATA message payload
 *
 * Sent by host with firmware chunk.
 */
struct OtaDataHeader {
    uint32_t offset;  ///< Byte offset in firmware image
    uint16_t length;  ///< Length of data in this chunk
    // Followed by: uint8_t data[length]
};
static_assert(sizeof(OtaDataHeader) == 6, "OtaDataHeader size mismatch");

/**
 * @brief OTA_ACK message payload
 *
 * Sent by device to acknowledge a message.
 */
struct OtaAckPayload {
    uint8_t status;       ///< OtaStatus value
    uint32_t nextOffset;  ///< Next expected offset (for flow control)
};
static_assert(sizeof(OtaAckPayload) == 5, "OtaAckPayload size mismatch");

/**
 * @brief OTA_ABORT message payload
 *
 * Sent to abort the transfer with a reason.
 */
struct OtaAbortPayload {
    uint8_t reason;  ///< OtaStatus value indicating abort reason
};
static_assert(sizeof(OtaAbortPayload) == 1, "OtaAbortPayload size mismatch");

#pragma pack(pop)

// =============================================================================
// Serialization Functions
// =============================================================================

/**
 * @brief Serialize OTA_BEGIN message
 *
 * @param firmwareSize Total firmware size
 * @param sha256 SHA256 hash of firmware
 * @param version Version string
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @param outLen [out] Bytes written
 * @return TransportError::kOk on success
 */
TransportError serializeOtaBegin(uint32_t firmwareSize, const uint8_t* sha256, const char* version,
                                 uint8_t* buf, size_t bufSize, size_t* outLen);

/**
 * @brief Serialize OTA_DATA message
 *
 * @param offset Byte offset in firmware
 * @param data Chunk data
 * @param dataLen Chunk length
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @param outLen [out] Bytes written
 * @return TransportError::kOk on success
 */
TransportError serializeOtaData(uint32_t offset, const uint8_t* data, size_t dataLen, uint8_t* buf,
                                size_t bufSize, size_t* outLen);

/**
 * @brief Serialize OTA_END message
 *
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @param outLen [out] Bytes written (always 0 for OTA_END)
 * @return TransportError::kOk on success
 */
TransportError serializeOtaEnd(uint8_t* buf, size_t bufSize, size_t* outLen);

/**
 * @brief Serialize OTA_ACK message
 *
 * @param status Status code
 * @param nextOffset Next expected offset
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @param outLen [out] Bytes written
 * @return TransportError::kOk on success
 */
TransportError serializeOtaAck(OtaStatus status, uint32_t nextOffset, uint8_t* buf, size_t bufSize,
                               size_t* outLen);

/**
 * @brief Serialize OTA_ABORT message
 *
 * @param reason Abort reason
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @param outLen [out] Bytes written
 * @return TransportError::kOk on success
 */
TransportError serializeOtaAbort(OtaStatus reason, uint8_t* buf, size_t bufSize, size_t* outLen);

// =============================================================================
// Deserialization Functions
// =============================================================================

/**
 * @brief Deserialize OTA_BEGIN payload
 *
 * @param payload Raw payload bytes
 * @param payloadLen Payload length
 * @param firmwareSize [out] Firmware size
 * @param sha256 [out] SHA256 hash (32 bytes)
 * @param version [out] Version string buffer
 * @param versionBufSize Size of version buffer
 * @return TransportError::kOk on success
 */
TransportError deserializeOtaBegin(const uint8_t* payload, size_t payloadLen,
                                   uint32_t* firmwareSize, uint8_t* sha256, char* version,
                                   size_t versionBufSize);

/**
 * @brief Deserialize OTA_DATA header
 *
 * @param payload Raw payload bytes
 * @param payloadLen Payload length
 * @param offset [out] Byte offset
 * @param data [out] Pointer to chunk data within payload
 * @param dataLen [out] Chunk data length
 * @return TransportError::kOk on success
 */
TransportError deserializeOtaData(const uint8_t* payload, size_t payloadLen, uint32_t* offset,
                                  const uint8_t** data, size_t* dataLen);

/**
 * @brief Deserialize OTA_ACK payload
 *
 * @param payload Raw payload bytes
 * @param payloadLen Payload length
 * @param status [out] Status code
 * @param nextOffset [out] Next expected offset
 * @return TransportError::kOk on success
 */
TransportError deserializeOtaAck(const uint8_t* payload, size_t payloadLen, OtaStatus* status,
                                 uint32_t* nextOffset);

/**
 * @brief Deserialize OTA_ABORT payload
 *
 * @param payload Raw payload bytes
 * @param payloadLen Payload length
 * @param reason [out] Abort reason
 * @return TransportError::kOk on success
 */
TransportError deserializeOtaAbort(const uint8_t* payload, size_t payloadLen, OtaStatus* reason);

}  // namespace domes
