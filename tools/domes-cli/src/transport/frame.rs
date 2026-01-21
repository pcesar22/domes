//! Frame codec for DOMES protocol
//!
//! Frame format: [0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
//! - Start bytes: 0xAA 0x55
//! - Length: 2 bytes little-endian, contains (Type + Payload) length
//! - Type: 1 byte message type
//! - Payload: 0-1024 bytes
//! - CRC32: 4 bytes little-endian, calculated over (Type + Payload)

use crc32fast::Hasher;
use thiserror::Error;

/// Frame start marker bytes
const START_BYTE_0: u8 = 0xAA;
const START_BYTE_1: u8 = 0x55;

/// Maximum payload size
pub const MAX_PAYLOAD_SIZE: usize = 1024;

/// Frame overhead: 2 start + 2 len + 1 type + 4 crc = 9 bytes
pub const FRAME_OVERHEAD: usize = 9;

/// Frame codec errors
#[derive(Debug, Error)]
pub enum FrameError {
    #[error("Payload too large: {0} > {MAX_PAYLOAD_SIZE}")]
    PayloadTooLarge(usize),

    #[error("Invalid length: {0}")]
    InvalidLength(u16),

    #[error("CRC mismatch: expected 0x{expected:08X}, got 0x{actual:08X}")]
    CrcMismatch { expected: u32, actual: u32 },
}

/// Encode a frame with the given type and payload
///
/// Returns the encoded frame as a Vec<u8>
pub fn encode_frame(msg_type: u8, payload: &[u8]) -> Result<Vec<u8>, FrameError> {
    if payload.len() > MAX_PAYLOAD_SIZE {
        return Err(FrameError::PayloadTooLarge(payload.len()));
    }

    // Length field = type (1 byte) + payload length
    let length = 1 + payload.len() as u16;

    // Calculate CRC over type + payload
    let mut hasher = Hasher::new();
    hasher.update(&[msg_type]);
    hasher.update(payload);
    let crc = hasher.finalize();

    // Build frame
    let frame_size = FRAME_OVERHEAD + payload.len();
    let mut frame = Vec::with_capacity(frame_size);

    // Start bytes
    frame.push(START_BYTE_0);
    frame.push(START_BYTE_1);

    // Length (little-endian)
    frame.extend_from_slice(&length.to_le_bytes());

    // Type
    frame.push(msg_type);

    // Payload
    frame.extend_from_slice(payload);

    // CRC32 (little-endian)
    frame.extend_from_slice(&crc.to_le_bytes());

    Ok(frame)
}

/// Decoded frame
#[derive(Debug, Clone)]
pub struct Frame {
    pub msg_type: u8,
    pub payload: Vec<u8>,
}

/// Frame decoder state machine
#[derive(Debug, Clone, Copy, PartialEq)]
enum DecoderState {
    WaitStart0,
    WaitStart1,
    WaitLenLow,
    WaitLenHigh,
    WaitType,
    WaitPayload,
    WaitCrc,
    Complete,
    Error,
}

/// Streaming frame decoder
pub struct FrameDecoder {
    state: DecoderState,
    length: u16,
    msg_type: u8,
    payload: Vec<u8>,
    crc_bytes: [u8; 4],
    crc_index: usize,
    payload_index: usize,
}

impl Default for FrameDecoder {
    fn default() -> Self {
        Self::new()
    }
}

impl FrameDecoder {
    /// Create a new frame decoder
    pub fn new() -> Self {
        Self {
            state: DecoderState::WaitStart0,
            length: 0,
            msg_type: 0,
            payload: Vec::new(),
            crc_bytes: [0; 4],
            crc_index: 0,
            payload_index: 0,
        }
    }

    /// Reset the decoder state
    pub fn reset(&mut self) {
        self.state = DecoderState::WaitStart0;
        self.length = 0;
        self.msg_type = 0;
        self.payload.clear();
        self.crc_bytes = [0; 4];
        self.crc_index = 0;
        self.payload_index = 0;
    }

