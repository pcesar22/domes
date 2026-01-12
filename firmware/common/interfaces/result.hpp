#pragma once

/**
 * @file result.hpp
 * @brief Platform-agnostic error types for transport layer
 *
 * Provides error types that work on both ESP32 (ESP-IDF) and Linux host.
 * Replaces esp_err_t for shared code.
 */

#include <cstdint>

namespace domes {

/**
 * @brief Transport operation error codes
 *
 * Platform-agnostic replacement for esp_err_t in shared code.
 */
enum class TransportError : int32_t {
    kOk = 0,               ///< Operation succeeded
    kTimeout = -1,         ///< Operation timed out
    kDisconnected = -2,    ///< Transport disconnected
    kInvalidArg = -3,      ///< Invalid argument
    kBufferFull = -4,      ///< Buffer full, try again later
    kBufferEmpty = -5,     ///< No data available
    kCrcMismatch = -6,     ///< CRC validation failed
    kProtocolError = -7,   ///< Protocol violation
    kNotInitialized = -8,  ///< Transport not initialized
    kAlreadyInit = -9,     ///< Transport already initialized
    kIoError = -10,        ///< Low-level I/O error
    kNoMemory = -11,       ///< Memory allocation failed
};

/**
 * @brief Check if error indicates success
 */
constexpr bool isOk(TransportError err) {
    return err == TransportError::kOk;
}

/**
 * @brief Convert TransportError to human-readable string
 */
constexpr const char* transportErrorToString(TransportError err) {
    switch (err) {
        case TransportError::kOk:
            return "OK";
        case TransportError::kTimeout:
            return "Timeout";
        case TransportError::kDisconnected:
            return "Disconnected";
        case TransportError::kInvalidArg:
            return "Invalid argument";
        case TransportError::kBufferFull:
            return "Buffer full";
        case TransportError::kBufferEmpty:
            return "Buffer empty";
        case TransportError::kCrcMismatch:
            return "CRC mismatch";
        case TransportError::kProtocolError:
            return "Protocol error";
        case TransportError::kNotInitialized:
            return "Not initialized";
        case TransportError::kAlreadyInit:
            return "Already initialized";
        case TransportError::kIoError:
            return "I/O error";
        case TransportError::kNoMemory:
            return "No memory";
        default:
            return "Unknown error";
    }
}

/**
 * @brief OTA-specific status codes for ACK messages
 */
enum class OtaStatus : uint8_t {
    kOk = 0,              ///< Success, ready for next chunk
    kBusy = 1,            ///< Busy, retry later
    kFlashError = 2,      ///< Flash write failed
    kVerifyFailed = 3,    ///< SHA256 verification failed
    kSizeMismatch = 4,    ///< Size doesn't match OTA_BEGIN
    kOffsetMismatch = 5,  ///< Unexpected chunk offset
    kVersionError = 6,    ///< Version parsing failed
    kPartitionError = 7,  ///< OTA partition error
    kAborted = 8,         ///< Transfer aborted by receiver
};

/**
 * @brief Convert OtaStatus to human-readable string
 */
constexpr const char* otaStatusToString(OtaStatus status) {
    switch (status) {
        case OtaStatus::kOk:
            return "OK";
        case OtaStatus::kBusy:
            return "Busy";
        case OtaStatus::kFlashError:
            return "Flash error";
        case OtaStatus::kVerifyFailed:
            return "Verification failed";
        case OtaStatus::kSizeMismatch:
            return "Size mismatch";
        case OtaStatus::kOffsetMismatch:
            return "Offset mismatch";
        case OtaStatus::kVersionError:
            return "Version error";
        case OtaStatus::kPartitionError:
            return "Partition error";
        case OtaStatus::kAborted:
            return "Aborted";
        default:
            return "Unknown status";
    }
}

}  // namespace domes
