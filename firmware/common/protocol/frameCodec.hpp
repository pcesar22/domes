#pragma once

/**
 * @file frameCodec.hpp
 * @brief Frame encoding/decoding for serial transport protocol
 *
 * Frame format:
 * ┌──────────┬──────────┬──────────┬──────────┬──────────┐
 * │ Start    │ Length   │ Type     │ Payload  │ CRC32    │
 * │ (2 bytes)│ (2 bytes)│ (1 byte) │ (N bytes)│ (4 bytes)│
 * │ 0xAA 0x55│ LE uint16│ uint8    │ variable │ LE uint32│
 * └──────────┴──────────┴──────────┴──────────┴──────────┘
 *
 * - Length: Size of (Type + Payload), NOT including start bytes or CRC
 * - CRC32: Calculated over (Type + Payload)
 * - Maximum payload: 1024 bytes (kMaxPayloadSize)
 * - Total frame overhead: 9 bytes (2 start + 2 length + 1 type + 4 CRC)
 */

#include "utils/crc32.hpp"
#include "interfaces/result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace domes {

/// Frame start bytes (magic number)
constexpr uint8_t kFrameStartByte0 = 0xAA;
constexpr uint8_t kFrameStartByte1 = 0x55;

/// Maximum payload size (excluding type byte)
constexpr size_t kMaxPayloadSize = 1024;

/// Frame header size (start bytes + length)
constexpr size_t kFrameHeaderSize = 4;

/// Frame overhead (header + type + CRC)
constexpr size_t kFrameOverhead = 9;

/// Maximum frame size
constexpr size_t kMaxFrameSize = kMaxPayloadSize + kFrameOverhead;

/**
 * @brief Encode a message into a frame
 *
 * @param type Message type byte
 * @param payload Payload data (can be nullptr if payloadLen is 0)
 * @param payloadLen Length of payload
 * @param frameBuf Output buffer for encoded frame
 * @param frameBufSize Size of output buffer
 * @param frameLen [out] Actual frame length written
 * @return TransportError::kOk on success
 * @return TransportError::kInvalidArg if buffer too small or payload too large
 */