    /// Feed a byte to the decoder
    ///
    /// Returns Some(Frame) when a complete frame is decoded, None otherwise
    pub fn feed_byte(&mut self, byte: u8) -> Option<Result<Frame, FrameError>> {
        match self.state {
            DecoderState::WaitStart0 => {
                if byte == START_BYTE_0 {
                    self.state = DecoderState::WaitStart1;
                }
                // Ignore non-start bytes (noise resilience)
                None
            }
            DecoderState::WaitStart1 => {
                if byte == START_BYTE_1 {
                    self.state = DecoderState::WaitLenLow;
                } else if byte == START_BYTE_0 {
                    // Stay in WaitStart1 (might be repeated start byte)
                } else {
                    self.state = DecoderState::WaitStart0;
                }
                None
            }
            DecoderState::WaitLenLow => {
                self.length = byte as u16;
                self.state = DecoderState::WaitLenHigh;
                None
            }
            DecoderState::WaitLenHigh => {
                self.length |= (byte as u16) << 8;

                // Validate length
                if self.length == 0 || self.length > (MAX_PAYLOAD_SIZE + 1) as u16 {
                    self.state = DecoderState::Error;
                    return Some(Err(FrameError::InvalidLength(self.length)));
                }

                self.state = DecoderState::WaitType;
                None
            }
            DecoderState::WaitType => {
                self.msg_type = byte;

                // Payload length = total length - type byte
                let payload_len = (self.length - 1) as usize;
                self.payload = Vec::with_capacity(payload_len);
                self.payload_index = 0;

                if payload_len == 0 {
                    self.state = DecoderState::WaitCrc;
                    self.crc_index = 0;
                } else {
                    self.state = DecoderState::WaitPayload;
                }
                None
            }
            DecoderState::WaitPayload => {
                self.payload.push(byte);
                self.payload_index += 1;

                let payload_len = (self.length - 1) as usize;
                if self.payload_index >= payload_len {
                    self.state = DecoderState::WaitCrc;
                    self.crc_index = 0;
                }
                None
            }
            DecoderState::WaitCrc => {
                self.crc_bytes[self.crc_index] = byte;
                self.crc_index += 1;

                if self.crc_index >= 4 {
                    self.state = DecoderState::Complete;

                    // Verify CRC
                    let received_crc = u32::from_le_bytes(self.crc_bytes);

                    let mut hasher = Hasher::new();
                    hasher.update(&[self.msg_type]);
                    hasher.update(&self.payload);
                    let calculated_crc = hasher.finalize();

                    if received_crc != calculated_crc {
                        return Some(Err(FrameError::CrcMismatch {
                            expected: calculated_crc,
                            actual: received_crc,
                        }));
                    }

                    Some(Ok(Frame {
                        msg_type: self.msg_type,
                        payload: std::mem::take(&mut self.payload),
                    }))
                } else {
                    None
                }
            }
            DecoderState::Complete | DecoderState::Error => {
                // Should call reset() before feeding more bytes
                None
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_empty_payload() {
        let frame = encode_frame(0x20, &[]).unwrap();
        assert_eq!(frame[0], START_BYTE_0);
        assert_eq!(frame[1], START_BYTE_1);
        assert_eq!(frame[2], 1); // Length low byte (just type)
        assert_eq!(frame[3], 0); // Length high byte
        assert_eq!(frame[4], 0x20); // Type
        // CRC follows
        assert_eq!(frame.len(), FRAME_OVERHEAD);
    }

    #[test]
    fn test_encode_decode_roundtrip() {
        let payload = [0x01, 0x02, 0x03, 0x04];
        let frame = encode_frame(0x21, &payload).unwrap();

        let mut decoder = FrameDecoder::new();
        let mut result = None;

        for byte in frame {
            if let Some(r) = decoder.feed_byte(byte) {
                result = Some(r);
            }
        }

        let decoded = result.unwrap().unwrap();
        assert_eq!(decoded.msg_type, 0x21);
        assert_eq!(decoded.payload, payload);
    }

    #[test]
    fn test_crc_mismatch() {
        let mut frame = encode_frame(0x20, &[0x01]).unwrap();
        // Corrupt the CRC
        let len = frame.len();
        frame[len - 1] ^= 0xFF;

        let mut decoder = FrameDecoder::new();
        let mut result = None;

        for byte in frame {
            if let Some(r) = decoder.feed_byte(byte) {
                result = Some(r);
            }
        }

        assert!(matches!(result.unwrap(), Err(FrameError::CrcMismatch { .. })));
    }

    #[test]
    fn test_noise_resilience() {
        let frame = encode_frame(0x20, &[]).unwrap();

        let mut decoder = FrameDecoder::new();

        // Feed garbage before frame
        decoder.feed_byte(0x00);
        decoder.feed_byte(0xFF);
        decoder.feed_byte(0x12);

        // Feed frame
        let mut result = None;
        for byte in frame {
            if let Some(r) = decoder.feed_byte(byte) {
                result = Some(r);
            }
        }

        assert!(result.unwrap().is_ok());
    }
}