inline TransportError encodeFrame(
    uint8_t type,
    const uint8_t* payload,
    size_t payloadLen,
    uint8_t* frameBuf,
    size_t frameBufSize,
    size_t* frameLen
) {
    if (payloadLen > kMaxPayloadSize) {
        return TransportError::kInvalidArg;
    }

    const size_t totalLen = kFrameOverhead + payloadLen;
    if (frameBufSize < totalLen) {
        return TransportError::kInvalidArg;
    }

    // Length field = type (1) + payload (N)
    const uint16_t lengthField = static_cast<uint16_t>(1 + payloadLen);

    // Start bytes
    frameBuf[0] = kFrameStartByte0;
    frameBuf[1] = kFrameStartByte1;

    // Length (little-endian)
    frameBuf[2] = static_cast<uint8_t>(lengthField & 0xFF);
    frameBuf[3] = static_cast<uint8_t>((lengthField >> 8) & 0xFF);

    // Type
    frameBuf[4] = type;

    // Payload
    if (payloadLen > 0 && payload != nullptr) {
        std::memcpy(&frameBuf[5], payload, payloadLen);
    }

    // CRC32 over (type + payload)
    uint32_t crc = crc32(&frameBuf[4], 1 + payloadLen);
    size_t crcOffset = 5 + payloadLen;
    frameBuf[crcOffset + 0] = static_cast<uint8_t>(crc & 0xFF);
    frameBuf[crcOffset + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    frameBuf[crcOffset + 2] = static_cast<uint8_t>((crc >> 16) & 0xFF);
    frameBuf[crcOffset + 3] = static_cast<uint8_t>((crc >> 24) & 0xFF);

    *frameLen = totalLen;
    return TransportError::kOk;
}

/**
 * @brief Frame decoder state machine
 *
 * Feed bytes one at a time via feedByte(). When a complete valid
 * frame is received, isComplete() returns true.
 *
 * @example
 * @code
 * FrameDecoder decoder;
 * while (transport.available()) {
 *     uint8_t byte;
 *     transport.read(&byte, 1);
 *     decoder.feedByte(byte);
 *     if (decoder.isComplete()) {
 *         processMessage(decoder.getType(), decoder.getPayload(), decoder.getPayloadLen());
 *         decoder.reset();
 *     }
 * }
 * @endcode
 */
class FrameDecoder {
public:
    enum class State : uint8_t {
        kWaitStart0,   ///< Waiting for first start byte (0xAA)
        kWaitStart1,   ///< Waiting for second start byte (0x55)
        kWaitLenLow,   ///< Waiting for length low byte
        kWaitLenHigh,  ///< Waiting for length high byte
        kReceiveData,  ///< Receiving type + payload
        kWaitCrc,      ///< Receiving CRC bytes
        kComplete,     ///< Frame complete and valid
        kError,        ///< Frame error (CRC mismatch or too large)
    };

    FrameDecoder() { reset(); }

    /**
     * @brief Reset decoder to initial state
     */
    void reset() {
        state_ = State::kWaitStart0;
        length_ = 0;
        dataIndex_ = 0;
        crcIndex_ = 0;
        receivedCrc_ = 0;
    }

    /**
     * @brief Feed a single byte to the decoder
     *
     * @param byte Received byte
     * @return Current state after processing
     */
    State feedByte(uint8_t byte) {
        switch (state_) {
            case State::kWaitStart0:
                if (byte == kFrameStartByte0) {
                    state_ = State::kWaitStart1;
                }
                break;

            case State::kWaitStart1:
                if (byte == kFrameStartByte1) {
                    state_ = State::kWaitLenLow;
                } else if (byte == kFrameStartByte0) {
                    // Stay in kWaitStart1 (could be 0xAA 0xAA 0x55)
                } else {
                    state_ = State::kWaitStart0;
                }
                break;

            case State::kWaitLenLow:
                length_ = byte;
                state_ = State::kWaitLenHigh;
                break;

            case State::kWaitLenHigh:
                length_ |= static_cast<uint16_t>(byte) << 8;
                if (length_ == 0 || length_ > kMaxPayloadSize + 1) {
                    // Invalid length (must be at least 1 for type)
                    state_ = State::kError;
                } else {
                    dataIndex_ = 0;
                    state_ = State::kReceiveData;
                }
                break;

            case State::kReceiveData:
                if (dataIndex_ < sizeof(data_)) {
                    data_[dataIndex_++] = byte;
                }
                if (dataIndex_ >= length_) {
                    crcIndex_ = 0;
                    receivedCrc_ = 0;
                    state_ = State::kWaitCrc;
                }
                break;

            case State::kWaitCrc:
                receivedCrc_ |= static_cast<uint32_t>(byte) << (crcIndex_ * 8);
                crcIndex_++;
                if (crcIndex_ >= 4) {
                    // Verify CRC
                    uint32_t calculatedCrc = crc32(data_.data(), length_);
                    if (calculatedCrc == receivedCrc_) {
                        state_ = State::kComplete;
                    } else {
                        state_ = State::kError;
                    }
                }
                break;

            case State::kComplete:
            case State::kError:
                // Must call reset() before feeding more bytes
                break;
        }

        return state_;
    }

    /// @return true if a complete valid frame was received
    bool isComplete() const { return state_ == State::kComplete; }

    /// @return true if a frame error occurred (CRC mismatch or invalid length)
    bool isError() const { return state_ == State::kError; }

    /// @return Current decoder state
    State getState() const { return state_; }

    /// @return Message type byte (valid only when isComplete())
    uint8_t getType() const { return data_[0]; }

    /// @return Pointer to payload data (valid only when isComplete())
    const uint8_t* getPayload() const { return &data_[1]; }

    /// @return Payload length in bytes (valid only when isComplete())
    size_t getPayloadLen() const { return length_ > 1 ? length_ - 1 : 0; }

    /// @return Raw data buffer (type + payload)
    const uint8_t* getData() const { return data_.data(); }

    /// @return Total data length (type + payload)
    size_t getDataLen() const { return length_; }

private:
    State state_;
    uint16_t length_;           ///< Length field from frame (type + payload)
    size_t dataIndex_;          ///< Current position in data buffer
    uint8_t crcIndex_;          ///< Current position in CRC (0-3)
    uint32_t receivedCrc_;      ///< CRC from frame
    std::array<uint8_t, kMaxPayloadSize + 1> data_;  ///< Type + payload buffer
};

}  // namespace domes
